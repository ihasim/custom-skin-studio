#include "core/Vfx.h"
#include "core/Hashes.h"
#include "core/Recolor.h"
#include <QRegularExpression>
#include <QStringList>
#include <algorithm>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace lol {

namespace {

const uint32_t kVfxSystem = fnv1a32("VfxSystemDefinitionData");
const uint32_t kValueColor = fnv1a32("ValueColor");
const uint32_t kParticleName = fnv1a32("particleName");

struct GroupDef { const char *key; const char *label; };
const GroupDef kGroups[] = {
    {"P", "Pasif"}, {"Q", "Q"}, {"W", "W"}, {"E", "E"}, {"R", "R (ulti)"},
    {"A", "Temel saldırı"}, {"I", "Bekleme parıltısı"}, {"O", "Diğer efektler"},
};

QString classify(const QString &particleName) {
    const QStringList toks = particleName.toLower().split(QRegularExpression("[_\\s\\.]+"), Qt::SkipEmptyParts);
    for (const QString &t : toks) {
        if (t == "q") return "Q";
        if (t == "w") return "W";
        if (t == "e") return "E";
        if (t == "r") return "R";
        if (t == "p" || t == "passive") return "P";
        if (t == "aa" || t == "ba" || t == "crit" || t == "basicattack" || t == "critattack") return "A";
        if (t == "idle") return "I";
    }
    return "O";
}

bool isTexturePath(const QString &s) {
    const QString l = s.toLower();
    return l.endsWith(".tex") || l.endsWith(".dds");
}

void collectColors(BinValue &v, bool inColor, const std::function<void(BinValue &)> &fn) {
    const bool here = inColor || (v.isStruct() && v.cls == kValueColor);
    if (here && v.type == BinType::Vec4) fn(v);
    for (auto &f : v.fields) collectColors(f.value, here, fn);
    for (auto &it : v.items) collectColors(it, here, fn);
}

void forEachColor(BinEntry &e, const std::function<void(BinValue &)> &fn) {
    for (auto &f : e.fields) collectColors(f.value, false, fn);
}

void forEachString(BinEntry &e, const std::function<void(BinValue &)> &fn) {
    visitEntry(e, [&](BinValue &v, uint32_t) { if (v.type == BinType::String) fn(v); });
}

inline float smooth(float a, float b, float x) { float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f); return t * t * (3 - 2 * t); }

} // namespace

QColor VfxGroup::current() const {
    if (!hasColor) return QColor(160, 160, 160);
    float r, g, b; hsvToRgb(themeHue, std::clamp(themeSat, 0.0f, 1.0f), 0.92f, r, g, b);
    return QColor::fromRgbF(r, g, b);
}

