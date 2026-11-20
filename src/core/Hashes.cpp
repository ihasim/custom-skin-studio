#include "core/Hashes.h"
#define XXH_INLINE_ALL
#include <xxhash.h>

namespace lol {

uint32_t fnv1a32(const QString &s) { return fnv1a32(s.toLower().toUtf8().constData()); }

uint32_t fnv1a32(const char *s) {
    uint32_t h = 0x811C9DC5u;
    for (; *s; ++s) {
        unsigned char c = static_cast<unsigned char>(*s);
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        h ^= c; h *= 0x01000193u;
    }
    return h;
}

uint32_t wwiseFnv1(const QString &s) {
    uint32_t h = 0x811C9DC5u;
    const QByteArray b = s.toLower().toUtf8();
    for (char c : b) { h *= 0x01000193u; h ^= static_cast<unsigned char>(c); }
    return h;
}

uint64_t xxh64Path(const QString &path) {
    const QByteArray b = path.toLower().toUtf8();
    return XXH64(b.constData(), size_t(b.size()), 0);
}

uint64_t xxh3(const QByteArray &data) { return XXH3_64bits(data.constData(), size_t(data.size())); }

} // namespace lol
