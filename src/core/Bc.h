#pragma once
// BC1 (DXT1) and BC3 (DXT5) block compression, both directions. Images are RGBA8888 (QImage::Format_RGBA8888).
#include <QByteArray>
#include <QImage>

namespace lol {

QImage decodeBc(const QByteArray &blocks, int w, int h, bool bc3);
QByteArray encodeBc(const QImage &rgba, bool bc3);     // one mip level; pads partial blocks by clamping

} // namespace lol
