#pragma once
// League texture (.tex) and DDS: decode to RGBA and encode back (DXT1 / DXT5 / BGRA8, optional mip chain).
#include <QByteArray>
#include <QImage>

namespace lol {

struct Texture {
    enum Format : uint8_t { DXT1 = 10, DXT5 = 12, BGRA8 = 20 };
    QImage image;          // RGBA8888, top mip
    uint8_t format = DXT1;
    bool mips = false;

    static bool looksLikeTexture(const QByteArray &data);
    static Texture fromBytes(const QByteArray &data);      // .tex or .dds
    QByteArray toTex() const;                              // .tex with the same format / mip setting
};

} // namespace lol
