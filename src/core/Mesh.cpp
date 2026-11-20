#include "core/Mesh.h"
#include "core/Hashes.h"

namespace lol {

QMatrix4x4 trs(const QVector3D &t, const QQuaternion &r, const QVector3D &s) {
    QMatrix4x4 m;
    m.translate(t);
    m.rotate(r);
    m.scale(s);
    return m;
}

static QVector3D v3(Reader &r) { float x = r.get<float>(), y = r.get<float>(), z = r.get<float>(); return {x, y, z}; }
static QQuaternion q4(Reader &r) { float x = r.get<float>(), y = r.get<float>(), z = r.get<float>(), w = r.get<float>(); return QQuaternion(w, x, y, z); }

SkinnedMesh SkinnedMesh::fromSkn(const QByteArray &data) {
    Reader r(data);
    if (r.get<uint32_t>() != 0x00112233u) throw FormatError("not an SKN file");
    uint16_t major = r.get<uint16_t>(); r.get<uint16_t>();
    SkinnedMesh m;
    uint32_t icount = 0, vcount = 0, vsize = 52, vtype = 0;
    if (major == 0) {
        icount = r.get<uint32_t>(); vcount = r.get<uint32_t>();
        m.submeshes.push_back({"Base", 0, vcount, 0, icount});
    } else {
        uint32_t sc = r.get<uint32_t>();
        for (uint32_t i = 0; i < sc; ++i) {
            Submesh s;
            QByteArray name = r.bytes(64);
            s.name = QString::fromUtf8(name.left(name.indexOf('\0') < 0 ? 64 : name.indexOf('\0')));
            s.vstart = r.get<uint32_t>(); s.vcount = r.get<uint32_t>(); s.istart = r.get<uint32_t>(); s.icount = r.get<uint32_t>();
            m.submeshes.push_back(s);
        }
        if (major == 4) r.get<uint32_t>();
        icount = r.get<uint32_t>(); vcount = r.get<uint32_t>();
        if (major == 4) {
            vsize = r.get<uint32_t>(); vtype = r.get<uint32_t>();
            r.skip(24 + 16);   // bounding box + sphere
        }
    }
    (void)vtype;
    m.indices.resize(icount);
    for (auto &i : m.indices) i = r.get<uint16_t>();
    m.positions.resize(vcount); m.normals.resize(vcount); m.uvs.resize(vcount); m.bones.resize(vcount); m.weights.resize(vcount);
    for (uint32_t i = 0; i < vcount; ++i) {
        int start = r.pos();
        m.positions[i] = v3(r);
        for (int k = 0; k < 4; ++k) m.bones[i][k] = r.get<uint8_t>();
        for (int k = 0; k < 4; ++k) m.weights[i][k] = r.get<float>();
        m.normals[i] = v3(r);
        float u = r.get<float>(), v = r.get<float>();
        m.uvs[i] = {u, v};
        r.seek(start + int(vsize));
    }
    return m;
}

Skeleton Skeleton::fromSkl(const QByteArray &data) {
    Reader r(data);
    r.skip(4);
    if (r.get<uint32_t>() != 0x22FD4FC3u) throw FormatError("unsupported (legacy) SKL skeleton");
    r.seek(8);
    if (r.get<uint32_t>() != 0) throw FormatError("unsupported SKL version");
    r.get<uint16_t>();
    uint16_t jc = r.get<uint16_t>();
    uint32_t ic = r.get<uint32_t>();
    int32_t jointsOff = r.get<int32_t>(); r.get<int32_t>();
    int32_t inflOff = r.get<int32_t>();
    Skeleton s;
    r.seek(jointsOff);
    s.joints.resize(jc);
    for (auto &j : s.joints) {
        r.get<uint16_t>();                 // flags
        r.get<int16_t>();                  // id
        j.parent = r.get<int16_t>();
        r.skip(2);
        j.hash = r.get<uint32_t>();
        r.get<float>();                    // radius
        j.localT = v3(r); j.localS = v3(r); j.localR = q4(r).normalized();
        QVector3D it = v3(r), is = v3(r); QQuaternion ir = q4(r).normalized();
        j.inverseBind = trs(it, ir, is);
        int nameAt = r.pos();
        int32_t rel = r.get<int32_t>();
        int back = r.pos();
        r.seek(nameAt + rel);
        j.name = QString::fromStdString(r.cstr());
        r.seek(back);
    }
    if (inflOff > 0) {
        r.seek(inflOff);
        s.influences.resize(ic);
        for (auto &i : s.influences) i = r.get<uint16_t>();
    }
    return s;
}

int Skeleton::indexOfHash(uint32_t h) const {
    for (size_t i = 0; i < joints.size(); ++i) if (joints[i].hash == h) return int(i);
    return -1;
}

} // namespace lol
