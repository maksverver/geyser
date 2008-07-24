#include "debug.h"

std::ostream &hexdump(std::ostream &os, const char *data, size_t len)
{
    static const char digits[] = "0123456789ABCDEF";
    for(size_t n = 0; n < len; ++n)
        os << digits[(data[n]>>4)&15] << digits[data[n]&15] << ' ';
    return os << "(" << len << " bytes)";
}
