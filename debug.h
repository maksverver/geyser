#ifndef DEBUG_H_INCLUDED
#define DEBUG_H_INCLUDED

#include <string>
#include <ostream>

std::ostream &hexdump(std::ostream &os, const char *data, size_t len);

inline std::ostream &hexdump(std::ostream &os, const std::string &str) {
    return hexdump(os, str.data(), str.size());
}

#endif /* ndef DEBUG_H_INCLUDED */
