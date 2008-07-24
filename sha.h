#ifndef SHA_H_INCLUDED
#define SHA_H_INCLUDED

#include <string>
#include <openssl/sha.h>

std::string sha1(const std::string &data);

class SHA1
{
    SHA_CTX context;

public:
    inline SHA1();
    inline void add(const std::string &str);
    inline void add(const void *data, size_t length);
    inline void finish(unsigned char digest[]);
    inline void reset();
};

//
// Implementation of inline functions
//

SHA1::SHA1()
{
    reset();
}

void SHA1::add(const std::string &str)
{
    add(str.data(), str.size());
}

void SHA1::add(const void *data, size_t length)
{
    SHA1_Update(&context, data, length);
}

void SHA1::finish(unsigned char digest[])
{
    SHA1_Final(digest, &context);
}

void SHA1::reset()
{
    SHA1_Init(&context);
}

#endif /*ndef SHA_H_INCLUDED*/
