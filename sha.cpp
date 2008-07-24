#include "sha.h"

std::string sha1(const std::string &data)
{
    class SHA1 sha;
    sha.add(data);
    char digest[SHA_DIGEST_LENGTH];
    sha.finish((unsigned char*)digest);
    return std::string(digest, SHA_DIGEST_LENGTH);
}
