#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>

// Maximum number of queued bytes per second (excludes protocol overhead)
extern unsigned cfg_upload_rate;

// Data directory; all file and directory entries (except those starting
// with a dot) will be shared.
extern std::string cfg_data_dir;

// Metadata directory; contains .torrent files corresponding to entries
// in the data directory. Note that the server will update entries in this
// directory and will remove .torrent files without a corresponding data entry!
extern std::string cfg_metadata_dir;

// Announce URL. If empty, it is generated from the tracker's hostname and port.
extern std::string cfg_announce_url;

// Port to which the tracker will be bound. Note that if the tracker port is
// ever changed, all metadata files must be manually removed so they will be
// regenerated!
extern unsigned short cfg_tracker_port;

// Specifies the minimum age required for all entries in a data subdirectory
// before its metadata is (re)generated. This prevents metadata files being
// generated while a data directory is still being copied (or otherwise being
// modified).
extern unsigned cfg_directory_cooldown;

// Minimum number of seconds to wait between initiating two updates of the
// metadata directory. If the update process itself takes long, there will
// be a shorter or no pause between updates.
extern unsigned cfg_directory_update_interval;

// Metadata file suffix (e.g. ".torrent"). Do NOT set to the empty string
// if the data and metadata directory are the same!
extern std::string cfg_metadata_suffix;

// Port range on which the seeder is bound.
extern unsigned short cfg_seeder_port_min, cfg_seeder_port_max;

// Interval at which peers should contact the tracker, in seconds.
extern unsigned cfg_tracker_rerequest_interval;

// Duration in seconds after which peers are purged from the list. Should be
// larger than cfg_tracker_rerequest_interval for proper operation.
extern unsigned cfg_tracker_purge_interval;

// Maximum number of peers to track per torrent. When this limit is reached,
// peers will be purged on a first-in-first-out basis to make room for new peers.
extern unsigned cfg_tracker_max_peers_per_torrent;


bool load_config(const char *filepath);

#endif // ndef SETTINGS_H
