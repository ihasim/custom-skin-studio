#pragma once
// Colour regions of a model texture: k-means in Lab over the texels the mesh actually uses, soft (two-label)
// assignment so region borders blend, and per-region hue / saturation / brightness / contrast edits.
#include "core/Mesh.h"
#include <QColor>
#include <QImage>
#include <QString>
#include <vector>

namespace lol {

struct RegionAdjust {
    float hue = 0;        // degrees
    float sat = 1;        // multiplier
    float val = 1;        // brightness multiplier
    float contrast = 1;   // around the region's mean brightness
    bool isIdentity() const { return hue == 0 && sat == 1 && val == 1 && contrast == 1; }
};

struct ColorRegion {
    QColor mean;          // average colour (swatch)
    float meanHue = 0, meanSat = 0, meanVal = 0;
    double share = 0;     // fraction of used texels
    QString name;
};

struct RegionMap {
    int w = 0, h = 0;
    std::vector<uint8_t> l1, l2, mix;     // per texel: best label, second label, weight of best (0..255); l1 = 255 unused
    std::vector<ColorRegion> regions;

    static RegionMap build(const QImage &rgba, const std::vector<uint8_t> &used, int k);
    int regionAt(int x, int y) const;
};

std::vector<uint8_t> uvMask(const SkinnedMesh &m, const std::vector<int> &submeshes, int w, int h);
QImage applyRegions(const QImage &original, const RegionMap &map, const std::vector<RegionAdjust> &adj, int highlight = -1);
QString colorName(const QColor &c);
void rgbToHsv(float r, float g, float b, float &h, float &s, float &v);
void hsvToRgb(float h, float s, float v, float &r, float &g, float &b);

} // namespace lol
