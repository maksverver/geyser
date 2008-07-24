#include "paths.h"

#include <unistd.h>
#ifdef __MINGW32__
#include <winsock.h>
#else
#include <libgen.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

bool isDir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

bool isFile(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

std::string realPath(const char *path)
{
    char real_path[PATH_MAX];
#ifdef __MINGW32__
    strcpy(real_path, path);
#else
    realpath(path, real_path);
#endif
    return real_path;
}

#ifdef __MINGW32__
char *basename(char *path)
{
    if(!path || !*path)
        return ".";
    
    char *end = path;
    while(*end)
      ++end;
    while(end != path && (*end == '/' || *end == '\\'))
      --end;
    if(end == path)
    {
       end[1] = '\0';
       return end;
    }

    *end = '\0';
    char *begin = end;
    while(begin != path && begin[-1] != '/' && begin[-1] != '\\')
      --begin;
    return begin;
}
#endif

std::string baseName(const char *path)
{
    std::string result;
    char *name = strdup(path);
    result = basename(name);
    free(name);
    return result;
}

bool hostName(std::string &result)
{
    char buffer[1024];
    buffer[sizeof(buffer) - 1] = '\0';
    if( gethostname(buffer, sizeof(buffer)) != 0 ||
        buffer[sizeof(buffer) - 1] != '\0' )
    {
        return false;
    }
    else
    {
        result = buffer;
        return true;
    }
}
