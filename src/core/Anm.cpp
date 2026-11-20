#include "core/Anm.h"
#include "core/Hashes.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace lol {

static QQuaternion decompressQuat(const uint8_t *b) {
    uint64_t bits = uint64_t(b[0]) | uint64_t(b[1]) << 8 | uint64_t(b[2]) << 16 | uint64_t(b[3]) << 24 |
                    uint64_t(b[4]) << 32 | uint64_t(b[5]) << 40;
    const int maxIndex = int((bits >> 45) & 3);
    const double k = 1.41421356237 / 32767.0, off = 0.70710678118;
    double a = double((bits >> 30) & 32767) * k - off;
    double bb = double((bits >> 15) & 32767) * k - off;
    double c = double(bits & 32767) * k - off;
    double d = std::sqrt(std::max(0.0, 1.0 - (a * a + bb * bb + c * c)));
    double x, y, z, w;
    switch (maxIndex) {
    case 0: x = d; y = a; z = bb; w = c; break;
    case 1: x = a; y = d; z = bb; w = c; break;
    case 2: x = a; y = bb; z = d; w = c; break;
    default: x = a; y = bb; z = c; w = d; break;
    }
    return QQuaternion(float(w), float(x), float(y), float(z));
}

static QVector3D readV3(Reader &r) { float x = r.get<float>(), y = r.get<float>(), z = r.get<float>(); return {x, y, z}; }

