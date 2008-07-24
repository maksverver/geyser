#include "MetaInfo.h"
#include "sha.h"
#include "paths.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <memory>
#include <queue>

MetaInfo::MetaInfo()
{
}

MetaInfo::~MetaInfo()
{
}

MetaInfo *MetaInfo::fromValue(const Value &value)
{
    MetaInfo *result = new MetaInfo();
    try
    {
        result->announce = value.dictAt("announce").asString();

        const Value &i = value.dictAt("info");
        result->m_name       = i.dictAt("name").asString();
        result->piece_length = i.dictAt("piece length").asInteger();
        result->piece_hashes = i.dictAt("pieces").asString();

        // Restrict piece length to the range [ 2B, 1GiB ]
        if(result->piece_length < 2 || result->piece_length > (1<<30))
            return false;

        bool hasLength = i.dictHasKey("length"),
            hasFiles  = i.dictHasKey("files");
        if((hasLength && hasFiles) || (!hasLength && !hasFiles))
            return false;
        if(hasLength)
        {
            result->type   = file;
            result->length = i.dictAt("length").asInteger();
            if(result->length < 0)
                return false;
            result->files.clear();
        }
        else
        {
            result->type = directory;
            result->length = 0;
            const List &filesList = i.dictAt("files").asList();
            for(size_t n = 0; n < filesList.size(); ++n)
            {
                // Get file length
                long long length = filesList[n].dictAt("length").asInteger();
                if(length < 0)
                    return false;
                result->length += length;

                // Get file path
                const List &pathList = filesList[n].dictAt("path").asList();
                std::string path;
                for(size_t p = 0; p < pathList.size(); ++p)
                {
                    const std::string &component = pathList[p].asString();
                    if( component == "." || component == ".." ||
                        component.find('/') != std::string::npos )
                        return false;
                    path += '/';
                    path += component;
                }
                result->files.push_back(std::make_pair(path, length));
            }
        }

        // Check lengths
        long long hashes_size = (long long)result->piece_hashes.size();
        if( hashes_size%20 != 0 || hashes_size/20 !=
                result->length/result->piece_length +
                (result->length%result->piece_length ? 1 : 0) )
            return false;

        // Set info hash
        result->m_infohash = sha1(bencode(i));

        return result;
    }
    catch(const ValueError &)
    {
        delete result;
        return false;
    }
}

MetaInfo *MetaInfo::fromFile(std::istream &stream)
{
    Value value;
    if(!bdecode(stream, value))
    {
        // DEBUG
        std::cerr << "Metainfo file is improperly encoded." << std::endl;
        return false;
    }

    MetaInfo *info = MetaInfo::fromValue(value);
    if(!info)
    {
        // DEBUG
        std::cerr << "Metainfo file contains invalid data." << std::endl;

        delete info;
        return false;
    }

    return info;
}

MetaInfo *MetaInfo::fromPath(const char *filepath)
{
    std::ifstream ifs(filepath, std::ifstream::binary);
    if(!ifs)
    {
        // DEBUG
        std::cerr << "Could not read file at \"" << filepath << "\"." << std::endl;
        return NULL;
    }

    return fromFile(ifs);
}

bool index(std::string::size_type start, const std::string &path, MetaInfo::FileList &list)
{
    struct stat st;
    if(stat(path.c_str(), &st) != 0)
        return false;
    if(S_ISDIR(st.st_mode))
    {
#ifdef S_ISLNK
        if(S_ISLNK(st.st_mode))
            return true;    // Skip symlinks to directories
#endif

        DIR *dir = opendir(path.c_str());
        if(dir == NULL)
            return false;
        struct dirent *entry;
        while((entry = readdir(dir)))
        {
            if(entry->d_name[0] == '.' && (
                entry->d_name[1] == '\0' ||
                (entry->d_name[1] == '.' && entry->d_name[2] == '\0') ))
            {
                // Skip "." and ".." entries
                continue;
            }

            std::string new_path;
            new_path.reserve(path.size() + 1 + std::strlen(entry->d_name));
            new_path += path;
            new_path += '/';
            new_path += entry->d_name;
            index(start, new_path, list);
        }
        closedir(dir);
        return true;
    }
    else
    if(S_ISREG(st.st_mode))
    {
        list.push_back(std::make_pair(path.substr(start), st.st_size));
        return true;
    }
    return true;
}

