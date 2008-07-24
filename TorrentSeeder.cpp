#include "TorrentSeeder.h"
#include "TorrentPeer.h"
#include "settings.h"
#include <sys/types.h>
#ifdef __MINGW32__
#include <winsock.h>
typedef int socklen_t;
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif
#include <unistd.h>
#include <cstdio>
#include <algorithm>
#include <fstream>

TorrentSeeder::TorrentSeeder(SocketSet &set, int fd)
    : Socket(set, fd, readable)
{
    sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    getsockname(fd, (sockaddr*)&addr, &addr_len);
    m_port = ntohs(addr.sin_port);
}

TorrentSeeder::~TorrentSeeder()
{
    for(MetaInfoMap::iterator i = metainfo.begin(); i != metainfo.end(); ++i)
        i->second->release();
}

void TorrentSeeder::onReadable()
{
    int newfd = accept(fd, NULL, NULL);
    if(newfd < 0)
        perror("accept failed");
    else
        peers.push_back(new TorrentPeer(*this, newfd));
}

TorrentSeeder* TorrentSeeder::create(SocketSet &set)
{
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    if(fd < 0)
        return NULL;

    // Try to bind to port in range
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = 0;
    for(int port = cfg_seeder_port_min; port <= cfg_seeder_port_max; ++port)
    {
        addr.sin_port = htons(port);
        if(bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0)
            break;
    }
    if(addr.sin_port == htons(cfg_seeder_port_max + 1))
    {
        // No ports available :/
        close(fd);
        return NULL;
    }

    if(listen(fd, 8) < 0)
    {
        close(fd);
        return NULL;
    }

    return new TorrentSeeder(set, fd);
}

const std::string &TorrentSeeder::id()
{
    static std::string id = "-MV1000-000000000000";
    return id;
}

const MetaInfo *TorrentSeeder::getMetaInfo(const std::string &infohash) const
{
    MetaInfoMap::const_iterator i = metainfo.find(infohash);
    if(i == metainfo.end())
        return NULL;
    return i->second;
}

bool TorrentSeeder::hasMetaInfo(const std::string &infohash) const
{
    return metainfo.find(infohash) != metainfo.end();
}

unsigned TorrentSeeder::ip() const
{
    char name[1024];
    if(gethostname(name, sizeof(name)) != 0)
        return 0;
    struct hostent *he = gethostbyname(name);
    if(he == NULL)
        return 0;
    return ((struct in_addr *)he->h_addr_list[0])->s_addr;
}

void TorrentSeeder::removePeer(TorrentPeer &peer)
{
    peers.erase(std::find(peers.begin(), peers.end(), &peer));
}

unsigned TorrentSeeder::queueUploadData(unsigned target)
{
    unsigned queued = 0;
    int n = peers.size();
    while(n-- && queued < target)
    {
        TorrentPeer *peer = peers.front();
        peers.pop_front();
        while(peer->hasRequests() && queued < target)
            queued += peer->queueRequest();
        peers.push_back(peer);
    }
    return queued;
}

// NOTE: takes ownership of info!
void TorrentSeeder::addTorrent(MetaInfo *info)
{
    MetaInfo * &ptr = metainfo[info->infohash()];
    if(ptr != NULL)
        ptr->release();
    ptr = info;
}

void TorrentSeeder::removeTorrent(MetaInfo *info)
{
    MetaInfoMap::iterator i = metainfo.find(info->infohash());
    if(i != metainfo.end())
    {
        i->second->release();
        metainfo.erase(i);
    }
}
