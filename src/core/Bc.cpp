#include "core/Bc.h"
#include <QtConcurrent/QtConcurrentMap>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#include <vector>

namespace lol {

namespace {

struct RGB { float r, g, b; };

inline void unpack565(uint16_t c, uint8_t out[3]) {
    int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    out[0] = uint8_t((r << 3) | (r >> 2)); out[1] = uint8_t((g << 2) | (g >> 4)); out[2] = uint8_t((b << 3) | (b >> 2));
}

inline uint16_t pack565(const RGB &c) {
    auto q = [](float v, int max) { return int(std::lround(std::clamp(v, 0.0f, 255.0f) * max / 255.0f)); };
    return uint16_t((q(c.r, 31) << 11) | (q(c.g, 63) << 5) | q(c.b, 31));
}

void decodeColorBlock(const uint8_t *b, uint8_t out[16][4], bool forceFour) {
    uint16_t c0, c1; std::memcpy(&c0, b, 2); std::memcpy(&c1, b + 2, 2);
    uint8_t pal[4][4];
    unpack565(c0, pal[0]); unpack565(c1, pal[1]);
    pal[0][3] = pal[1][3] = 255;
    if (c0 > c1 || forceFour) {
        for (int k = 0; k < 3; ++k) { pal[2][k] = uint8_t((2 * pal[0][k] + pal[1][k] + 1) / 3); pal[3][k] = uint8_t((pal[0][k] + 2 * pal[1][k] + 1) / 3); }
        pal[2][3] = pal[3][3] = 255;
    } else {
        for (int k = 0; k < 3; ++k) { pal[2][k] = uint8_t((pal[0][k] + pal[1][k]) / 2); pal[3][k] = 0; }
        pal[2][3] = 255; pal[3][3] = 0;
    }
    uint32_t idx; std::memcpy(&idx, b + 4, 4);
    for (int i = 0; i < 16; ++i) std::memcpy(out[i], pal[(idx >> (2 * i)) & 3], 4);
}

void decodeAlphaBlock(const uint8_t *b, uint8_t out[16][4]) {
    uint8_t a[8]; a[0] = b[0]; a[1] = b[1];
    if (a[0] > a[1]) for (int i = 1; i < 7; ++i) a[i + 1] = uint8_t(((7 - i) * a[0] + i * a[1]) / 7);
    else { for (int i = 1; i < 5; ++i) a[i + 1] = uint8_t(((5 - i) * a[0] + i * a[1]) / 5); a[6] = 0; a[7] = 255; }
    uint64_t bits = 0; for (int i = 0; i < 6; ++i) bits |= uint64_t(b[2 + i]) << (8 * i);
    for (int i = 0; i < 16; ++i) out[i][3] = a[(bits >> (3 * i)) & 7];
}

float colorError(const RGB px[16], const RGB pal[4], uint8_t idx[16]) {
    float err = 0;
    for (int i = 0; i < 16; ++i) {
        float best = 1e30f; int bi = 0;
        for (int k = 0; k < 4; ++k) {
            float dr = px[i].r - pal[k].r, dg = px[i].g - pal[k].g, db = px[i].b - pal[k].b;
            float d = dr * dr * 0.30f + dg * dg * 0.59f + db * db * 0.11f;
            if (d < best) { best = d; bi = k; }
        }
        idx[i] = uint8_t(bi); err += best;
    }
    return err;
}

void palette(uint16_t c0, uint16_t c1, RGB pal[4]) {
    uint8_t a[3], b[3]; unpack565(c0, a); unpack565(c1, b);
    pal[0] = {float(a[0]), float(a[1]), float(a[2])};
    pal[1] = {float(b[0]), float(b[1]), float(b[2])};
    pal[2] = {(2 * pal[0].r + pal[1].r) / 3, (2 * pal[0].g + pal[1].g) / 3, (2 * pal[0].b + pal[1].b) / 3};
    pal[3] = {(pal[0].r + 2 * pal[1].r) / 3, (pal[0].g + 2 * pal[1].g) / 3, (pal[0].b + 2 * pal[1].b) / 3};
}

void encodeColorBlock(const RGB px[16], uint8_t *out) {
    // principal axis by power iteration
    RGB mean{0, 0, 0};
    for (int i = 0; i < 16; ++i) { mean.r += px[i].r; mean.g += px[i].g; mean.b += px[i].b; }
    mean.r /= 16; mean.g /= 16; mean.b /= 16;
    float cov[6] = {0, 0, 0, 0, 0, 0};
    for (int i = 0; i < 16; ++i) {
        float r = px[i].r - mean.r, g = px[i].g - mean.g, b = px[i].b - mean.b;
        cov[0] += r * r; cov[1] += r * g; cov[2] += r * b; cov[3] += g * g; cov[4] += g * b; cov[5] += b * b;
    }
    float ax[3] = {1, 1, 1};
    for (int it = 0; it < 8; ++it) {
        float x = cov[0] * ax[0] + cov[1] * ax[1] + cov[2] * ax[2];
        float y = cov[1] * ax[0] + cov[3] * ax[1] + cov[4] * ax[2];
        float z = cov[2] * ax[0] + cov[4] * ax[1] + cov[5] * ax[2];
        float n = std::max({std::fabs(x), std::fabs(y), std::fabs(z)});
        if (n < 1e-6f) break;
        ax[0] = x / n; ax[1] = y / n; ax[2] = z / n;
    }
    float mn = 1e30f, mx = -1e30f;
    for (int i = 0; i < 16; ++i) {
        float p = (px[i].r - mean.r) * ax[0] + (px[i].g - mean.g) * ax[1] + (px[i].b - mean.b) * ax[2];
        mn = std::min(mn, p); mx = std::max(mx, p);
    }
    float l2 = ax[0] * ax[0] + ax[1] * ax[1] + ax[2] * ax[2];
    if (l2 < 1e-12f) l2 = 1;
    RGB e0{mean.r + ax[0] * mx / l2, mean.g + ax[1] * mx / l2, mean.b + ax[2] * mx / l2};
    RGB e1{mean.r + ax[0] * mn / l2, mean.g + ax[1] * mn / l2, mean.b + ax[2] * mn / l2};

    uint16_t best0 = pack565(e0), best1 = pack565(e1);
    RGB pal[4]; uint8_t idx[16], bestIdx[16];
    palette(best0, best1, pal);
    float bestErr = colorError(px, pal, bestIdx);
    // least-squares refinement of the endpoints given the current assignment
    for (int iter = 0; iter < 3; ++iter) {
        static const float wa[4] = {1, 0, 2.0f / 3, 1.0f / 3};
        float aa = 0, ab = 0, bb = 0; RGB ax2{0, 0, 0}, bx{0, 0, 0};
        for (int i = 0; i < 16; ++i) {
            float a = wa[bestIdx[i]], b = 1 - a;
            aa += a * a; ab += a * b; bb += b * b;
            ax2.r += a * px[i].r; ax2.g += a * px[i].g; ax2.b += a * px[i].b;
            bx.r += b * px[i].r; bx.g += b * px[i].g; bx.b += b * px[i].b;
        }
        float det = aa * bb - ab * ab;
        if (std::fabs(det) < 1e-6f) break;
        RGB n0{(ax2.r * bb - bx.r * ab) / det, (ax2.g * bb - bx.g * ab) / det, (ax2.b * bb - bx.b * ab) / det};
        RGB n1{(bx.r * aa - ax2.r * ab) / det, (bx.g * aa - ax2.g * ab) / det, (bx.b * aa - ax2.b * ab) / det};
        uint16_t c0 = pack565(n0), c1 = pack565(n1);
        palette(c0, c1, pal);
        float err = colorError(px, pal, idx);
        if (err + 1e-3f < bestErr) { bestErr = err; best0 = c0; best1 = c1; std::memcpy(bestIdx, idx, 16); }
        else break;
    }
    // four-colour mode needs c0 > c1
    if (best0 < best1) {
        std::swap(best0, best1);
        for (auto &i : bestIdx) i = uint8_t(i == 0 ? 1 : i == 1 ? 0 : i == 2 ? 3 : 2);
    }
    if (best0 == best1) std::memset(bestIdx, 0, 16);
    std::memcpy(out, &best0, 2); std::memcpy(out + 2, &best1, 2);
    uint32_t bits = 0;
    for (int i = 0; i < 16; ++i) bits |= uint32_t(bestIdx[i]) << (2 * i);
    std::memcpy(out + 4, &bits, 4);
}

void encodeAlphaBlock(const uint8_t a[16], uint8_t *out) {
    uint8_t mn = 255, mx = 0;
    for (int i = 0; i < 16; ++i) { mn = std::min(mn, a[i]); mx = std::max(mx, a[i]); }
    out[0] = mx; out[1] = mn;
    uint8_t pal[8]; pal[0] = mx; pal[1] = mn;
    for (int i = 1; i < 7; ++i) pal[i + 1] = uint8_t(((7 - i) * mx + i * mn) / 7);
    uint64_t bits = 0;
    for (int i = 0; i < 16; ++i) {
        int best = 0, bd = 1 << 30;
        for (int k = 0; k < 8; ++k) { int d = std::abs(int(a[i]) - int(pal[k])); if (d < bd) { bd = d; best = k; } }
        if (mx == mn) best = 0;
        bits |= uint64_t(best) << (3 * i);
    }
    for (int i = 0; i < 6; ++i) out[2 + i] = uint8_t(bits >> (8 * i));
}

} // namespace

QImage decodeBc(const QByteArray &blocks, int w, int h, bool bc3) {
    QImage img(w, h, QImage::Format_RGBA8888);
    const int bw = (w + 3) / 4, bh = (h + 3) / 4, bs = bc3 ? 16 : 8;
    if (blocks.size() < qsizetype(bw) * bh * bs) { img.fill(Qt::magenta); return img; }
    const uint8_t *p = reinterpret_cast<const uint8_t *>(blocks.constData());
    for (int by = 0; by < bh; ++by)
        for (int bx = 0; bx < bw; ++bx) {
            const uint8_t *b = p + (size_t(by) * bw + bx) * bs;
            uint8_t px[16][4];
            if (bc3) { decodeColorBlock(b + 8, px, true); decodeAlphaBlock(b, px); }
            else decodeColorBlock(b, px, false);
            for (int y = 0; y < 4; ++y) {
                int yy = by * 4 + y; if (yy >= h) break;
                uchar *line = img.scanLine(yy);
                for (int x = 0; x < 4; ++x) { int xx = bx * 4 + x; if (xx >= w) break; std::memcpy(line + xx * 4, px[y * 4 + x], 4); }
            }
        }
    return img;
}

QByteArray encodeBc(const QImage &src, bool bc3) {
    QImage img = src.convertToFormat(QImage::Format_RGBA8888);
    const int w = img.width(), h = img.height();
    const int bw = (w + 3) / 4, bh = (h + 3) / 4, bs = bc3 ? 16 : 8;
    QByteArray out(qsizetype(bw) * bh * bs, '\0');
    std::vector<int> rows(static_cast<size_t>(bh));
    std::iota(rows.begin(), rows.end(), 0);
    QtConcurrent::blockingMap(rows, [&](int by) {
        for (int bx = 0; bx < bw; ++bx) {
            RGB px[16]; uint8_t a[16];
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 4; ++x) {
                    int xx = std::min(bx * 4 + x, w - 1), yy = std::min(by * 4 + y, h - 1);
                    const uchar *c = img.constScanLine(yy) + xx * 4;
                    px[y * 4 + x] = {float(c[0]), float(c[1]), float(c[2])};
                    a[y * 4 + x] = c[3];
                }
            uint8_t *o = reinterpret_cast<uint8_t *>(out.data()) + (size_t(by) * bw + bx) * bs;
            if (bc3) { encodeAlphaBlock(a, o); encodeColorBlock(px, o + 8); }
            else encodeColorBlock(px, o);
        }
    });
    return out;
}

} // namespace lol