void VfxEditor::analyze(BinFile &bin, const TextureLoader &load) {
    groups_.clear();
    std::map<QString, VfxGroup> byKey;
    for (const auto &d : kGroups) { VfxGroup g; g.key = d.key; g.label = d.label; byKey[g.key] = g; }

    // idle effects: SkinCharacterDataProperties.idleParticlesEffects[].effectKey -> ResourceResolver.resourceMap
    std::set<uint32_t> idleEntries;
    {
        std::set<uint32_t> idleKeys;
        for (auto &e : bin.entries)
            if (BinField *f = e.field(fnv1a32("idleParticlesEffects")))
                for (auto &it : f->value.items)
                    if (const BinField *k = it.field(fnv1a32("effectKey"))) idleKeys.insert(k->value.u32());
        for (auto &e : bin.entries)
            if (BinField *f = e.field(fnv1a32("resourceMap")))
                for (size_t i = 0; i < f->value.keys.size(); ++i)
                    if (idleKeys.count(f->value.keys[i].u32())) idleEntries.insert(f->value.items[i].u32());
    }

    for (auto &e : bin.entries) {
        if (e.cls != kVfxSystem) continue;
        QString name;
        if (BinField *f = e.field(kParticleName)) name = f->value.string();
        QString key = idleEntries.count(e.name) ? "I" : classify(name);
        VfxGroup &g = byKey[key];
        g.entries.push_back(e.name);
        if (!name.isEmpty() && g.detail.count(',') < 2) g.detail += (g.detail.isEmpty() ? "" : ", ") + QString(name).replace('_', ' ');
        forEachString(e, [&](BinValue &v) { if (isTexturePath(v.string())) g.textures.insert(v.string().toLower()); });
    }

    for (const auto &d : kGroups) {
        VfxGroup g = byKey[d.key];
        if (g.entries.empty()) continue;
        // main hue: circular mean over saturated colours (ValueColor values + texture pixels, equal total weight)
        double cx = 0, cy = 0, sw = 0, ss = 0;
        double tcx = 0, tcy = 0, tsw = 0, tss = 0;
        for (uint32_t en : g.entries) {
            BinEntry *e = bin.entry(en);
            if (!e) continue;
            forEachColor(*e, [&](BinValue &v) {
                QVector4D c = v.vec4();
                float m = std::max({c.x(), c.y(), c.z(), 1e-6f});
                float h, s, val; rgbToHsv(c.x() / std::max(m, 1.0f), c.y() / std::max(m, 1.0f), c.z() / std::max(m, 1.0f), h, s, val);
                if (s < 0.25f || val < 0.2f) return;
                double w = s * val;
                cx += w * std::cos(h * M_PI / 180); cy += w * std::sin(h * M_PI / 180); sw += w; ss += w * s;
            });
        }
        for (const QString &t : g.textures) {
            QImage img = load(t);
            if (img.isNull()) continue;
            QImage small = img.scaled(48, 48, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGBA8888);
            for (int y = 0; y < small.height(); ++y) {
                const uchar *p = small.constScanLine(y);
                for (int x = 0; x < small.width(); ++x, p += 4) {
                    float h, s, val; rgbToHsv(p[0] / 255.f, p[1] / 255.f, p[2] / 255.f, h, s, val);
                    if (s < 0.3f || val < 0.25f || p[3] < 40) continue;
                    double w = s * val * (p[3] / 255.0);
                    tcx += w * std::cos(h * M_PI / 180); tcy += w * std::sin(h * M_PI / 180); tsw += w; tss += w * s;
                }
            }
        }
        if (sw > 0 && tsw > 0) { double k = sw / tsw; tcx *= k; tcy *= k; tss *= k; tsw = sw; }
        cx += tcx; cy += tcy; sw += tsw; ss += tss;
        if (sw > 1e-6) {
            g.hasColor = true;
            g.themeHue = float(std::fmod(std::atan2(cy, cx) * 180 / M_PI + 360, 360));
            g.themeSat = float(ss / sw);
        }
        groups_.push_back(g);
    }
}

QColor VfxEditor::recolor(const QColor &c, float themeHue, float themeSat, const QColor &target) {
    float h, s, v; rgbToHsv(c.redF(), c.greenF(), c.blueF(), h, s, v);
    float d = std::fmod(h - themeHue + 540.0f, 360.0f) - 180.0f;
    float f = (1.0f - smooth(50.0f, 75.0f, std::fabs(d))) * smooth(0.06f, 0.18f, s);
    if (f <= 0) return c;
    float th, ts, tv; rgbToHsv(target.redF(), target.greenF(), target.blueF(), th, ts, tv);
    float nh, ns;
    if (ts < 0.12f) { nh = h; ns = s * 0.12f; }
    else {
        nh = th + d * 0.35f;
        ns = std::clamp(s * std::clamp(ts / std::max(themeSat, 0.05f), 0.1f, 1.6f), 0.0f, 1.0f);
    }
    // blend in hue space along the shortest arc
    float dh = std::fmod(nh - h + 540.0f, 360.0f) - 180.0f;
    float rh = h + dh * f, rs = s + (ns - s) * f;
    float r, g, b; hsvToRgb(rh, rs, v, r, g, b);
    return QColor::fromRgbF(std::clamp(r, 0.0f, 1.0f), std::clamp(g, 0.0f, 1.0f), std::clamp(b, 0.0f, 1.0f), c.alphaF());
}

