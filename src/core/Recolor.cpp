#include "core/Recolor.h"
#include <QtConcurrent/QtConcurrentMap>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

namespace lol {

void rgbToHsv(float r, float g, float b, float &h, float &s, float &v) {
    float mx = std::max({r, g, b}), mn = std::min({r, g, b}), d = mx - mn;
    v = mx; s = mx > 1e-6f ? d / mx : 0;
    if (d < 1e-6f) { h = 0; return; }
    if (mx == r) h = 60.0f * std::fmod((g - b) / d, 6.0f);
    else if (mx == g) h = 60.0f * ((b - r) / d + 2.0f);
    else h = 60.0f * ((r - g) / d + 4.0f);
    if (h < 0) h += 360.0f;
}

void hsvToRgb(float h, float s, float v, float &r, float &g, float &b) {
    h = std::fmod(h, 360.0f); if (h < 0) h += 360.0f;
    float c = v * s, x = c * (1 - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1)), m = v - c;
    float rr, gg, bb;
    if (h < 60) { rr = c; gg = x; bb = 0; } else if (h < 120) { rr = x; gg = c; bb = 0; }
    else if (h < 180) { rr = 0; gg = c; bb = x; } else if (h < 240) { rr = 0; gg = x; bb = c; }
    else if (h < 300) { rr = x; gg = 0; bb = c; } else { rr = c; gg = 0; bb = x; }
    r = rr + m; g = gg + m; b = bb + m;
}

namespace {

struct Lab { float L, a, b; };

inline float lin(float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); }

Lab toLab(const uchar *p) {
    static float table[256];
    static bool init = false;
    if (!init) { for (int i = 0; i < 256; ++i) table[i] = lin(i / 255.0f); init = true; }
    float r = table[p[0]], g = table[p[1]], b = table[p[2]];
    float x = (0.4124f * r + 0.3576f * g + 0.1805f * b) / 0.95047f;
    float y = (0.2126f * r + 0.7152f * g + 0.0722f * b);
    float z = (0.0193f * r + 0.1192f * g + 0.9505f * b) / 1.08883f;
    auto f = [](float t) { return t > 0.008856f ? std::cbrt(t) : 7.787f * t + 16.0f / 116.0f; };
    float fx = f(x), fy = f(y), fz = f(z);
    return {116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz)};
}

inline float d2(const Lab &a, const Lab &b) { float x = a.L - b.L, y = a.a - b.a, z = a.b - b.b; return x * x + y * y + z * z; }

} // namespace

std::vector<uint8_t> uvMask(const SkinnedMesh &m, const std::vector<int> &subs, int w, int h) {
    std::vector<uint8_t> mask(size_t(w) * h, 0);
    for (int si : subs) {
        const Submesh &s = m.submeshes[size_t(si)];
        for (uint32_t i = s.istart; i + 2 < s.istart + s.icount; i += 3) {
            QVector2D t[3];
            for (int k = 0; k < 3; ++k) { const QVector2D uv = m.uvs[m.indices[i + k]]; t[k] = {uv.x() * w, uv.y() * h}; }
            float minx = std::floor(std::min({t[0].x(), t[1].x(), t[2].x()})) - 2, maxx = std::ceil(std::max({t[0].x(), t[1].x(), t[2].x()})) + 2;
            float miny = std::floor(std::min({t[0].y(), t[1].y(), t[2].y()})) - 2, maxy = std::ceil(std::max({t[0].y(), t[1].y(), t[2].y()})) + 2;
            float area = (t[1].x() - t[0].x()) * (t[2].y() - t[0].y()) - (t[2].x() - t[0].x()) * (t[1].y() - t[0].y());
            if (std::fabs(area) < 1e-9f) continue;
            // conservative: grow triangle by ~1.5 texel through a tolerance on barycentrics
            for (int y = std::max(0, int(miny)); y <= std::min(h - 1, int(maxy)); ++y)
                for (int x = std::max(0, int(minx)); x <= std::min(w - 1, int(maxx)); ++x) {
                    float px = x + 0.5f, py = y + 0.5f;
                    float w0 = ((t[1].x() - px) * (t[2].y() - py) - (t[2].x() - px) * (t[1].y() - py)) / area;
                    float w1 = ((t[2].x() - px) * (t[0].y() - py) - (t[0].x() - px) * (t[2].y() - py)) / area;
                    float w2 = 1 - w0 - w1;
                    const float tol = 1.5f / std::sqrt(std::fabs(area));
                    if (w0 >= -tol && w1 >= -tol && w2 >= -tol) mask[size_t(y) * w + x] = 1;
                }
        }
    }
    return mask;
}

