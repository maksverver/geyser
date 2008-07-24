#ifndef TORRENTSEEDER_H_INCLUDED
#define TORRENTSEEDER_H_INCLUDED

#include <string>
#include <map>
#include <deque>

#include "MetaInfo.h"
#include "Socket.h"

class TorrentPeer;
class TorrentEvent;

typedef std::map<std::string, MetaInfo*> MetaInfoMap;

class TorrentSeeder : public Socket
{
protected:
    unsigned short m_port;
    MetaInfoMap metainfo;
    std::deque<TorrentPeer*> peers;

    TorrentSeeder(SocketSet &set, int fd);
    void onReadable();

public:
    static TorrentSeeder *create(SocketSet &set);
    ~TorrentSeeder();

    unsigned ip() const;
    inline unsigned short port() const { return m_port; }
    const std::string &id();

    const MetaInfo *getMetaInfo(const std::string &infohash) const;
    bool hasMetaInfo(const std::string &infohash) const;

    unsigned queueUploadData(unsigned target);
    void removePeer(TorrentPeer &peer);

    void addTorrent(MetaInfo *info);
    void removeTorrent(MetaInfo *info);
};

#endif /* ndef TORRENTSERVER_H_INCLUDED */
