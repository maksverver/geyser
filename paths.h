#ifndef PATHS_H_INCLUDED
#define PATHS_H_INCLUDED
#include <string>

bool isDir(const char *path);
bool isFile(const char *path);
std::string realPath(const char *path);
std::string baseName(const char *baseName);
bool hostName(std::string &result);

#endif /*ndef PATHS_H_INCLUDED*/