RegionMap RegionMap::build(const QImage &src, const std::vector<uint8_t> &used, int k) {
    QImage img = src.convertToFormat(QImage::Format_RGBA8888);
    RegionMap rm; rm.w = img.width(); rm.h = img.height();
    const size_t N = size_t(rm.w) * rm.h;
    rm.l1.assign(N, 255); rm.l2.assign(N, 255); rm.mix.assign(N, 255);
    std::vector<size_t> idx;
    for (size_t i = 0; i < N; ++i) if (used.empty() || used[i]) idx.push_back(i);
    if (idx.empty()) return rm;
    // sample, k-means++ init, Lloyd iterations
    std::mt19937 rng(1907);
    std::vector<Lab> sample;
    const size_t want = std::min<size_t>(60000, idx.size());
    for (size_t s = 0; s < want; ++s) {
        size_t i = idx[size_t(double(s) / want * idx.size())];
        sample.push_back(toLab(img.constBits() + i * 4));
    }
    k = std::max(1, std::min(k, int(sample.size())));
    std::vector<Lab> c;
    c.push_back(sample[rng() % sample.size()]);
    std::vector<float> dist(sample.size(), 1e30f);
    while (int(c.size()) < k) {
        double total = 0;
        for (size_t i = 0; i < sample.size(); ++i) { dist[i] = std::min(dist[i], d2(sample[i], c.back())); total += dist[i]; }
        double r = std::uniform_real_distribution<double>(0, total)(rng), acc = 0;
        size_t pick = 0;
        for (size_t i = 0; i < sample.size(); ++i) { acc += dist[i]; if (acc >= r) { pick = i; break; } }
        c.push_back(sample[pick]);
    }
    std::vector<int> lab(sample.size(), 0);
    for (int it = 0; it < 15; ++it) {
        std::vector<double> sL(size_t(k), 0), sa(size_t(k), 0), sb(size_t(k), 0); std::vector<int> n(size_t(k), 0);
        for (size_t i = 0; i < sample.size(); ++i) {
            int best = 0; float bd = 1e30f;
            for (int j = 0; j < k; ++j) { float d = d2(sample[i], c[size_t(j)]); if (d < bd) { bd = d; best = j; } }
            lab[i] = best; sL[size_t(best)] += sample[i].L; sa[size_t(best)] += sample[i].a; sb[size_t(best)] += sample[i].b; n[size_t(best)]++;
        }
        for (int j = 0; j < k; ++j) if (n[size_t(j)]) c[size_t(j)] = {float(sL[size_t(j)] / n[size_t(j)]), float(sa[size_t(j)] / n[size_t(j)]), float(sb[size_t(j)] / n[size_t(j)])};
    }
    // assign every used texel: two nearest centroids, soft weight
    const float sigma2 = 2.0f * 7.0f * 7.0f;
    std::vector<double> sumR(size_t(k), 0), sumG(size_t(k), 0), sumB(size_t(k), 0); std::vector<size_t> cnt(size_t(k), 0);
    for (size_t i : idx) {
        const uchar *p = img.constBits() + i * 4;
        Lab x = toLab(p);
        int b1 = 0, b2 = 0; float d1 = 1e30f, dd2 = 1e30f;
        for (int j = 0; j < k; ++j) {
            float d = d2(x, c[size_t(j)]);
            if (d < d1) { dd2 = d1; b2 = b1; d1 = d; b1 = j; } else if (d < dd2) { dd2 = d; b2 = j; }
        }
        if (k == 1) b2 = b1, dd2 = d1;
        float w1 = 1.0f / (1.0f + std::exp((d1 - dd2) / sigma2));
        rm.l1[i] = uint8_t(b1); rm.l2[i] = uint8_t(b2); rm.mix[i] = uint8_t(std::lround(w1 * 255));
        sumR[size_t(b1)] += p[0]; sumG[size_t(b1)] += p[1]; sumB[size_t(b1)] += p[2]; cnt[size_t(b1)]++;
    }
    // order regions by size (largest first) and relabel
    std::vector<int> order(static_cast<size_t>(k)); std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) { return cnt[size_t(a)] > cnt[size_t(b)]; });
    std::vector<uint8_t> remap(256, 255);
    for (int j = 0; j < k; ++j) remap[size_t(order[size_t(j)])] = uint8_t(j);
    for (size_t i : idx) { rm.l1[i] = remap[rm.l1[i]]; rm.l2[i] = remap[rm.l2[i]]; }
    for (int j = 0; j < k; ++j) {
        int o = order[size_t(j)];
        ColorRegion r;
        double n = std::max<size_t>(1, cnt[size_t(o)]);
        r.mean = QColor(int(sumR[size_t(o)] / n), int(sumG[size_t(o)] / n), int(sumB[size_t(o)] / n));
        rgbToHsv(r.mean.redF(), r.mean.greenF(), r.mean.blueF(), r.meanHue, r.meanSat, r.meanVal);
        r.share = double(cnt[size_t(o)]) / double(idx.size());
        r.name = colorName(r.mean);
        rm.regions.push_back(r);
    }
    // unique names ("Mavi", "Mavi 2")
    std::vector<QString> base;
    for (const auto &r : rm.regions) base.push_back(r.name);
    for (size_t j = 0; j < rm.regions.size(); ++j) {
        int same = 0;
        for (size_t q = 0; q < j; ++q) if (base[q] == base[j]) ++same;
        if (same) rm.regions[j].name += " " + QString::number(same + 1);
    }
    return rm;
}