QImage VfxEditor::recolorImage(const QImage &src, float themeHue, float themeSat, const QColor &target) {
    QImage img = src.convertToFormat(QImage::Format_RGBA8888);
    // 4096-entry cache keyed by quantised colour keeps big textures fast
    std::vector<int> cache(1 << 15, -1);
    for (int y = 0; y < img.height(); ++y) {
        uchar *p = img.scanLine(y);
        for (int x = 0; x < img.width(); ++x, p += 4) {
            int key = ((p[0] >> 3) << 10) | ((p[1] >> 3) << 5) | (p[2] >> 3);
            int &cv = cache[size_t(key)];
            if (cv < 0) {
                QColor o = recolor(QColor(p[0], p[1], p[2]), themeHue, themeSat, target);
                cv = (o.red() << 16) | (o.green() << 8) | o.blue();
            }
            // keep the low bits of the original so gradients stay smooth
            int dr = ((cv >> 16) & 255) - ((p[0] >> 3) << 3 | 4), dg = ((cv >> 8) & 255) - ((p[1] >> 3) << 3 | 4), db = (cv & 255) - ((p[2] >> 3) << 3 | 4);
            p[0] = uchar(std::clamp(p[0] + dr, 0, 255)); p[1] = uchar(std::clamp(p[1] + dg, 0, 255)); p[2] = uchar(std::clamp(p[2] + db, 0, 255));
        }
    }
    return img;
}

BinFile VfxEditor::apply(const BinFile &orig, const std::map<QString, VfxTarget> &targets, const TextureLoader &load,
                         std::map<QString, QImage> &textures) const {
    BinFile bin = orig;
    auto targetOf = [&](const QString &key) -> const VfxTarget * {
        auto it = targets.find(key);
        return (it != targets.end() && it->second.enabled) ? &it->second : nullptr;
    };
    // texture -> groups using it
    std::map<QString, std::vector<const VfxGroup *>> users;
    for (const auto &g : groups_) for (const auto &t : g.textures) users[t].push_back(&g);

    for (const auto &g : groups_) {
        const VfxTarget *t = targetOf(g.key);
        if (!t || !g.hasColor) continue;
        std::map<QString, QString> rename;   // original path -> path this group uses
        for (const QString &tex : g.textures) {
            bool shared = false;
            for (const VfxGroup *o : users[tex]) {
                if (o == &g) continue;
                const VfxTarget *ot = targetOf(o->key);
                if (!ot || ot->color != t->color) { shared = true; break; }
            }
            QImage img = load(tex);
            if (img.isNull()) continue;            // not available: leave this texture as it is
            QString out = tex;
            if (shared || tex.endsWith(".dds")) {   // .dds are rewritten as .tex under a new name
                int dot = tex.lastIndexOf('.');
                out = tex.left(dot) + "_studio_" + g.key.toLower() + ".tex";
                rename[tex] = out;
            }
            textures[out] = recolorImage(img, g.themeHue, g.themeSat, t->color);
        }
        for (uint32_t en : g.entries) {
            BinEntry *e = bin.entry(en);
            if (!e) continue;
            forEachColor(*e, [&](BinValue &v) {
                QVector4D c = v.vec4();
                float m = std::max({c.x(), c.y(), c.z(), 1.0f});
                QColor q = QColor::fromRgbF(c.x() / m, c.y() / m, c.z() / m);
                QColor n = recolor(q, g.themeHue, g.themeSat, t->color);
                v.setVec4(QVector4D(float(n.redF()) * m, float(n.greenF()) * m, float(n.blueF()) * m, c.w()));
            });
            if (!rename.empty())
                forEachString(*e, [&](BinValue &v) {
                    auto it = rename.find(v.string().toLower());
                    if (it != rename.end()) v.setString(it->second);
                });
        }
    }
    return bin;
}

} // namespace lol
