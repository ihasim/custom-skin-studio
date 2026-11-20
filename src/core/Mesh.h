#pragma once
// Skinned mesh (.skn) and skeleton (.skl).
#include <QByteArray>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QString>
#include <QVector2D>
#include <QVector3D>
#include <array>
#include <vector>

namespace lol {

struct Submesh {
    QString name;
    uint32_t vstart = 0, vcount = 0, istart = 0, icount = 0;
};

struct SkinnedMesh {
    std::vector<Submesh> submeshes;
    std::vector<uint16_t> indices;
    std::vector<QVector3D> positions, normals;
    std::vector<QVector2D> uvs;
    std::vector<std::array<uint8_t, 4>> bones;   // indices into Skeleton::influences
    std::vector<std::array<float, 4>> weights;

    static SkinnedMesh fromSkn(const QByteArray &data);
};

struct Joint {
    QString name;
    int16_t parent = -1;
    uint32_t hash = 0;
    QVector3D localT, localS{1, 1, 1};
    QQuaternion localR;
    QMatrix4x4 inverseBind;
};

struct Skeleton {
    std::vector<Joint> joints;
    std::vector<uint16_t> influences;   // SKN bone index -> joint index
    static Skeleton fromSkl(const QByteArray &data);
    int indexOfHash(uint32_t h) const;
};

QMatrix4x4 trs(const QVector3D &t, const QQuaternion &r, const QVector3D &s);

} // namespace lol