int RegionMap::regionAt(int x, int y) const {
    if (x < 0 || y < 0 || x >= w || y >= h) return -1;
    int best = -1, bestDist = 1 << 30;
    // nearest used texel in a small window (clicks can land on seams)
    for (int r = 0; r <= 6 && best < 0; ++r)
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                int xx = x + dx, yy = y + dy;
                if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                uint8_t l = l1[size_t(yy) * w + xx];
                if (l != 255 && dx * dx + dy * dy < bestDist) { bestDist = dx * dx + dy * dy; best = l; }
            }
    return best;
}

QString colorName(const QColor &c) {
    float h, s, v; rgbToHsv(c.redF(), c.greenF(), c.blueF(), h, s, v);
    if (v < 0.16f) return "Siyah";
    if (s < 0.14f) return v > 0.8f ? "Beyaz" : v > 0.45f ? "Gri" : "Koyu gri";
    QString base;
    if (h < 15 || h >= 345) base = "Kırmızı";
    else if (h < 40) base = (v < 0.6f) ? "Kahverengi" : (s < 0.55f ? "Ten rengi" : "Turuncu");
    else if (h < 65) base = (v < 0.55f) ? "Koyu altın" : "Altın";
    else if (h < 160) base = "Yeşil";
    else if (h < 200) base = "Turkuaz";
    else if (h < 255) base = "Mavi";
    else if (h < 290) base = "Mor";
    else base = "Pembe";
    if (v < 0.4f && base != "Kahverengi" && base != "Koyu altın") base = "Koyu " + base.toLower();
    return base;
}

static inline void adjustPixel(const uchar *in, uchar *out, const RegionAdjust &a, const ColorRegion &r) {
    float h, s, v;
    rgbToHsv(in[0] / 255.0f, in[1] / 255.0f, in[2] / 255.0f, h, s, v);
    h += a.hue;
    s = std::clamp(s * a.sat, 0.0f, 1.0f);
    v = r.meanVal + (v - r.meanVal) * a.contrast;
    v = std::clamp(v * a.val, 0.0f, 1.0f);
    float rr, gg, bb; hsvToRgb(h, s, v, rr, gg, bb);
    out[0] = uchar(std::lround(rr * 255)); out[1] = uchar(std::lround(gg * 255)); out[2] = uchar(std::lround(bb * 255)); out[3] = in[3];
}

QImage applyRegions(const QImage &original, const RegionMap &map, const std::vector<RegionAdjust> &adj, int highlight) {
    QImage src = original.convertToFormat(QImage::Format_RGBA8888);
    QImage out = src.copy();
    bool any = highlight >= 0;
    for (const auto &a : adj) any = any || !a.isIdentity();
    if (!any || map.w != src.width() || map.h != src.height()) return out;
    std::vector<int> rows(static_cast<size_t>(map.h)); std::iota(rows.begin(), rows.end(), 0);
    QtConcurrent::blockingMap(rows, [&](int y) {
        const uchar *in = src.constScanLine(y);
        uchar *o = out.scanLine(y);
        for (int x = 0; x < map.w; ++x) {
            size_t i = size_t(y) * map.w + x;
            uint8_t l1 = map.l1[i];
            if (l1 == 255) continue;
            uint8_t l2 = map.l2[i];
            const RegionAdjust &a1 = adj[l1], &a2 = adj[l2];
            if (a1.isIdentity() && a2.isIdentity() && highlight < 0) continue;
            uchar c1[4], c2[4];
            if (a1.isIdentity()) std::memcpy(c1, in + x * 4, 4); else adjustPixel(in + x * 4, c1, a1, map.regions[l1]);
            if (l2 == l1) std::memcpy(c2, c1, 4);
            else if (a2.isIdentity()) std::memcpy(c2, in + x * 4, 4); else adjustPixel(in + x * 4, c2, a2, map.regions[l2]);
            float w = map.mix[i] / 255.0f;
            float hl = 0;
            if (highlight >= 0) hl = 0.55f * ((l1 == highlight ? w : 0) + (l2 == highlight && l2 != l1 ? 1 - w : 0));
            static const float accent[3] = {45, 212, 191};
            for (int k = 0; k < 3; ++k) {
                float v = c1[k] * w + c2[k] * (1 - w);
                v = v * (1 - hl) + accent[k] * hl;
                o[x * 4 + k] = uchar(std::clamp(std::lround(v), 0L, 255L));
            }
        }
    });
    return out;
}

} // namespace lol
