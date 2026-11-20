#include "app/SkinGeometry.h"
#include "app/AppController.h"
#include <unordered_map>

namespace {
struct Vtx { float px, py, pz, nx, ny, nz, u, v; };
}

SkinGeometry::SkinGeometry(QQuick3DObject *parent) : QQuick3DGeometry(parent) {
    if (auto *c = AppController::instance()) {
        connect(c, &AppController::poseChanged, this, &SkinGeometry::pose);
        connect(c, &AppController::projectChanged, this, &SkinGeometry::rebuild);
    }
}

void SkinGeometry::setPart(int p) {
    if (p == part_) return;
    part_ = p;
    emit partChanged();
    rebuild();
}

void SkinGeometry::rebuild() {
    clear();
    verts_.clear();
    AppController *c = AppController::instance();
    const lol::SkinProject *pr = c ? c->project() : nullptr;
    if (!pr || part_ < 0 || part_ >= int(pr->parts.size())) { update(); return; }
    const auto &m = pr->mesh;
    const QStringList subs = pr->parts[size_t(part_)].submesh.split(", ");
    std::unordered_map<uint32_t, uint32_t> local;
    std::vector<uint32_t> idx;
    for (const auto &s : m.submeshes) {
        if (!subs.contains(s.name)) continue;
        for (uint32_t i = s.istart; i + 2 < s.istart + s.icount; i += 3) {
            uint32_t tri[3] = {m.indices[i], m.indices[i + 2], m.indices[i + 1]};   // reversed winding (X mirror)
            for (uint32_t g : tri) {
                auto it = local.find(g);
                if (it == local.end()) { it = local.emplace(g, uint32_t(verts_.size())).first; verts_.push_back(g); }
                idx.push_back(it->second);
            }
        }
    }
    QByteArray ib(reinterpret_cast<const char *>(idx.data()), qsizetype(idx.size() * 4));
    setIndexData(ib);
    setStride(sizeof(Vtx));
    setPrimitiveType(PrimitiveType::Triangles);
    addAttribute(Attribute::PositionSemantic, 0, Attribute::F32Type);
    addAttribute(Attribute::NormalSemantic, 12, Attribute::F32Type);
    addAttribute(Attribute::TexCoord0Semantic, 24, Attribute::F32Type);
    addAttribute(Attribute::IndexSemantic, 0, Attribute::U32Type);
    pose();
}

void SkinGeometry::pose() {
    AppController *c = AppController::instance();
    const lol::SkinProject *pr = c ? c->project() : nullptr;
    if (!pr || verts_.empty()) return;
    const auto &m = pr->mesh;
    const auto &skin = c->skinMatrices();
    buf_.resize(qsizetype(verts_.size() * sizeof(Vtx)));
    auto *out = reinterpret_cast<Vtx *>(buf_.data());
    QVector3D lo(1e9f, 1e9f, 1e9f), hi(-1e9f, -1e9f, -1e9f);
    for (size_t k = 0; k < verts_.size(); ++k) {
        const uint32_t g = verts_[k];
        QVector3D p = m.positions[g], n = m.normals.empty() ? QVector3D(0, 1, 0) : m.normals[g];
        if (!skin.empty()) {
            QVector3D sp, sn;
            const auto &b = m.bones[g];
            const auto &w = m.weights[g];
            float tw = 0;
            for (int i = 0; i < 4; ++i) {
                if (w[size_t(i)] <= 0 || b[size_t(i)] >= skin.size()) continue;
                const QMatrix4x4 &M = skin[b[size_t(i)]];
                sp += w[size_t(i)] * M.map(p);
                sn += w[size_t(i)] * M.mapVector(n);
                tw += w[size_t(i)];
            }
            if (tw > 0) { p = sp / tw; n = sn.normalized(); }
        }
        const QVector2D uv = m.uvs[g];
        out[k] = Vtx{-p.x(), p.y(), p.z(), -n.x(), n.y(), n.z(), uv.x(), uv.y()};
        lo.setX(std::min(lo.x(), -p.x())); lo.setY(std::min(lo.y(), p.y())); lo.setZ(std::min(lo.z(), p.z()));
        hi.setX(std::max(hi.x(), -p.x())); hi.setY(std::max(hi.y(), p.y())); hi.setZ(std::max(hi.z(), p.z()));
    }
    setVertexData(buf_);
    setBounds(lo, hi);
    update();
}

// ------------------------------------------------------------------

SkinTexture::SkinTexture(QQuick3DObject *parent) : QQuick3DTextureData(parent) {
    if (auto *c = AppController::instance()) {
        connect(c, &AppController::partTextureChanged, this, [this](int p) { if (p == part_) refresh(); });
        connect(c, &AppController::projectChanged, this, &SkinTexture::refresh);
    }
}

void SkinTexture::setPart(int p) {
    if (p == part_) return;
    part_ = p;
    emit partChanged();
    refresh();
}

void SkinTexture::refresh() {
    AppController *c = AppController::instance();
    if (!c || part_ < 0) return;
    QImage img = c->image(QString("part/%1").arg(part_));
    if (img.isNull()) return;
    img = img.convertToFormat(QImage::Format_RGBA8888);
    setSize(img.size());
    setFormat(Format::RGBA8);
    setHasTransparency(false);
    setTextureData(QByteArray(reinterpret_cast<const char *>(img.constBits()), img.sizeInBytes()));
}
