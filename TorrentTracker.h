#ifndef TORRENTTRACKER_H_INCLUDED
#define TORRENTTRACKER_H_INCLUDED

#include "Socket.h"
#include "TorrentSeeder.h"
#include "Queue.h"

#include <vector>
#include <ctime>

struct PeerInfo
{
    char info_hash[20], peer_id[20];
    unsigned ip;
    unsigned short port;
    std::time_t last_time;

    long long uploaded, downloaded, left;
};

class TorrentTracker : public Socket
{
    typedef std::deque<PeerInfo> PeerInfoList;
    typedef std::map<std::string, PeerInfoList> TorrentPeerInfoMap;

    unsigned short port;
    TorrentPeerInfoMap torrent_peers;
    PeerInfo local_peer;
    TorrentSeeder &seeder;
    int update_interval, purge_interval, max_peers_per_torrent;
    Queue<TorrentEvent> event_queue;

    TorrentTracker(int fd, TorrentSeeder &seeder, unsigned port);

    void onReadable();

    bool store(const PeerInfo &peer, bool stopped = false);
    std::vector<const PeerInfo*> list(
        const char *omit_id, const char *info_hash, int count );

public:
    static TorrentTracker *create(TorrentSeeder &seeder, unsigned short port);
    ~TorrentTracker();

    void addTorrent(MetaInfo *info, bool take_ownership = false);
    void removeTorrent(MetaInfo *info);
    void processQueuedEvents();

    // Property getter/setters
    inline int updateInterval() { return update_interval; }
    inline void updateInterval(int i) { update_interval = i; }
    inline int purgeInterval() { return purge_interval; }
    inline void purgeInterval(int i) { purge_interval = i; }
    inline int maxPeersPerTorrent() { return max_peers_per_torrent; }
    inline void maxPeersPerTorrent(int i) { max_peers_per_torrent = i; }

    friend class TrackerRequestHandler;
};

#endif /* ndef TorrentSeeder_H_INCLUDED */
