#include "Socket.h"
#ifdef __MINGW32__
#include <winsock.h>
#define close(fd) closesocket(fd)
#else
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#endif
#include <vector>

static std::vector<Socket*> sockets;

void SocketSet::addSocket(Socket &socket)
{
    if(socket.fd >= 0)
    {
        if(int(sockets.size()) <= socket.fd)
            sockets.resize(socket.fd + 1);
        sockets[socket.fd] = &socket;
    }
}

void SocketSet::removeSocket(Socket &socket)
{
    if(socket.fd >= 0 && socket.fd < int(sockets.size()))
    {
        sockets[socket.fd] = NULL;
    }
}

bool SocketSet::process(int timeout_ms)
{
    struct timeval timeout;
    timeout.tv_sec  = timeout_ms/1000;
    timeout.tv_usec = 1000*(timeout_ms%1000);

    fd_set readfds, writefds, exceptfds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);

    int nfds = 0;

    for(int n = 0; n < int(sockets.size()); ++n)
        if(sockets[n])
        {
            if(sockets[n]->mask & readable)
                FD_SET(n, &readfds);
            if(sockets[n]->mask & writable)
                FD_SET(n, &writefds);
            if(sockets[n]->mask & exception)
                FD_SET(n, &exceptfds);
            nfds = n + 1;
        }

    if(nfds)
    {
        if(select(nfds, &readfds, &writefds, &exceptfds, &timeout) < 0)
            return false;

        for(int n = 0; n < int(sockets.size()); ++n)
        {
            if(FD_ISSET(n, &readfds) && sockets[n])
                sockets[n]->onReadable();
            if(FD_ISSET(n, &writefds) && sockets[n])
                sockets[n]->onWritable();
            if(FD_ISSET(n, &exceptfds) && sockets[n])
                sockets[n]->onException();
        }
    }
    
    return true;
}


Socket::Socket(SocketSet &set, int fd, int mask) : fd(fd), mask(mask), m_set(set)
{
    m_set.addSocket(*this);
}

Socket::~Socket()
{
    m_set.removeSocket(*this);
    close(fd);
}

void onReadable()
{
}

void onWrititable()
{
}

void onException()
{
}

