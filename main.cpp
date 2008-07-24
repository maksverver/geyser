#include "settings.h"
#include "TorrentTracker.h"
#include "TorrentSeeder.h"
#include "TorrentDirectory.h"
#include "paths.h"
#include <omnithread.h>
#include <unistd.h>
#include <signal.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>

//
//  Global variables
//

static SocketSet        socket_set;
static TorrentSeeder    *seeder;
static TorrentTracker   *tracker;
static TorrentDirectory *directory;

void run_directory_thread(void *arg)
{
    TorrentDirectory &dir = *(TorrentDirectory*)arg;
    time_t now;
    while(true)
    {
        now = time(0);

        dir.update(cfg_directory_cooldown);

        int s = now + cfg_directory_update_interval - time(0);
        if(s > 0)
            omni_thread::sleep((unsigned)s);
    }
}

void run_main_thread()
{
    // FIXME: make seeder use absolute paths, if that's not too slow
    chdir(cfg_data_dir.c_str());

    int last_time = time(0) - 1;
    unsigned exceeded = 0;
    while(socket_set.process(250))
    {
        tracker->processQueuedEvents();

        // Queue upload data
        for(int t = time(NULL); last_time < t; ++last_time)
        {
            if(exceeded >= cfg_upload_rate)
                exceeded -= cfg_upload_rate;
            else
            {
                unsigned queued = seeder->queueUploadData(cfg_upload_rate - exceeded);
                exceeded = (queued > cfg_upload_rate) ? (queued - cfg_upload_rate) : 0;
            }
        }
    }
}

#ifdef __MINGW32__
#include <winsock.h>
#endif

int main()
{
#ifdef __MINGW32__
    {
        WSADATA wsaData;
        if(WSAStartup(0x0101, &wsaData) != 0)
        {
            std::cerr << "ERROR: could not initialize WinSock 1.1" << std::endl;
            return 1;
        }
    }
#endif

    // Try to load configuration file
    // form (1) home dir, (2) working directory (3) /usr/local/etc (4) /etc
    char *home = getenv("HOME");
    if( !(home && *home && load_config((std::string(home) + "/.geyser.conf").c_str())) &&
        !load_config("geyser.conf") &&
        !load_config("/usr/local/etc/geyser.conf") &&
        !load_config("/etc/geyser.conf") )
    {
        std::cerr << "WARNING: no configuration files found - using only default values!" << std::endl;
    }

    // Create seeder
    if(!(seeder = TorrentSeeder::create(socket_set)))
    {
        std::perror("create seeder");
        return 1;
    }

    // Create tracker
    if(!(tracker = TorrentTracker::create(*seeder, cfg_tracker_port)))
    {
        std::perror("create tracker");
        return 1;
    }

    // Get announce URL
    std::string announce;
    if(!cfg_announce_url.empty())
    {
        announce = cfg_announce_url;
    }
    else
    {
        // Generate announce URL from hostname
        std::string hostname;
        if(!hostName(hostname))
        {
            std::perror("lookup hostname");
            return 1;
        }
        std::ostringstream anounce_ss;
        anounce_ss << "http://" << hostname;
        if(cfg_tracker_port != 80)
            anounce_ss << ":" << cfg_tracker_port;
        anounce_ss << "/announce";
        announce = anounce_ss.str();
    }

    // Create directory
    directory = new TorrentDirectory(
        *tracker, cfg_data_dir.c_str(), cfg_metadata_dir.c_str(), announce );

    // Block unwanted PIPE signal (sent when writing to a closed socket)
    /* Note: it's important to do this before any threads are created, so they inherit
       the correct signal mask. */
    sigset_t signal_mask;
    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGPIPE);
    sigprocmask(SIG_BLOCK, &signal_mask, NULL);

    // Start up.
    omni_thread::create(run_directory_thread, directory);
    run_main_thread();
    return 1;
}
