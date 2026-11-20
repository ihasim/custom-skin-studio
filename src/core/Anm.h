#pragma once
// Skeletal animation (.anm): r3d2anmd v4/v5 and compressed r3d2canm.
#include <QByteArray>
#include <QQuaternion>
#include <QVector3D>
#include <vector>

namespace lol {

struct Animation {
    struct Track {
        uint32_t jointHash = 0;
        std::vector<std::pair<float, QVector3D>> t, s;     // (seconds, value), sorted
        std::vector<std::pair<float, QQuaternion>> r;
    };
    float duration = 0;   // seconds
    float fps = 30;
    std::vector<Track> tracks;

    static Animation fromBytes(const QByteArray &data);
    // Local transform of one track at time `sec`; returns false for a missing channel.
    void sample(const Track &tr, float sec, QVector3D &t, QQuaternion &r, QVector3D &s) const;
};

} // namespace lol
