#include "MetaInfo.h"
#include "paths.h"
#include <iostream>
#include <sstream>
#include <string>
#include <limits.h>
#include <stdlib.h>

#include "debug.h"

int main(int argc, char *argv[])
{
    unsigned short port = 7000;

    if(argc != 2 && argc != 3)
    {
        std::cerr << "Usage: " << baseName(argv[0])
                  << " <input> [<output>]" << std::endl;
        return argc != 1;
    }

    std::string input = argv[1], output = (argc >= 3 ? argv[2] : ".");

    // Build announce URL
    std::string hostname;
    if(!hostName(hostname))
    {
        std::cerr << "Could not determine hostname!" << std::endl;
        return 1;
    }
    std::ostringstream anounce_ss;
    anounce_ss << "http://" << hostname;
    if(port != 80)
        anounce_ss << ":" << port;
    anounce_ss << "/announce";
    std::string announce = anounce_ss.str();

    // TODO: get piece size from command line
    MetaInfo *info = MetaInfo::generate(realPath(input.c_str()), announce);
    if(!info)
    {
        std::cerr << "Could not read files from path \"" << argv[1] << "\"!" << std::endl;
        return 1;
    }

    if(output[0] == '-' && output[1] == '\0')
        info->toFile(std::cout);
    else
    {
        if(isDir(output.c_str()))
        {
            if(*output.rbegin() != '/')
                output += '/';
            output += info->name();
            output += ".torrent";
        }
        info->toPath(output.c_str());
    }

    delete info;
}