Animation Animation::fromBytes(const QByteArray &data) {
    Reader r(data);
    QByteArray magic = r.bytes(8);
    uint32_t version = r.get<uint32_t>();
    Animation a;
    if (magic == "r3d2canm") {
        r.skip(12);                                   // file size, format token, flags
        uint32_t trackCount = r.get<uint32_t>(), frameCount = r.get<uint32_t>();
        r.skip(4);                                    // jump cache count
        float maxTime = r.get<float>(); a.fps = r.get<float>();
        r.skip(24);                                   // error metrics
        QVector3D tmin = readV3(r), tmax = readV3(r), smin = readV3(r), smax = readV3(r);
        int32_t framesOff = r.get<int32_t>(); r.get<int32_t>(); int32_t hashesOff = r.get<int32_t>();
        if (framesOff <= 0 || hashesOff <= 0) throw FormatError("ANM without frames");
        r.seek(hashesOff + 12);
        a.tracks.resize(trackCount);
        for (auto &t : a.tracks) t.jointHash = r.get<uint32_t>();
        r.seek(framesOff + 12);
        a.duration = maxTime;
        auto dq = [](const QVector3D &mn, const QVector3D &mx, const uint8_t *b) {
            auto u = [&](int i) { return float(b[i] | (b[i + 1] << 8)) / 65535.0f; };
            return QVector3D(mn.x() + (mx.x() - mn.x()) * u(0), mn.y() + (mx.y() - mn.y()) * u(2), mn.z() + (mx.z() - mn.z()) * u(4));
        };
        for (uint32_t i = 0; i < frameCount; ++i) {
            uint16_t ct = r.get<uint16_t>(), bits = r.get<uint16_t>();
            const uint8_t *v = reinterpret_cast<const uint8_t *>(r.ptr());
            r.skip(6);
            uint32_t ti = bits & 0x3FFF;
            if (ti >= trackCount) continue;
            float sec = float(ct) / 65535.0f * maxTime;
            Track &tr = a.tracks[ti];
            switch (bits >> 14) {
            case 0: tr.r.push_back({sec, decompressQuat(v)}); break;
            case 1: tr.t.push_back({sec, dq(tmin, tmax, v)}); break;
            case 2: tr.s.push_back({sec, dq(smin, smax, v)}); break;
            default: break;
            }
        }
    } else if (magic == "r3d2anmd" && (version == 4 || version == 5)) {
        r.skip(16);
        uint32_t trackCount = r.get<uint32_t>(), frameCount = r.get<uint32_t>();
        float frameDur = r.get<float>();
        a.fps = frameDur > 0 ? 1.0f / frameDur : 30.0f;
        a.duration = frameCount > 1 ? float(frameCount - 1) * frameDur : frameDur;
        int32_t hashesOff = r.get<int32_t>(); r.skip(8);
        int32_t vecsOff = r.get<int32_t>(), quatsOff = r.get<int32_t>(), framesOff = r.get<int32_t>();
        std::vector<QVector3D> vecs; std::vector<QQuaternion> quats;
        if (version == 5) {
            int vc = (quatsOff - vecsOff) / 12, qc = (hashesOff - quatsOff) / 6, hc = (framesOff - hashesOff) / 4;
            r.seek(vecsOff + 12); for (int i = 0; i < vc; ++i) vecs.push_back(readV3(r));
            r.seek(quatsOff + 12);
            for (int i = 0; i < qc; ++i) { quats.push_back(decompressQuat(reinterpret_cast<const uint8_t *>(r.ptr()))); r.skip(6); }
            r.seek(hashesOff + 12);
            a.tracks.resize(trackCount);
            for (int i = 0; i < hc && i < int(trackCount); ++i) a.tracks[size_t(i)].jointHash = r.get<uint32_t>();
            r.seek(framesOff + 12);
            for (uint32_t f = 0; f < frameCount; ++f)
                for (auto &tr : a.tracks) {
                    uint16_t ti = r.get<uint16_t>(), si = r.get<uint16_t>(), ri = r.get<uint16_t>();
                    float sec = float(f) * frameDur;
                    if (ti < vecs.size()) tr.t.push_back({sec, vecs[ti]});
                    if (si < vecs.size()) tr.s.push_back({sec, vecs[si]});
                    if (ri < quats.size()) tr.r.push_back({sec, quats[ri]});
                }
        } else {
            int vc = (quatsOff - vecsOff) / 12, qc = (framesOff - quatsOff) / 16;
            r.seek(vecsOff + 12); for (int i = 0; i < vc; ++i) vecs.push_back(readV3(r));
            r.seek(quatsOff + 12);
            for (int i = 0; i < qc; ++i) { float x = r.get<float>(), y = r.get<float>(), z = r.get<float>(), w = r.get<float>(); quats.push_back(QQuaternion(w, x, y, z)); }
            r.seek(framesOff + 12);
            std::map<uint32_t, size_t> byHash;
            for (uint32_t f = 0; f < frameCount; ++f)
                for (uint32_t t = 0; t < trackCount; ++t) {
                    uint32_t h = r.get<uint32_t>();
                    uint16_t ti = r.get<uint16_t>(), si = r.get<uint16_t>(), ri = r.get<uint16_t>(); r.skip(2);
                    auto it = byHash.find(h);
                    if (it == byHash.end()) { a.tracks.push_back({}); a.tracks.back().jointHash = h; it = byHash.emplace(h, a.tracks.size() - 1).first; }
                    Track &tr = a.tracks[it->second];
                    float sec = float(f) * frameDur;
                    if (ti < vecs.size()) tr.t.push_back({sec, vecs[ti]});
                    if (si < vecs.size()) tr.s.push_back({sec, vecs[si]});
                    if (ri < quats.size()) tr.r.push_back({sec, quats[ri]});
                }
        }
    } else {
        throw FormatError("unsupported ANM format");
    }
    for (auto &tr : a.tracks) {
        auto byTime = [](const auto &x, const auto &y) { return x.first < y.first; };
        std::stable_sort(tr.t.begin(), tr.t.end(), byTime);
        std::stable_sort(tr.s.begin(), tr.s.end(), byTime);
        std::stable_sort(tr.r.begin(), tr.r.end(), byTime);
    }
    return a;
}

template <class V, class Mix>
static bool sampleKeys(const std::vector<std::pair<float, V>> &keys, float sec, V &out, Mix mix) {
    if (keys.empty()) return false;
    if (sec <= keys.front().first) { out = keys.front().second; return true; }
    if (sec >= keys.back().first) { out = keys.back().second; return true; }
    auto it = std::upper_bound(keys.begin(), keys.end(), sec, [](float s, const auto &k) { return s < k.first; });
    const auto &b = *it, &a = *(it - 1);
    float span = b.first - a.first;
    float f = span > 1e-6f ? (sec - a.first) / span : 0.0f;
    out = mix(a.second, b.second, f);
    return true;
}

void Animation::sample(const Track &tr, float sec, QVector3D &t, QQuaternion &r, QVector3D &s) const {
    sampleKeys(tr.t, sec, t, [](const QVector3D &x, const QVector3D &y, float f) { return x + (y - x) * f; });
    sampleKeys(tr.s, sec, s, [](const QVector3D &x, const QVector3D &y, float f) { return x + (y - x) * f; });
    sampleKeys(tr.r, sec, r, [](const QQuaternion &x, const QQuaternion &y, float f) { return QQuaternion::nlerp(x, y, f); });
}

} // namespace lol
