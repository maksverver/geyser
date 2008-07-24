#ifndef METAINFO_H_INCLUDED
#define METAINFO_H_INCLUDED

#include "RefCountingObject.h"
#include "bcoding.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

typedef char Byte;
typedef std::vector<Byte> ByteBuffer;

class MetaInfo : public RefCountingObject
{
public:
    typedef std::vector<std::pair<std::string, long long> > FileList;

private:
    std::string announce;

    enum { file, directory } type;
    std::string m_name;

    long long length, piece_length;
    std::string piece_hashes;

    FileList files;

    std::string m_infohash;

    MetaInfo();

public:
    static MetaInfo *fromValue(const Value &value);
    static MetaInfo *fromFile(std::istream &stream);
    static MetaInfo *fromPath(const char *filepath);

    static MetaInfo *generate( const std::string &filepath,
        const std::string &announce, unsigned piece_length = 1<<18 );

    ~MetaInfo();

    inline const std::string &name() const { return m_name; }
    inline const std::string &infohash() const { return m_infohash; }

    inline unsigned pieces() const;
    inline unsigned pieceLength(unsigned piece) const;

    bool validRequest(unsigned piece, unsigned begin, unsigned length) const;
    bool fetchPiece(unsigned piece, ByteBuffer &data) const;

    void toValue(Value &result) const;
    void toFile(std::ostream &stream) const;
    void toPath(const char *filepath) const;
};

unsigned MetaInfo::pieces() const
{
    return (length + piece_length - 1)/piece_length;
};

// Note: assumes piece is in range [0, pieces)
unsigned MetaInfo::pieceLength(unsigned piece) const
{
    return std::min(length - piece_length*piece, piece_length);
}

#endif /*ndef METAINFO_H_INCLUDED */
