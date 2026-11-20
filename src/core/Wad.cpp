#include "core/Wad.h"
#include "core/Hashes.h"
#include <zstd.h>
#include <miniz.h>
#include <QFile>
#include <algorithm>
#include <cstring>
#include <vector>

namespace lol {

QByteArray zstdDecompress(const QByteArray &in, size_t expected) {
    QByteArray out;
    out.resize(qsizetype(expected));
    ZSTD_DStream *ds = ZSTD_createDStream();
    ZSTD_initDStream(ds);
    ZSTD_inBuffer ib{in.constData(), size_t(in.size()), 0};
    ZSTD_outBuffer ob{out.data(), size_t(out.size()), 0};
    while (ib.pos < ib.size) {
        // Sub-chunked entries may mix zstd frames with raw (stored) chunks; copy raw bytes through.
        if (ib.size - ib.pos >= 4) {
            uint32_t magic; std::memcpy(&magic, static_cast<const char *>(ib.src) + ib.pos, 4);
            if (magic != ZSTD_MAGICNUMBER) {
                size_t n = std::min(ib.size - ib.pos, ob.size - ob.pos);
                const char *src = static_cast<const char *>(ib.src) + ib.pos;
                // copy up to the next zstd frame
                size_t k = 4;
                while (k + 4 <= ib.size - ib.pos) {
                    uint32_t m; std::memcpy(&m, src + k, 4);
                    if (m == ZSTD_MAGICNUMBER) break;
                    ++k;
                }
                if (k + 4 > ib.size - ib.pos) k = ib.size - ib.pos;
                n = std::min(n, k);
                std::memcpy(static_cast<char *>(ob.dst) + ob.pos, src, n);
                ob.pos += n; ib.pos += n;
                continue;
            }
        }
        size_t r = ZSTD_decompressStream(ds, &ob, &ib);
        if (ZSTD_isError(r)) { ZSTD_freeDStream(ds); throw FormatError(std::string("zstd: ") + ZSTD_getErrorName(r)); }
        if (r == 0) ZSTD_initDStream(ds);   // frame finished, maybe another follows
        if (ob.pos == ob.size && ib.pos < ib.size && r != 0) break;
    }
    ZSTD_freeDStream(ds);
    out.resize(qsizetype(ob.pos));
    return out;
}

QByteArray zstdCompress(const QByteArray &in, int level) {
    QByteArray out;
    out.resize(qsizetype(ZSTD_compressBound(size_t(in.size()))));
    size_t n = ZSTD_compress(out.data(), size_t(out.size()), in.constData(), size_t(in.size()), level);
    if (ZSTD_isError(n)) throw FormatError("zstd compress failed");
    out.resize(qsizetype(n));
    return out;
}

static QByteArray gunzip(const QByteArray &in, size_t expected) {
    // gzip header (RFC 1952), then raw deflate
    Reader r(in);
    if (r.get<uint8_t>() != 0x1f || r.get<uint8_t>() != 0x8b) throw FormatError("bad gzip");
    r.skip(1); uint8_t flg = r.get<uint8_t>(); r.skip(6);
    if (flg & 4) { uint16_t x = r.get<uint16_t>(); r.skip(x); }
    if (flg & 8) r.cstr();
    if (flg & 16) r.cstr();
    if (flg & 2) r.skip(2);
    QByteArray out; out.resize(qsizetype(expected));
    size_t n = tinfl_decompress_mem_to_mem(out.data(), expected, r.ptr(), size_t(r.left()), 0);
    if (n == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED) throw FormatError("gzip inflate failed");
    out.resize(qsizetype(n));
    return out;
}

Wad Wad::fromBytes(const QByteArray &data, const QString &name) {
    Reader r(data);
    if (data.size() < 4 || data[0] != 'R' || data[1] != 'W')
        throw FormatError(("not a WAD file: " + name).toStdString());
    r.skip(2);
    uint8_t major = r.get<uint8_t>(); r.get<uint8_t>();
    if (major != 3) throw FormatError(("unsupported WAD version " + QString::number(major) + ": " + name).toStdString());
    r.skip(256); r.skip(8);
    uint32_t count = r.get<uint32_t>();
    Wad w;
    for (uint32_t i = 0; i < count; ++i) {
        Entry e;
        e.hash = r.get<uint64_t>();
        uint32_t off = r.get<uint32_t>(), csize = r.get<uint32_t>();
        e.usize = r.get<uint32_t>();
        uint8_t t = r.get<uint8_t>();
        e.type = t & 0x0F;
        r.skip(1 + 2 + 8);
        if (qsizetype(off) + csize > data.size()) throw FormatError("WAD entry out of range");
        e.stored = data.mid(off, csize);
        w.entries[e.hash] = std::move(e);
    }
    return w;
}

QByteArray Wad::toBytes() const {
    Writer w;
    w.bytes("RW", 2); w.put<uint8_t>(3); w.put<uint8_t>(3);
    w.bytes(QByteArray(256, '\0')); w.put<uint64_t>(0);
    w.put<uint32_t>(uint32_t(entries.size()));
    uint32_t off = uint32_t(272 + 32 * entries.size());
    for (const auto &[h, e] : entries) {          // std::map keeps them sorted by hash
        w.put<uint64_t>(h); w.put<uint32_t>(off); w.put<uint32_t>(uint32_t(e.stored.size()));
        w.put<uint32_t>(e.usize); w.put<uint8_t>(e.type); w.put<uint8_t>(0); w.put<uint16_t>(0);
        w.put<uint64_t>(xxh3(e.stored));
        off += uint32_t(e.stored.size());
    }
    for (const auto &[h, e] : entries) w.bytes(e.stored);
    return w.buf;
}

bool Wad::contains(const QString &path) const { return contains(xxh64Path(path)); }

QByteArray Wad::read(uint64_t h) const {
    auto it = entries.find(h);
    if (it == entries.end()) throw FormatError("file not in WAD");
    const Entry &e = it->second;
    switch (e.type) {
    case 0: return e.stored;
    case 1: return gunzip(e.stored, e.usize);
    case 3: case 4: return zstdDecompress(e.stored, e.usize);
    default: throw FormatError("unsupported WAD entry type");
    }
}

QByteArray Wad::read(const QString &path) const { return read(xxh64Path(path)); }

void Wad::put(uint64_t h, const QByteArray &data, bool compress) {
    Entry e; e.hash = h; e.usize = uint32_t(data.size());
    if (compress) { e.type = 3; e.stored = zstdCompress(data); }
    else { e.type = 0; e.stored = data; }
    entries[h] = std::move(e);
}

void Wad::put(const QString &path, const QByteArray &data, bool compress) { put(xxh64Path(path), data, compress); }

bool WadFile::open(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QByteArray head = f.read(272);
    if (head.size() < 272 || head[0] != 'R' || head[1] != 'W' || uint8_t(head[2]) != 3) return false;
    uint32_t n; std::memcpy(&n, head.constData() + 268, 4);
    QByteArray toc = f.read(qint64(n) * 32);
    if (toc.size() < qsizetype(n) * 32) return false;
    toc_.clear();
    for (uint32_t i = 0; i < n; ++i) {
        const char *p = toc.constData() + i * 32;
        uint64_t h; Loc l;
        std::memcpy(&h, p, 8); std::memcpy(&l.off, p + 8, 4); std::memcpy(&l.csize, p + 12, 4); std::memcpy(&l.usize, p + 16, 4);
        l.type = uint8_t(p[20]) & 0x0F;
        toc_[h] = l;
    }
    path_ = path;
    return true;
}

QByteArray WadFile::read(uint64_t h) const {
    auto it = toc_.find(h);
    if (it == toc_.end()) return {};
    QFile f(path_);
    if (!f.open(QIODevice::ReadOnly) || !f.seek(it->second.off)) return {};
    QByteArray raw = f.read(it->second.csize);
    Wad tmp;
    Wad::Entry e; e.hash = h; e.type = it->second.type; e.usize = it->second.usize; e.stored = raw;
    tmp.entries[h] = e;
    try { return tmp.read(h); } catch (const FormatError &) { return {}; }
}

} // namespace lol
