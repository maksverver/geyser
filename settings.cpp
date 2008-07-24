#include "settings.h"
#include <cctype>
#include <fstream>
#include <sstream>

// Default settings; see header file for descriptions
unsigned        cfg_upload_rate                     = 256*1024;  // 256 KiB/s
std::string     cfg_data_dir                        = "data";
std::string     cfg_metadata_dir                    = "metadata";
std::string     cfg_announce_url                    = "";
unsigned short  cfg_tracker_port                    = 7000;
unsigned        cfg_directory_cooldown              = 60;
unsigned        cfg_directory_update_interval       = 300;
std::string     cfg_metadata_suffix                 = ".torrent";
unsigned short  cfg_seeder_port_min                 = 6881;
unsigned short  cfg_seeder_port_max                 = 6999;
unsigned        cfg_tracker_rerequest_interval      = 90;
unsigned        cfg_tracker_purge_interval          = 120;
unsigned        cfg_tracker_max_peers_per_torrent   = 1000;

const struct Parameter {
    std::string name;
    enum Type { String, Port, Unsigned } type;
    void *data;
} parameters[] = {
#   define DECL(id,type,cast) { #id, Parameter::type, const_cast<cast*>(&cfg_##id) }
#   define STR(id) DECL(id, String, std::string)
#   define PRT(id) DECL(id, Port, unsigned short)
#   define UNS(id) DECL(id, Unsigned, unsigned)
    UNS(upload_rate), STR(data_dir), STR(metadata_dir), STR(announce_url),
    PRT(tracker_port), UNS(directory_cooldown), UNS(directory_update_interval),
    STR(metadata_suffix), PRT(seeder_port_min), PRT(seeder_port_max),
    UNS(tracker_rerequest_interval), UNS(tracker_purge_interval),
    UNS(tracker_max_peers_per_torrent) };
const int num_parameters = sizeof(parameters)/sizeof(*parameters);

#include <iostream> // DEBUG
bool set_parameter(const std::string &key, std::string &value)
{
    for(int n = 0; n < num_parameters; ++n)
        if(parameters[n].name == key)
        {
            switch(parameters[n].type)
            {
            case Parameter::String:
                {
                    *(std::string*)parameters[n].data = value;
                } break;

            case Parameter::Port:
                {
                    std::istringstream iss(value);
                    if(!(iss >> *(unsigned short*)parameters[n].data))
                    {
                        std::cerr << "Invalid port number: \"" << value << "\"!" << std::endl;
                        return false;
                    }
                } break;

            case Parameter::Unsigned:
                {
                    std::istringstream iss(value);
                    if(!(iss >> *(unsigned*)parameters[n].data))
                    {
                        std::cerr << "Invalid unsigned integer: \"" << value << "\"!" << std::endl;
                        return false;
                    }
                } break;
            }
            return true;
        }
    std::cerr << "Unknown parameter: \"" << key << "\"!" << std::endl;
    return false;
}

bool parse_config_line(const std::string line)
{
    // Strip leading/trailing whitespace
    std::string::size_type p = 0, q = line.size(), r, s;
    while(p < line.size() && std::isspace(line[p])) ++p;
    if(p == line.size() || line[p] == '#')
        return true;
    while(std::isspace(line[q - 1])) --q;

    r = line.find('=', p);
    if(r == std::string::npos || r == p || r + 1 == q)
        return false;
    s = r + 1;
    while(std::isspace(line[r - 1])) --r;
    while(std::isspace(line[s])) ++s;

    std::string key   (line.begin() + p, line.begin() + r),
                value (line.begin() + s, line.begin() + q);
    return set_parameter(key, value);
}

bool load_config(const char *filepath)
{
    std::ifstream ifs(filepath);
    if(!ifs)
        return false;

    std::string line;
    int number = 0;
    while(getline(ifs, line))
    {
        ++number;
        if(!parse_config_line(line))
        {
            std::cerr << "Parse error at line " << number
                      << " of config file \"" << filepath << "\"!" << std::endl;
            return false;
        }
    }

    return true;
}
