#include "TorrentTracker.h"
#include "HttpRequest.h"
#include "bcoding.h"
#include "settings.h"
#include <sys/types.h>
#ifdef __MINGW32__
#include <winsock.h>
typedef int socklen_t;
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif
#include <unistd.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <cstring>

class TrackerRequestHandler : public HttpRequestHandler
{
    TorrentTracker &tracker;
    std::string input;
    unsigned ip;

    void handleRequest(std::ostream &os, HttpRequest &request);
    void handleAnnounceRequest(std::ostream &os, HttpRequest &request);
    void handleScrapeRequest(std::ostream &os, HttpRequest &request);

public:
    TrackerRequestHandler (TorrentTracker &tracker, int fd, unsigned ip);

};

TrackerRequestHandler::TrackerRequestHandler(TorrentTracker &tracker, int fd, unsigned ip)
    : HttpRequestHandler(tracker.set(), fd), tracker(tracker), ip(ip)
{
}

bool parseInt(const std::string &str, long long &result)
{
    if(str.empty())
        return false;
    bool neg = false;

    std::string::const_iterator i = str.begin();
    if(*i == '-')
        neg = true, ++i;
    else
    if(*i == '+')
        ++i;
    if(i == str.end())
        return false;

    result = 0;
    while(i != str.end())
    {
        if(*i < '0' || *i > '9')
            return false;
        result = 10*result + (*i - '0');
        ++i;
    }
    result = neg ? -result : +result;
    return true;
}

const char *ipToString(unsigned ip)
{
    struct in_addr addr;
    addr.s_addr = ip;
    return inet_ntoa(addr); // FIXME: not thread safe!
}

void TrackerRequestHandler::handleRequest(std::ostream &os, HttpRequest &request)
{
    if(request.location == "/announce")
        handleAnnounceRequest(os, request);
    else
    if(request.location == "/scrape")
        handleScrapeRequest(os, request);
    else
        os << "HTTP/1.0 404 Not Found\r\n\r\nResource not found.\r\n";
}

void TrackerRequestHandler::handleAnnounceRequest(std::ostream &os, HttpRequest &request)
{
    QueryVarMap vars = parseQueryString(request.query);
    QueryVarMap::const_iterator i;
    long long numWant = 50;
    if( (i = vars.find("info_hash")) == vars.end() || i->second.size() != 20 ||
        (i = vars.find("peer_id")) == vars.end() || i->second.size() != 20 ||
        ( (i = vars.find("numwant")) != vars.end() &&
          (!parseInt(i->second, numWant) || numWant < 0) ) )
    {
        os << "HTTP/1.0 400 Bad Request\r\n\r\nBad Request\r\n";
        return;
    }

    long long port;
    if( (i = vars.find("port")) == vars.end() || !parseInt(i->second, port) ||
        port < 1 || port > 65535 )
        port = 0;

    if( tracker.torrent_peers.find(vars.find("info_hash")->second)
        == tracker.torrent_peers.end() )
    {
        os << "HTTP/1.0 403 Forbidden\r\n\r\nForbidden\r\n";
        return;
    }

    std::string event;
    if((i = vars.find("event")) != vars.end())
        event = i->second;

    // Construct peer info
    PeerInfo peer;
    memcpy(peer.info_hash, vars.find("info_hash")->second.data(), 20);
    memcpy(peer.peer_id, vars.find("peer_id")->second.data(), 20);
    peer.ip         = ip;
    peer.port       = port;
    if( (i = vars.find("uploaded")) == vars.end() ||
        !parseInt(i->second, peer.uploaded) )
        peer.uploaded = 0;
    if( (i = vars.find("downloaded")) == vars.end() ||
        !parseInt(i->second, peer.downloaded) )
        peer.downloaded = 0;
    if( (i = vars.find("left")) == vars.end() ||
        !parseInt(i->second, peer.left) )
        peer.left = 0;

    // Store peer state
    tracker.store(peer, port == 0 || event == "stopped");

    // Get list of peers to display
    std::vector<const PeerInfo*> peers = tracker.list(peer.peer_id, peer.info_hash, numWant);

    Value reply;
    Dict &replyDict = reply.makeDict();
    replyDict["interval"].assign(tracker.update_interval);
    if((i = vars.find("compact")) != vars.end() && i->second == "1")
    {
        // Compact listing
        std::string &peersString = replyDict["peers"].makeString();
        peersString.reserve(6*peers.size());
        for(size_t n = 0; n < peers.size(); ++n)
        {
            char data[6] = {
                ((char*)&(peers[n]->ip))[0],
                ((char*)&(peers[n]->ip))[1],
                ((char*)&(peers[n]->ip))[2],
                ((char*)&(peers[n]->ip))[3],
                ((peers[n]->port)>>8)&255,
                (peers[n]->port)&255 };
            peersString.append(data, 6);
        }
    }
    else
    {
        // Full listing
        List &peersList = replyDict["peers"].makeList();
        peersList.resize(peers.size());
        for(size_t n = 0; n < peers.size(); ++n)
        {
            Dict &peerDict = peersList[n].makeDict();
            peerDict["peer id"].assign(peers[n]->peer_id, 20);
            peerDict["ip"].assign(ipToString(peers[n]->ip));
            peerDict["port"].assign(peers[n]->port);
        }
    }

    os << "HTTP/1.0 200 OK\r\nContent-type: text/plain\r\n\r\n";
    bencode(os, reply); 
}

