#include "TorrentPeer.h"
#ifdef __MINGW32__
#include <winsock.h>
#define read(fd,buf,len) recv(fd,buf,len,0)
#define write(fd,buf,len) send(fd,buf,len,0)
#else
#include <unistd.h>
#endif
#include <cstdio>
#include <algorithm>

#ifdef DEBUG
#define INFO(x) (_info() << x << std::endl)
#else
#define INFO(x) ;
#endif

inline bool operator== (const Request &r, const Request &s)
{
    return r.piece == s.piece &&
           r.begin == s.begin &&
           r.length == s.length;
}

inline bool operator< (const Request &r, const Request &s)
{
    return ( r.piece < s.piece || r.piece == s.piece &&
           ( r.begin < s.begin || r.begin == s.begin &&
           ( r.length <= s.length ) ) );
}


// Maximum number of requests a peer may have in queue
static const int max_requests = 1000;

TorrentPeer::TorrentPeer(TorrentSeeder &server, int fd)
    : Socket(server.set(), fd, readable|exception), server(server),
    info(0), input_pos(0), output_pos(0), waiting_for(handshake),
    piece_index(unsigned(-1))
{
    // Prepare to receive 48-byte handshake
    input.resize(20 + 8 + 20);

    INFO("constructed");
}

TorrentPeer::~TorrentPeer()
{
    INFO("destructed");
    info->release();
    server.removePeer(*this);
}

void TorrentPeer::destroy()
{
    delete this;
}

void TorrentPeer::onReadable()
{
    INFO("readable");

    ssize_t bytes = read(fd, &input[input_pos], input.size() - input_pos);

    if(bytes > 0)
    {
        // DEBUG
        //hexdump(std::cout, &input[input_pos], bytes) << std::endl;

        input_pos += bytes;
        if(input_pos >= input.size())
        {
            processInput();
            input_pos = 0;
        }
    }
    else
    {
        if(bytes < 0)
            INFO("read error");
        destroy();
    }
}

void TorrentPeer::onWritable()
{
    INFO("writable");

    if(!output.empty())
    {
        ssize_t bytes = write( fd, &output.front()[output_pos],
                                   output.front().size() - output_pos );

        if(bytes < 0)
        {
            // Write error
            perror("write");
            destroy();
        }
        else
        {
            // DEBUG
            //hexdump(std::cout, &output.front()[output_pos], bytes) << std::endl;

            output_pos += bytes;
            if(output_pos >= output.front().size())
            {
                output.pop();
                output_pos = 0;
            }
        }
    }

    if(output.empty())
        mask &= ~writable;
}

void TorrentPeer::onException()
{
    INFO("exception");
    destroy();
}

std::ostream& TorrentPeer::_info()
{
    return std::cerr << "TorrentPeer(" << fd << "): ";
}

inline static unsigned parse_int(const ByteBuffer &buffer, int pos)
{
    unsigned char *b = (unsigned char*)&buffer[pos];
    return (b[0] << 24) | (b[1] << 16) | (b[2] <<  8) | (b[3]);
}

inline void append_int(ByteBuffer &buffer, int i)
{
    size_t pos = buffer.size();
    buffer.resize(pos + 4);
    buffer[pos + 0] = Byte(i>>24);
    buffer[pos + 1] = Byte(i>>16);
    buffer[pos + 2] = Byte(i>> 8);
    buffer[pos + 3] = Byte(i>> 0);
}

