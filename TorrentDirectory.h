#ifndef TORRENTDIRECTORY_H_INCLUDED
#define TORRENTDIRECTORY_H_INCLUDED

#include "TorrentTracker.h"
#include <string>

class TorrentDirectory
{
    TorrentTracker &tracker;
    MetaInfoMap current;
    std::string data_dir, metadata_dir, announce_url;
    bool single_dir;

public:
    TorrentDirectory(
        TorrentTracker &tracker,
        const char *data_dir, const char *metadata_dir,
        const std::string &announce );
    ~TorrentDirectory();

    bool update(unsigned cooldown);
};

#endif /* ndef TORRENTDIRECTORY_H_INCLUDED */
