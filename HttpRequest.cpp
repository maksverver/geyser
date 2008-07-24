#include "HttpRequest.h"
#include <sstream>
#ifdef __MINGW32__
#include <winsock.h>
#define read(fd,buf,len) recv(fd,buf,len,0)
#define write(fd,buf,len) send(fd,buf,len,0)
#else
#include <unistd.h>
#endif

static const int maxRequestSize = 4096;

int decode_hexdigit(char c)
{
    static const char digits[] = "00112233445566778899aAbBcCdDeEfF";
    const char *d = std::find(digits, digits + 32, c);
    return (d == digits + 32) ? -1000 : (d - digits)/2;
}

std::string urldecode(const char *first, const char *last)
{
    std::string result;
    result.reserve(last - first);
    while(first != last)
    {
        if(*first == '%')
        {
            if(last - first < 3)
                break;
            int c = 16*decode_hexdigit(first[1]) + decode_hexdigit(first[2]);
            if(c >= 0)
                result += char(c);
            first += 3;
        }
        else
        {
            result += *first;
            ++first;
        }
    }
    return result;
}

QueryVarMap parseQueryString(const std::string query)
{
    QueryVarMap vars;
    std::string::size_type pos, next = (std::string::size_type)-1;
    const char *data = query.data();
    do {
        pos  = next + 1;
        next = std::min(query.find('&', pos), query.size());

        std::string::size_type sep = query.find('=', pos);
        if(sep < next)
        {
            vars.insert( std::make_pair(
                urldecode(data + pos, data + sep),
                urldecode(data + sep + 1, data + next) ) );
        }
    } while(next < query.size());
    return vars;
}

HttpRequestHandler::HttpRequestHandler(SocketSet &set, int fd)
    : Socket(set, fd, readable), output_pos(0)
{
}

HttpRequestHandler::~HttpRequestHandler()
{
}

bool parseRequestLine(HttpRequest &request, std::string line)
{
    std::string uri, protocol;
    std::istringstream iss(line);
    iss >> request.method >> uri >> protocol;
    if(protocol.find("HTTP/") != 0)
        return false;

    std::string::size_type sep = uri.find('?');
    if(sep == std::string::npos)
    {
        request.location = uri;
    }
    else
    {
        request.location.assign(uri.begin(), uri.begin() + sep);
        request.query.assign(uri.begin() + sep + 1, uri.end());
    }
    return true;
}

void HttpRequestHandler::onReadable()
{
    char buffer[2048];
    ssize_t bytes = read(fd, buffer, sizeof(buffer));

    if(bytes <= 0)
    {
        // End-of-stream reached; or error (if bytes < 0)
        delete this;
        return;
    }

    if(int(input.size()) + bytes > maxRequestSize)
    {
        // Request size exceeded
        delete this;
        return;
    }

    input.append(buffer, bytes);
    std::string::size_type end = input.find("\r\n\r\n");
    if(end != std::string::npos)
    {
        HttpRequest request;
        bool first = true;
        std::string::size_type pos, sep = (std::string::size_type)-2;
        do {
            pos = sep + 2;
            sep = input.find("\r\n", pos);
            std::string line(input.begin() + pos, input.begin() + sep);
            if(first)
            {
                if(!parseRequestLine(request, line))
                {
                    delete this;
                    return;
                }
                first = false;
            }
            else
            {
                request.headers.push_back(line);
            }
        } while(sep != end);

        // Handle request
        std::ostringstream oss;
        handleRequest(oss, request);
        output = oss.str();
        mask = writable;

        // DEBUG
        /*
        std::cout << "Received HTTP request: " << request.method << ' ' << request.location
                  << '?' << request.query << "\n"
                  << "Sending HTTP reply: " << output << "\n----" << std::endl;
        */
    }
}

void HttpRequestHandler::onWritable()
{
    if(output_pos < output.size())
    {
        ssize_t bytes = write( fd, output.data() + output_pos,
                                output.size() - output_pos );
        if(bytes < 0)
        {
            // Write error!
#ifdef DEBUG
            perror("write");
#endif
            delete this;
        }
        else
        {
            output_pos += bytes;
        }
    }

    if(output_pos >= output.size())
    {
        delete this;
    }
}