void TorrentPeer::processInput()
{
    switch(waiting_for)
    {
    case handshake:
        {
            const char *bittorrent = "\23BitTorrent protocol";
            std::string protocol(&input[0], 20);
            if(protocol != bittorrent)
            {
                INFO("invalid protocol id: " << protocol);
                destroy();
                return;
            }

            std::string infohash(&input[28], 20);
            info = server.getMetaInfo(infohash);
            if(info == NULL)
            {
                INFO("unknow infohash");
                destroy();
                return;
            }
            info->acquire();

            ByteBuffer data;
            // Protocol version
            data.insert(data.end(), bittorrent, bittorrent + 20);
            // 8 reserved bytes
            data.insert(data.end(), 8, 0);
            // Add metainfo hash
            data.insert(data.end(), infohash.data(), infohash.data() + 20);
            // Send local peer id
            data.insert(data.end(), server.id().data(), server.id().data() + 20);
            queueOutput(data);

            // Add 'bitfield' message
            data.clear();
            int pieces = info->pieces();
            append_int(data, 1 + pieces/8 + (pieces%8 == 0 ? 0 : 1));
            queueOutput(data);
            data.clear();
            data.push_back(bitfield);
            data.insert(data.end(), pieces/8, 255);
            if(pieces%8)
                data.push_back(~char((1<<(8 - pieces%8))-1));
            queueOutput(data);

            // Add 'unchoke' message
            queueMessage(unchoke);

            // Set output buffer!

            // Set input buffer
            input.resize(20);
            waiting_for = identifier;

            INFO("handshake succesful");
        } break;

    case identifier:
        {
            INFO("peer id received");
            input.resize(4);
            waiting_for = length;
        } break;

    case length:
        {
            unsigned length = parse_int(input, 0);
            if(length > (1<<18))
            {
                INFO("message of length " << length << " too large");
                destroy();
                return;
            }
            if(length == 0)
            {
                // Empty message
                processMessage(ByteBuffer());
            }
            else
            {
                input.resize(length);
                waiting_for = message;
            }
        } break;

    case message:
        {
            processMessage(input);
            input.resize(4);
            waiting_for = length;
        } break;
    }
}

void TorrentPeer::processMessage(const ByteBuffer &message)
{
    if(message.size() == 0)
    {
        INFO("Keep-alive received");
        return;
    }

    INFO("Message received: " << int(message[0]) << " (" << message.size() - 1 << " additional bytes)");

    switch(message[0])
    {
    case request:
        if(message.size() == 13)
        {
            Request r = {
                parse_int(message, 1),      // piece
                parse_int(message, 5),      // begin
                parse_int(message, 9) };    // length
            if(info->validRequest(r.piece, r.begin, r.length))
            {
                INFO("Received request for piece " << r.piece << " [" << r.begin << ',' << r.begin + r.length << ")");
                while(int(requests.size()) >= max_requests)
                    requests.pop_front();
                requests.push_back(r);
            }
        }
        break;

    case cancel:
        if(message.size() == 13)
        {
            Request r = {
                parse_int(message, 1),      // piece
                parse_int(message, 5),      // begin
                parse_int(message, 9) };    // length
            std::deque<Request>::iterator i = std::find(requests.begin(), requests.end(), r);
            if(i != requests.end())
            {
                INFO("Canceled request for piece " << r.piece << " [" << r.begin << ',' << r.begin + r.length << ")");
                requests.erase(i);
            }
        }
        break;
    }
}

unsigned TorrentPeer::queueRequest()
{
    if(requests.empty())
        return 0;

    const Request &r = requests.front();
    if(r.piece != piece_index)
    {
        if(!info->fetchPiece(r.piece, piece_data))
        {
            INFO("Unable to fetch piece " << r.piece << "!");
            requests.pop_front();
            return 0;
        }
        piece_index = r.piece;
    }

    if(r.begin + r.length > piece_data.size())
    {
        INFO( "Unable to satisfy request for short piece "
              << r.piece << " of length " << piece_data.size() );
        requests.pop_front();
        return 0;
    }

    INFO("Queueing data for piece " << r.piece << " [" << r.begin << ',' << r.begin + r.length << ")");

    ByteBuffer data;
    data.reserve(4 + 1 + 8 + r.length);
    append_int(data, 1 + 8 + r.length);
    data.push_back(::piece);
    append_int(data, r.piece);
    append_int(data, r.begin);
    data.insert(data.end(), &piece_data[r.begin], &piece_data[r.begin + r.length]);
    queueOutput(data);
    requests.pop_front();
    return r.length;
}

void TorrentPeer::queueOutput(const ByteBuffer &data)
{
    output.push(data);
    mask |= writable;
}

void TorrentPeer::queueMessage(MessageType type)
{
    ByteBuffer data;
    data.reserve(5);
    append_int(data, 1);
    data.push_back(type);
    queueOutput(data);
}