void TrackerRequestHandler::handleScrapeRequest(std::ostream &os, HttpRequest &request)
{
    Value reply;
    Dict &filesDict = reply.makeDict()["files"].makeDict();

    QueryVarMap vars = parseQueryString(request.query);
    QueryVarMap::const_iterator i = vars.find("info_hash");

    for( TorrentTracker::TorrentPeerInfoMap::const_iterator j =
            tracker.torrent_peers.begin(); j != tracker.torrent_peers.end(); ++j )
    {
        if(i != vars.end() && i->second != j->first)
            continue;

        long long complete = 0, incomplete = 0;
        for( TorrentTracker::PeerInfoList::const_iterator k =
                j->second.begin(); k != j->second.end(); ++k )
        {
            if(k->left == 0)
                ++complete;
            if(k->left > 0)
                ++incomplete;
        }

        Dict &dict = filesDict[j->first].makeDict();
        dict["complete"].assign(complete);
        dict["incomplete"].assign(incomplete);
        dict["downloaded"].assign(0ll);   // TODO
        const MetaInfo *info = tracker.seeder.getMetaInfo(j->first);
        if(info)
            dict["name"].assign(info->name());
    }

    os << "HTTP/1.0 200 OK\r\nContent-type: text/plain\r\n\r\n";
    bencode(os, reply); 
}

TorrentTracker::TorrentTracker(int fd, TorrentSeeder &seeder, unsigned port)
    : Socket(seeder.set(), fd, readable), port(port), seeder(seeder),
      update_interval(cfg_tracker_rerequest_interval),
      purge_interval(cfg_tracker_purge_interval),
      max_peers_per_torrent(cfg_tracker_max_peers_per_torrent)
{
    std::memcpy(local_peer.peer_id, seeder.id().data(), 20);
    local_peer.ip   = seeder.ip();
    local_peer.port = seeder.port();
}

TorrentTracker::~TorrentTracker()
{
}


void TorrentTracker::onReadable()
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int newfd = accept(fd, (struct sockaddr*)&addr, &len);
    if(newfd >= 0 && len == sizeof(addr))
        new TrackerRequestHandler(*this, newfd, addr.sin_addr.s_addr);
}

TorrentTracker* TorrentTracker::create(TorrentSeeder &seeder, unsigned short port)
{
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    if(fd < 0)
        return NULL;

    // Try to bind to port
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = 0;
    addr.sin_port = htons(port);
    if( bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0 ||
        listen(fd, 8) < 0 )
    {
        close(fd);
        return NULL;
    }

    return new TorrentTracker(fd, seeder, port);
}

bool TorrentTracker::store(const PeerInfo &peer, bool stopped)
{
    std::deque<PeerInfo> &peers = torrent_peers[std::string(peer.info_hash, 20)];

    // Remove existing entry (based on peer id)
    for( std::deque<PeerInfo>::iterator i = peers.begin();
         i != peers.end(); ++i )
    {
        if(memcmp(peer.peer_id, i->peer_id, 20) == 0)
        {
            peers.erase(i);
            break;
        }
    }

    if(!stopped)
    {
        while(int(peers.size()) >= max_peers_per_torrent)
            peers.erase(peers.begin());
        peers.push_back(peer);
        peers.back().last_time = std::time(NULL);
    }
    return true;
}

std::vector<const PeerInfo*> TorrentTracker::list(
    const char *omit_id, const char *info_hash, int count )
{
    std::deque<PeerInfo> &peers = torrent_peers[std::string(info_hash, 20)];

    // Purge peers
    int purge_time = std::time(NULL) - purge_interval;
    std::deque<PeerInfo>::iterator i = peers.begin();
    while(i != peers.end() && peers.front().last_time <= purge_time)
        ++i;
    peers.erase(peers.begin(), i);

    // Build list of results
    std::vector<const PeerInfo*> result;
    for( std::deque<PeerInfo>::const_iterator i = peers.begin();
         i != peers.end(); ++i )
    {
        if( memcmp(info_hash, i->info_hash, 20) == 0 &&
            ( omit_id == NULL || memcmp(omit_id, i->peer_id, 20)) )
            result.push_back(&(*i));
    }

    // Always add local peer
    result.push_back(&local_peer);

    // Send a random (and shuffled) subset of size upto 'count'
    std::random_shuffle(result.begin(), result.end());
    if(int(result.size()) > count)
        result.erase(result.begin() + count, result.end());
    return result;
}

struct TorrentEvent
{
    enum { Added, Removed } type;
    MetaInfo *info;
};

void TorrentTracker::addTorrent(MetaInfo *info, bool take_ownership)
{
    if(!take_ownership)
        info->acquire();
    TorrentEvent event = { TorrentEvent::Added, info };
    event_queue.push(event);
}

void TorrentTracker::removeTorrent(MetaInfo *info)
{
    TorrentEvent event = { TorrentEvent::Removed, info };
    event_queue.push(event);
}

void TorrentTracker::processQueuedEvents()
{
    TorrentEvent event;
    while(event_queue.pop(event))
    {
        switch(event.type)
        {
        case TorrentEvent::Added:
            torrent_peers[event.info->infohash()];
            seeder.addTorrent(event.info);
            break;

        case TorrentEvent::Removed:
            torrent_peers.erase(event.info->infohash());
            seeder.removeTorrent(event.info);
            break;
        }
    }
}
