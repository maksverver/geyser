#ifndef HTTPREQUEST_H_INCLUDED
#define HTTPREQUEST_H_INCLUDED

#include "Socket.h"
#include <vector>
#include <iostream>
#include <map>

typedef std::multimap<std::string, std::string> QueryVarMap;
QueryVarMap parseQueryString(const std::string query);

struct HttpRequest
{
    std::string method, location, query;
    std::vector<std::string> headers;
    std::string body;
};

class HttpRequestHandler : public Socket
{
    std::string input, output;
    std::string::size_type output_pos;

    virtual void handleRequest(std::ostream &os, HttpRequest &request) = 0;

public:
    HttpRequestHandler(SocketSet &set, int fd);
    ~HttpRequestHandler();

    void onReadable();
    void onWritable();
};


#endif /* ndef HTTPREQUEST_H_INCLUDED */
