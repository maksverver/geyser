#include "TorrentDirectory.h"
#include "MetaInfo.h"
#include "settings.h"
#include "paths.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <cstdio>
#include <ctime>
#include <algorithm>
#include <iostream>

class DirList
{
    const std::string base;
    DIR *dir;
    struct dirent *de;

public:
    DirList(const std::string &base, bool warn = true)
        : base(base), dir(opendir(base.c_str()))
    {
        if(!dir && warn)
            std::perror(base.c_str());
    }

    ~DirList() {
        if(dir)
            closedir(dir);
    }

    operator bool () {
        return dir;
    }

    bool read() {
        return dir && (de = readdir(dir));
    }

    std::string path() const {
        std::string p = base;
        p += '/';
        p += de->d_name;
        return p;
    }

    const char *name() const {
        return de->d_name;
    }
};

// Maps file names to their modification time
typedef std::map<std::string, time_t> FileModMap;

TorrentDirectory::TorrentDirectory(
    TorrentTracker &tracker, const char *data_dir, const char *metadata_dir,
    const std::string &announce_url )
    : tracker(tracker),
      data_dir(realPath(data_dir)), metadata_dir(realPath(metadata_dir)),
      announce_url(announce_url), single_dir(data_dir == metadata_dir)
{
}

TorrentDirectory::~TorrentDirectory()
{
}

time_t maxTime(const std::string path)
{
    struct stat st;

    if(stat(path.c_str(), &st) != 0)
        return 0;

    time_t res = st.st_mtime;

    if(S_ISDIR(st.st_mode))
    {
        DirList dl(path);
        while(dl.read())
        {
            const char *name = dl.name();
            if( name[0] == '.' && (name[1] == '\0' ||
                (name[1] == '.' && name[2] == '\0')) )
                continue;   // Skip "." and ".." entries
            res = std::max(res, maxTime(dl.path()));
        }
    }

    return res;
}

bool TorrentDirectory::update(unsigned cooldown)
{
    FileModMap metadata, data;

#ifdef DEBUG
    std::cerr << "Scanning data directory: " << data_dir << std::endl;
#endif

    {   // Generate listing of name and modification time for data dir
        DirList dl(data_dir);
        if(!dl)
            return false;
        while(dl.read()) {
            std::string name = dl.name();
            if(name[0] == '.')
                continue;   // Skip files starting with '.'

            if( single_dir && (name.size() >= cfg_metadata_suffix.size() &&
                name.find(cfg_metadata_suffix, name.size() - cfg_metadata_suffix.size()) != std::string::npos) )
                continue;   // Skip torrent files in data dir

            std::string path = dl.path();
            if(!(isFile(path.c_str()) || isDir(path.c_str())))
                continue;   // Only allow regular files and directories

#ifdef DEBUG
            std::cerr << "\t" << path << std::endl;
#endif

            time_t t = maxTime(dl.path());
            if(t > 0)
                data[name] = t;
        }
    }

#ifdef DEBUG
    std::cerr << "Scanning metadata directory: " << metadata_dir << std::endl;
#endif

    {   // Generate listing of name and modification time for 'metadata' dir
        DirList dl(metadata_dir);
        if(!dl)
            return false;
        while(dl.read()) {
            std::string name = dl.name();
            if(name[0] == '.')
                continue;   // Skip files starting with '.'

            if( name.size() <= cfg_metadata_suffix.size() ||
                name.find(cfg_metadata_suffix, name.size() - cfg_metadata_suffix.size())
                    == std::string::npos )
                continue;   // Skip files not ending in .torrent

            std::string path = dl.path();
            if(!isFile(path.c_str()))
                continue;   // Only allow regular files

#ifdef DEBUG
            std::cerr << "\t" << path << std::endl;
#endif

            time_t t = maxTime(path);
            if(t > 0)
                metadata[name.substr(0, name.size() - 8)] = t;
        }
    }

#ifdef DEBUG
    std::cerr << "Registering new metadata files..." << std::endl;
#endif

    // Add new metadata files
    for(FileModMap::const_iterator i = data.begin(); i != data.end(); ++i)
    {
        if(i->second + (time_t)cooldown >= time(0))
            continue;   // Do not add file until after the cooldown period

        if(current.find(i->first) != current.end())
            continue;   // Already registered.

        // Check to see if the metadata entry already exists
        FileModMap::const_iterator j = metadata.find(i->first);
        std::string mi_path = metadata_dir + '/' + i->first + cfg_metadata_suffix;
        MetaInfo *mi = 0;
        if(j == metadata.end() || i->second > j->second)
        {
#ifdef DEBUG
            std::cerr << "\tgenerating " << mi_path << std::endl;
#endif
            // Regenerate metadata info
            mi = MetaInfo::generate(data_dir + '/' + i->first, announce_url);

            if(mi)
                mi->toPath(mi_path.c_str());
        }
        else
        {
#ifdef DEBUG
            std::cerr << "\tloading " << mi_path << std::endl;
#endif
            // Read metadata info from file
            mi = MetaInfo::fromPath(mi_path.c_str());
        }

        if(mi)
        {
            // Register it.
            tracker.addTorrent(mi, true);
            current[i->first] = mi;
        }
#ifdef DEBUG
        else
        {
            std::cerr << "\t\t failed!" << std::endl;
        }
#endif
    }

#ifdef DEBUG
    std::cerr << "Removing obsolete metadata files..." << std::endl;
#endif

    // Remove obsolete metadata files
    for(FileModMap::const_iterator i = metadata.begin(); i != metadata.end(); ++i)
    {
        if(data.find(i->first) == data.end())
        {
            // Unlink metadata file without corresponding data file
            // or with a data file that is too new.
            std::string path = metadata_dir + '/' + i->first + cfg_metadata_suffix;
#ifdef DEBUG
            std::cerr << "\t" << path<< std::endl;
#endif
            if(unlink(path.c_str()) != 0)
                perror(path.c_str());

            MetaInfoMap::iterator k = current.find(i->first);
            if(k != current.end())
            {
                // Remove torrent from server and current listing
                tracker.removeTorrent(k->second);
                current.erase(k);
            }
        }
    }

#ifdef DEBUG
    std::cerr << "Directory updated!" << std::endl;
#endif

    return true;
}
