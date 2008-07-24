#ifndef TORRENTPEER_H_INCLUDED
#define TORRENTPEER_H_INCLUDED

#include <string>
#include <vector>
#include <queue>
#include <deque>
#include <iostream>

#include "Socket.h"
#include "TorrentSeeder.h"

enum MessageType {
    choke           = 0,
    unchoke         = 1,
    interested      = 2,
    not_interested  = 3,
    have            = 4,
    bitfield        = 5,
    request         = 6,
    piece           = 7,
    cancel          = 8
};

struct Request
{
    unsigned piece, begin, length;
};

class TorrentPeer : public Socket
{
protected:
    TorrentSeeder &server;
    const MetaInfo *info;

    ByteBuffer input;
    unsigned input_pos;
    std::queue<ByteBuffer> output;
    unsigned output_pos;

    enum { handshake, identifier, length, message } waiting_for;

    std::deque<Request> requests;
    ByteBuffer piece_data;
    unsigned piece_index;

    void onReadable();
    void onWritable();
    void onException();

    void processInput();
    void processMessage(const ByteBuffer &message);

    void queueOutput(const ByteBuffer &data);
    inline void queueMessage(MessageType type);

    void destroy();

    std::ostream& _info();

public:
    TorrentPeer(TorrentSeeder &server, int fd);
    ~TorrentPeer();

    inline bool hasRequests() { return !requests.empty(); }
    unsigned queueRequest();
};

#endif /* ndef TORRENTPEER_H_INCLUDED */
