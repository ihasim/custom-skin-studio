#pragma once
// WAD v3 archive (.wad.client): a table of files named by xxh64(lower-case path).
#include <QByteArray>
#include <QString>
#include <map>
#include <cstdint>

namespace lol {

class Wad {
public:
    struct Entry {
        uint64_t hash = 0;
        uint8_t type = 0;          // 0 raw, 1 gzip, 2 satellite, 3 zstd, 4 zstd (sub-chunked)
        uint32_t usize = 0;        // uncompressed size
        QByteArray stored;         // bytes as stored in the archive (compressed or raw)
    };

    static Wad fromBytes(const QByteArray &data, const QString &nameForErrors = {});
    QByteArray toBytes() const;    // v3.3, entries sorted by hash

    bool contains(uint64_t h) const { return entries.count(h) != 0; }
    bool contains(const QString &path) const;
    QByteArray read(uint64_t h) const;                 // decompressed; throws if missing
    QByteArray read(const QString &path) const;
    void put(uint64_t h, const QByteArray &data, bool compress = true);
    void put(const QString &path, const QByteArray &data, bool compress = true);

    std::map<uint64_t, Entry> entries;
};

// Read-only WAD on disk: reads only the table, then single entries on demand (game WADs are large).
class WadFile {
public:
    bool open(const QString &path);
    bool contains(uint64_t h) const { return toc_.count(h) != 0; }
    QByteArray read(uint64_t h) const;     // empty if missing
    QString path() const { return path_; }
private:
    struct Loc { uint32_t off, csize, usize; uint8_t type; };
    std::map<uint64_t, Loc> toc_;
    QString path_;
};

QByteArray zstdDecompress(const QByteArray &in, size_t expected);
QByteArray zstdCompress(const QByteArray &in, int level = 15);

} // namespace lol
