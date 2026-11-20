#include "core/Tex.h"
#include "core/Bc.h"
#include "core/Hashes.h"
#include <vector>

namespace lol {

static int levelSize(int w, int h, uint8_t fmt) {
    if (fmt == Texture::BGRA8) return w * h * 4;
    return ((w + 3) / 4) * ((h + 3) / 4) * (fmt == Texture::DXT5 ? 16 : 8);
}

static QImage decodeLevel(const QByteArray &d, int w, int h, uint8_t fmt) {
    if (fmt == Texture::BGRA8) {
        QImage img(reinterpret_cast<const uchar *>(d.constData()), w, h, w * 4, QImage::Format_ARGB32);  // BGRA in memory (LE)
        return img.convertToFormat(QImage::Format_RGBA8888);
    }
    return decodeBc(d, w, h, fmt == Texture::DXT5);
}

bool Texture::looksLikeTexture(const QByteArray &d) { return d.startsWith("TEX\0") || d.startsWith("DDS "); }

Texture Texture::fromBytes(const QByteArray &data) {
    Texture t;
    Reader r(data);
    if (data.startsWith(QByteArray("TEX\0", 4))) {
        r.skip(4);
        int w = r.get<uint16_t>(), h = r.get<uint16_t>();
        r.get<uint8_t>();
        t.format = r.get<uint8_t>();
        r.get<uint8_t>();
        t.mips = (r.get<uint8_t>() & 1) != 0;
        if (t.format != DXT1 && t.format != DXT5 && t.format != BGRA8)
            throw FormatError("unsupported TEX format " + std::to_string(t.format));
        const int top = levelSize(w, h, t.format);
        // mips are stored smallest first, so the full-size level is at the end
        QByteArray lvl = data.right(top);
        if (lvl.size() < top) throw FormatError("TEX data too short");
        t.image = decodeLevel(lvl, w, h, t.format);
        return t;
    }
    if (data.startsWith("DDS ")) {
        r.seek(12);
        int h = int(r.get<uint32_t>()), w = int(r.get<uint32_t>());
        r.seek(28); int mipCount = int(r.get<uint32_t>());
        r.seek(84); QByteArray fourcc = r.bytes(4);
        r.seek(88); uint32_t bits = r.get<uint32_t>();
        int off = 128;
        if (fourcc == "DXT1") t.format = DXT1;
        else if (fourcc == "DXT5") t.format = DXT5;
        else if (fourcc == QByteArray(4, '\0') && bits == 32) t.format = BGRA8;
        else throw FormatError("unsupported DDS format");
        t.mips = mipCount > 1;
        t.image = decodeLevel(data.mid(off, levelSize(w, h, t.format)), w, h, t.format);   // DDS: largest first
        return t;
    }
    throw FormatError("not a texture");
}

QByteArray Texture::toTex() const {
    QImage top = image.convertToFormat(QImage::Format_RGBA8888);
    std::vector<QByteArray> levels;
    int w = top.width(), h = top.height();
    QImage cur = top;
    while (true) {
        QByteArray enc;
        if (format == BGRA8) {
            QImage bgra = cur.convertToFormat(QImage::Format_ARGB32);
            enc = QByteArray(reinterpret_cast<const char *>(bgra.constBits()), qsizetype(bgra.width()) * bgra.height() * 4);
        } else {
            enc = encodeBc(cur, format == DXT5);
        }
        levels.push_back(enc);
        if (!mips || (cur.width() == 1 && cur.height() == 1)) break;
        int nw = std::max(1, cur.width() / 2), nh = std::max(1, cur.height() / 2);
        cur = top.scaled(nw, nh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    Writer wr;
    wr.bytes("TEX\0", 4);
    wr.put<uint16_t>(uint16_t(w)); wr.put<uint16_t>(uint16_t(h));
    wr.put<uint8_t>(1); wr.put<uint8_t>(format); wr.put<uint8_t>(0); wr.put<uint8_t>(mips ? 1 : 0);
    for (auto it = levels.rbegin(); it != levels.rend(); ++it) wr.bytes(*it);
    return wr.buf;
}

} // namespace lol