MetaInfo *MetaInfo::generate(
    const std::string &filepath, const std::string &announce, unsigned piece_length )
{

    std::auto_ptr<MetaInfo> info(new MetaInfo());

    // Determine file type
    if(isDir(filepath.c_str()))
        info->type = directory;
    else
    if(isFile(filepath.c_str()))
        info->type = file;
    else
        return NULL;

    // Search for files
    if(!index(filepath.size(), filepath, info->files))
        return NULL;

    // Compute total length
    info->length = 0;
    for( FileList::const_iterator i = info->files.begin();
         i != info->files.end(); ++i )
        info->length += i->second;

    // Set name
    info->m_name = baseName(filepath.c_str());

    // Set announce URL
    info->announce = announce;

    // Create piece hashes
    info->piece_length  = piece_length;
    info->piece_hashes.reserve(20*((info->length + piece_length - 1)/piece_length));
    class SHA1 hash;
    long long piece_pos = 0;
    for( FileList::const_iterator i = info->files.begin();
         i != info->files.end(); ++i )
    {
        std::ifstream ifs((filepath + i->first).c_str(), std::ifstream::binary);
        long long read = 0;
        while(read < i->second)
        {
            char buffer[8192];
            long long size = std::min(
                std::min((long long)sizeof(buffer), i->second - read), piece_length - piece_pos);
            if(!ifs.read(buffer, size))
                return NULL; // read error

            read      += size;
            piece_pos += size;
            hash.add(buffer, size);
            if(piece_pos == piece_length)
            {
                // Add piece hash
                unsigned char digest[20];
                hash.finish(digest);
                info->piece_hashes.append((char*)digest, 20);
                hash.reset();
                piece_pos = 0;
            }
        }
    }
    // Add hash for final (incomplete) piece
    if(piece_pos != 0)
    {
        unsigned char digest[20];
        hash.finish(digest);
        info->piece_hashes.append((char*)digest, 20);
    }

    // Set infohash
    Value value;
    info->toValue(value);
    info->m_infohash = sha1(bencode(value.dictAt("info")));

    return info.release();
}


bool MetaInfo::validRequest(unsigned piece, unsigned begin, unsigned length) const
{
    if(piece >= pieces())
        return false;

    unsigned l = pieceLength(piece);
    return l >= begin && l - begin >= length;
}

bool MetaInfo::fetchPiece(unsigned piece, ByteBuffer &data) const
{
    // Note: assumes current working directory is data directory
    // FIXME: make this more efficient for torrents with a large number of files

    long long skip = piece_length*piece, pos = 0ll, size = pieceLength(piece);
    data.resize(size);

    if(type == file)
    {
        // Read piece from a file
        std::ifstream ifs(m_name.c_str(), std::ifstream::binary);
        ifs.seekg(piece_length*piece, std::ios_base::beg);
        return ifs.read(&data[0], pieceLength(piece));
    }
    else
    {
        // Read piece from a directory
        chdir(name().c_str());
        for( FileList::const_iterator i = files.begin();
            i != files.end(); ++i )
        {
            const long long &file_size = i->second;
            if(file_size <= skip)
            {
                // Skip this file entirely
                skip -= file_size;
                continue;
            }

            long long read_size = std::min(file_size - skip, size - pos);
            std::ifstream ifs(i->first.c_str() + 1, std::ifstream::binary);
            if(skip > 0)
            {
                if(!ifs.seekg(skip, std::ios_base::beg))
                    break;
                skip = 0;
            }
            if(!ifs.read(&data[pos], read_size))
                break;
            pos += read_size;
        }
        chdir("..");
        return pos == size;
    }
}

void MetaInfo::toValue(Value &result) const
{
    result.clear();
    Dict &resultDict = result.makeDict();
    resultDict["announce"].assign(announce);

    Dict &infoDict = resultDict["info"].makeDict();
    infoDict["name"].assign(m_name);
    infoDict["piece length"].assign(piece_length);
    infoDict["pieces"].assign(piece_hashes);

    if(type == file)
    {
        infoDict["length"].assign(length);
    }
    else
    {
        List &filesList = infoDict["files"].makeList();
        filesList.resize(files.size());
        for(size_t n = 0; n < files.size(); ++n)
        {
            Dict &fileDict = filesList[n].makeDict();
            fileDict["length"].assign(files[n].second);

            List &pathList = fileDict["path"].makeList();
            const char *i, *j = files[n].first.c_str();
            size_t parts = 0;
            do {
                i = ++j;
                while(*j && *j != '/')
                    ++j;
                pathList.resize(parts + 1);
                pathList[parts++].assign(i, j - i);
            } while(*j);
        }
    }
}

void MetaInfo::toFile(std::ostream &stream) const
{
    Value value;
    toValue(value);
    bencode(stream, value);
}

void MetaInfo::toPath(const char *filepath) const
{
    std::ofstream ofs(filepath, std::ofstream::binary);
    toFile(ofs);
}

