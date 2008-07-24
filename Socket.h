#ifndef SOCKET_H_INCLUDED
#define SOCKET_H_INCLUDED

#include <vector>

enum SelectMask { readable = 1, writable = 2, exception = 4 };

class Socket;

class SocketSet
{
    std::vector<Socket*> sockets;

public:
    void addSocket(Socket &socket);
    void removeSocket(Socket &socket);
    bool process(int timeout_ms = 50);
};

class Socket
{
    friend class SocketSet;

protected:
    int fd, mask;
    SocketSet &m_set;

    virtual void onReadable() { };
    virtual void onWritable() { };
    virtual void onException() { };

public:
    Socket(SocketSet &set, int fd, int mask = readable|writable|exception);
    virtual ~Socket();

    inline int file() { return fd; }
    inline SocketSet &set() { return m_set; }
};

#endif /* ndef SOCKET_H_INCLUDED */
