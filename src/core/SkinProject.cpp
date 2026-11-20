#include "core/SkinProject.h"
#include "core/Hashes.h"
#include <QBuffer>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <set>

namespace lol {

namespace {

uint32_t F(const char *s) { return fnv1a32(s); }

// File / String BIN value -> WAD hash
uint64_t pathHash(const BinValue &v) {
    if (v.type == BinType::File) return v.u64();
    if (v.type == BinType::String) return xxh64Path(v.string().toLower());
    return 0;
}

BinEntry *skinProps(BinFile &b) {
    auto es = b.entriesOfClass(F("SkinCharacterDataProperties"));
    return es.empty() ? nullptr : es.front();
}

QString fileLabel(const QString &p) { return QFileInfo(p).fileName(); }

struct AnimLabel { const char *key; const char *label; int order; };
const AnimLabel kAnimLabels[] = {
    {"idle_enter", "Bekleme girişi", 0}, {"idle", "Bekleme", 0}, {"run", "Koşma", 1}, {"attack", "Saldırı", 2}, {"crit", "Kritik saldırı", 3},
    {"spell1", "Q", 4}, {"spell2", "W", 5}, {"spell3", "E", 6}, {"spell4", "R", 7}, {"channel", "Kanal", 12}, {"respawn", "Yeniden doğma", 14},
    {"dance", "Dans", 8}, {"taunt", "Kışkırtma", 9}, {"joke", "Şaka", 10}, {"laugh", "Gülme", 11},
    {"recall", "Geri dönüş", 12}, {"death", "Ölüm", 13},
};

} // namespace

QString voiceGroup(const QString &event, QString *label) {
    QString rest = event;
    const QStringList parts = event.split('_');
    // Play_vo_<Champ>_<Rest...>
    QString champ;
    if (parts.size() >= 4 && parts[0].compare("play", Qt::CaseInsensitive) == 0 && parts[1].compare("vo", Qt::CaseInsensitive) == 0) {
        champ = parts[2];
        rest = parts.mid(3).join('_');
    }
    auto who = [](const QString &s) {
        // trailing target name: General / Darius / Standard ...
        static const QRegularExpression re("(2D|3D)([A-Za-z]+)$");
        auto m = re.match(s);
        if (!m.hasMatch()) return QString();
        QString t = m.captured(2);
        if (t == "General" || t == "Standard") return QString(" (genel)");
        return " — " + t;
    };
    QString g = "O", l = rest;
    if (!champ.isEmpty() && rest.startsWith(champ, Qt::CaseInsensitive)) {
        QString ab = rest.mid(champ.size());
        const QString low = ab.toLower();
        if (low.startsWith("basicattack") || low.startsWith("critattack")) { g = "A"; l = low.startsWith("crit") ? "Kritik saldırı" : "Temel saldırı"; }
        else if (low.startsWith("passive") || low.startsWith("p_")) { g = "P"; l = "Pasif"; }
        else if (!ab.isEmpty() && QString("QWER").contains(ab[0].toUpper()) && (ab.size() == 1 || !ab[1].isLower())) {
            g = QString(ab[0].toUpper());
            l = g + (low.mid(1).startsWith("attack") ? " — güçlendirilmiş vuruş" : " kullanınca");
        }
    } else {
        const QString low = rest.toLower();
        if (low.startsWith("attack")) { g = "A"; l = "Saldırı emri"; }
        else if (low.startsWith("move")) { g = "M"; l = "Yürüme emri"; }
        else if (low.startsWith("death")) { g = "D"; l = "Ölüm"; }
        else if (low.startsWith("joke")) { g = "S"; l = "Şaka" + who(rest); }
        else if (low.startsWith("taunt")) { g = "S"; l = "Kışkırtma" + who(rest); }
        else if (low.startsWith("laugh")) { g = "S"; l = "Gülme" + who(rest); }
        else if (low.startsWith("dance")) { g = "S"; l = "Dans" + who(rest); }
        else if (low.startsWith("kill")) { g = "K"; l = "Öldürme" + who(rest); }
        else if (low.startsWith("firstencounter")) { g = "K"; l = "İlk karşılaşma" + who(rest); }
        else if (low.startsWith("recall")) { g = "M"; l = "Geri dönüş"; }
        else if (low.startsWith("respawn") || low.startsWith("spawn")) { g = "D"; l = "Yeniden doğma"; }
        else if (low.startsWith("buy") || low.startsWith("shop")) { g = "O"; l = "Mağaza"; }
    }
    if (label) *label = l;
    return g;
}

PcmClip VoiceLine::processed() const {
    if (mode == Mute) { PcmClip c; c.rate = 48000; c.samples.assign(4800, 0); return c; }
    if (mode == Replace && !source.isEmpty()) return processVoice(resample(source, 48000), edit);
    return {};
}

QString SkinProject::findGameWad(const QString &gameDir, const QString &champion) {
    if (gameDir.isEmpty() || champion.isEmpty()) return {};
    const QString want = champion.toLower() + ".wad.client";
    // fast paths: League install layout and a few common folder layouts
    const QStringList guesses = {
        gameDir + "/Game/DATA/FINAL/Champions/", gameDir + "/DATA/FINAL/Champions/", gameDir + "/FINAL/Champions/",
        gameDir + "/Champions/", gameDir + "/" + champion.toLower() + "/wads/", gameDir + "/wads/", gameDir + "/",
    };
    for (const QString &g : guesses) {
        QDir d(g);
        if (!d.exists()) continue;
        for (const QString &f : d.entryList(QDir::Files))
            if (f.toLower() == want) return d.filePath(f);
    }
    QDirIterator it(gameDir, QDir::Files, QDirIterator::Subdirectories);
    int seen = 0;
    while (it.hasNext() && ++seen < 200000) {
        it.next();
        if (it.fileName().toLower() == want) return it.filePath();
    }
    return {};
}

QByteArray SkinProject::readFile(uint64_t h) const {
    for (const auto &[n, w] : wads)
        if (w.contains(h)) return w.read(h);
    if (game_ && game_->contains(h)) return game_->read(h);
    return {};
}

QByteArray SkinProject::readFile(const QString &path) const { return readFile(xxh64Path(path.toLower())); }

QImage SkinProject::readTexture(uint64_t h, uint8_t *fmt, bool *mips) const {
    const QByteArray d = readFile(h);
    if (d.isEmpty() || !Texture::looksLikeTexture(d)) return {};
    try {
        Texture t = Texture::fromBytes(d);
        if (fmt) *fmt = t.format;
        if (mips) *mips = t.mips;
        return t.image;
    } catch (const FormatError &) { return {}; }
}

void SkinProject::load(const QString &path, const QString &gameDir) {
    fantomePath = path;
    fantome = Fantome::load(path);
    for (const auto &[n, bytes] : fantome.wads) wads.emplace(n, Wad::fromBytes(bytes, n));

    // champion: the WAD without a locale ("Garen.wad.client")
    for (const auto &[n, w] : wads) {
        const QString base = n.left(n.size() - int(QString(".wad.client").size()));
        if (!base.contains('.')) { mainWadName = n; champion = base; break; }
    }
    if (mainWadName.isEmpty()) throw FormatError("pakette şampiyonun ana WAD dosyası yok (ör. Garen.wad.client)");

    gameWadPath = findGameWad(gameDir, champion);
    if (!gameWadPath.isEmpty()) {
        game_ = std::make_unique<WadFile>();
        if (!game_->open(gameWadPath)) { game_.reset(); gameWadPath.clear(); }
    }

    const QString c = champion.toLower();
    for (int i = 0; i < 200 && skinBinPath.isEmpty(); ++i) {
        const QString p = QString("data/characters/%1/skins/skin%2.bin").arg(c).arg(i);
        if (wads.at(mainWadName).contains(p)) skinBinPath = p;
    }
    if (skinBinPath.isEmpty()) throw FormatError("pakette skin tanım dosyası (skinN.bin) bulunamadı");
    skinBin = BinFile::fromBytes(wads.at(mainWadName).read(skinBinPath));
    if (!skinProps(skinBin)) throw FormatError("skin tanım dosyasında SkinCharacterDataProperties yok");

    name = fantome.info.value("Name").toString();
    author = fantome.info.value("Author").toString();
    version = fantome.info.value("Version").toString();
    description = fantome.info.value("Description").toString();
    if (!fantome.image.isEmpty()) cover = QImage::fromData(fantome.image);

    loadModel();
    vfx.analyze(skinBin, [this](const QString &p) { return readTexture(xxh64Path(p)); });
    for (const auto &g : vfx.groups()) vfxTargets[g.key] = VfxTarget{false, g.current()};
    loadLoadscreen();
    loadVoice();
}

void SkinProject::loadModel() {
    BinEntry *props = skinProps(skinBin);
    BinField *mp = props->field(F("skinMeshProperties"));
    if (!mp) throw FormatError("skinMeshProperties yok");
    BinValue &m = mp->value;
    const BinField *skn = m.field(F("simpleSkin"));
    if (!skn) throw FormatError("modelin yolu (simpleSkin) yok");
    const QByteArray sknData = readFile(pathHash(skn->value));
    if (sknData.isEmpty()) throw FormatError(("model dosyası bulunamadı: " + skn->value.string()).toStdString());
    mesh = SkinnedMesh::fromSkn(sknData);

    if (const BinField *skl = m.field(F("skeleton"))) {
        const QByteArray d = readFile(pathHash(skl->value));
        if (!d.isEmpty()) { skeleton = Skeleton::fromSkl(d); hasSkeleton = true; }
    }

    uint64_t baseTex = 0;
    QString baseLabel;
    if (const BinField *t = m.field(F("texture"))) {
        baseTex = pathHash(t->value);
        baseLabel = t->value.type == BinType::String ? fileLabel(t->value.string()) : QString("ana doku");
    }
    std::map<QString, std::pair<uint64_t, QString>> overrides;
    if (const BinField *mo = m.field(F("materialOverride")))
        for (const BinValue &it : mo->value.items) {
            const BinField *sm = it.field(F("submesh")), *tx = it.field(F("texture"));
            if (sm && tx) overrides[sm->value.string().toLower()] =
                {pathHash(tx->value), tx->value.type == BinType::String ? fileLabel(tx->value.string()) : sm->value.string() + " dokusu"};
        }

    // one part per distinct texture; submeshes sharing a texture share the part
    std::map<uint64_t, int> byTex;
    for (const Submesh &s : mesh.submeshes) {
        auto o = overrides.find(s.name.toLower());
        uint64_t h = o != overrides.end() ? o->second.first : baseTex;
        QString lab = o != overrides.end() ? o->second.second : baseLabel;
        if (!h) continue;
        auto it = byTex.find(h);
        if (it != byTex.end()) { parts[size_t(it->second)].submesh += ", " + s.name; continue; }
        ModelPart p;
        p.submesh = s.name; p.texHash = h; p.texLabel = lab;
        p.original = readTexture(h, &p.texFormat, &p.texMips);
        if (p.original.isNull()) continue;
        p.original = p.original.convertToFormat(QImage::Format_RGBA8888);
        byTex[h] = int(parts.size());
        parts.push_back(std::move(p));
    }
    for (ModelPart &p : parts) {
        std::vector<int> subs;
        for (int i = 0; i < int(mesh.submeshes.size()); ++i)
            if (p.submesh.split(", ").contains(mesh.submeshes[size_t(i)].name)) subs.push_back(i);
        auto used = uvMask(mesh, subs, p.original.width(), p.original.height());
        p.regions = RegionMap::build(p.original, used, 6);
        p.adjust.assign(p.regions.regions.size(), RegionAdjust{});
    }

    // animations: the animation BIN (one of the skin BIN's links) maps clip-name hashes to .anm files
    std::map<uint32_t, QString> clipNames;
    {
        const QStringList bases = {"Idle", "Idle_Enter", "Run", "Attack", "Crit", "Spell1", "Spell2", "Spell3", "Spell4", "Dance",
                                   "Death", "Recall", "Joke", "Taunt", "Laugh", "Channel", "Respawn", "Passive", "Emote"};
        const QStringList sufs = {"", "1", "2", "3", "4", "5", "6", "_Base", "1_Base", "2_Base", "3_Base", "4_Base", "_Loop",
                                  "_Loop_Base", "_Haste", "_Fast", "_Spell1", "_Leadout", "_Winddown", "_Wndup", "_0", "_90",
                                  "_180", "_-90", "_-180", "_In", "_Out", "_Into", "_Loop1", "A", "B", "C", "_Turn"};
        for (const QString &b : bases) for (const QString &s : sufs) clipNames[fnv1a32(b + s)] = b + s;
    }
    std::set<uint64_t> seenFiles;
    for (const QByteArray &link : skinBin.links) {
        const QString l = QString::fromUtf8(link).toLower();
        if (!l.contains("/animations/")) continue;
        const QByteArray d = readFile(l);
        if (d.isEmpty()) continue;
        BinFile ab;
        try { ab = BinFile::fromBytes(d); } catch (const FormatError &) { continue; }
        for (BinEntry &e : ab.entries) {
            BinField *cm = e.field(F("mClipDataMap"));
            if (!cm) continue;
            for (size_t i = 0; i < cm->value.items.size() && i < cm->value.keys.size(); ++i) {
                const BinField *rd = cm->value.items[i].field(F("mAnimationResourceData"));
                const BinField *fp = rd ? rd->value.field(F("mAnimationFilePath")) : nullptr;
                if (!fp) continue;
                const uint64_t fh = pathHash(fp->value);
                if (!fh || seenFiles.count(fh)) continue;
                seenFiles.insert(fh);
                AnimClip c;
                c.file = fh;
                const uint32_t kh = cm->value.keys[i].u32();
                auto n = clipNames.find(kh);
                c.clip = n != clipNames.end() ? n->second : QString("#%1").arg(kh, 8, 16, QChar('0'));
                const QString low = c.clip.toLower();
                c.label = c.clip;
                for (const auto &al : kAnimLabels)
                    if (low.startsWith(al.key)) {
                        QString rest = c.clip.mid(int(strlen(al.key)));
                        rest.remove(QRegularExpression("(?i)_?base"));
                        if (QString(al.key).startsWith("spell")) rest.remove(0, 1);   // the ability digit
                        c.label = QString(al.label) + (rest.isEmpty() || rest == "1" ? "" : " " + rest.replace('_', ' ').trimmed());
                        c.order = al.order;
                        break;
                    }
                if (c.clip.startsWith('#')) c.label = "Diğer " + c.clip.mid(1, 4);
                animations.push_back(c);
            }
        }
    }
    std::stable_sort(animations.begin(), animations.end(), [](const AnimClip &a, const AnimClip &b) {
        return a.order != b.order ? a.order < b.order : a.label < b.label;
    });
}

Animation SkinProject::loadAnimation(uint64_t file) const {
    const QByteArray d = readFile(file);
    if (d.isEmpty()) throw FormatError("animasyon dosyası bulunamadı");
    return Animation::fromBytes(d);
}

void SkinProject::loadLoadscreen() {
    BinEntry *props = skinProps(skinBin);
    BinField *ls = props->field(F("loadscreen"));
    if (!ls) return;
    if (const BinField *img = ls->value.field(F("image"))) {
        loadscreen = readTexture(pathHash(img->value));
        if (!loadscreen.isNull()) loadscreenSize = loadscreen.size();
    }
}

void SkinProject::loadVoice() {
    BinEntry *props = skinProps(skinBin);
    BinField *ap = props->field(F("skinAudioProperties"));
    if (!ap) { voiceNote = "Bu skinde ses tanımı yok."; return; }
    BinField *units = ap->value.field(F("bankUnits"));
    if (!units) { voiceNote = "Bu skinde ses bankası yok."; return; }
    QStringList events;
    QString eventsBnk, audioWpk;
    for (BinValue &u : units->value.items) {
        BinField *bp = u.field(F("bankPath"));
        if (!bp) continue;
        QString ev, wp;
        for (const BinValue &p : bp->value.items) {
            const QString s = p.string().toLower();
            if (s.endsWith("_vo_events.bnk")) ev = s;
            if (s.endsWith("_vo_audio.wpk")) wp = s;
        }
        if (ev.isEmpty()) continue;
        eventsBnk = ev; audioWpk = wp;
        if (BinField *es = u.field(F("events")))
            for (const BinValue &e : es->value.items) events << e.string();
    }
    if (eventsBnk.isEmpty()) { voiceNote = "Bu skinde konuşma (VO) bankası yok."; return; }
    // VO paths are locale specific; some packages put en_us paths into every locale WAD
    auto localeVariants = [](const QString &p) {
        QStringList out{p};
        static const QRegularExpression re("/vo/[a-z]{2}_[a-z]{2}/");
        for (const char *loc : {"en_us", "tr_tr"}) { QString q = p; q.replace(re, QString("/vo/%1/").arg(loc)); out << q; }
        out.removeDuplicates();
        return out;
    };
    QString bnkPath, wpkPath;
    for (const auto &[n, w] : wads) {
        for (const QString &cand : localeVariants(eventsBnk))
            if (w.contains(cand)) {
                if (bnkPath.isEmpty()) bnkPath = cand;
                voiceWads_.push_back(n);
                break;
            }
    }
    if (bnkPath.isEmpty()) {
        voiceNote = "Paketin içinde ses bankası yok; bu skin oyunun kendi seslerini kullanıyor. Ses değiştirme şimdilik "
                    "yalnızca kendi ses bankasını taşıyan paketlerde çalışıyor.";
        return;
    }
    voiceBankPath = bnkPath;
    for (const QString &cand : localeVariants(audioWpk)) {
        if (wads.at(voiceWads_.front()).contains(cand)) { wpkPath = cand; break; }
    }
    const Wad &vw = wads.at(voiceWads_.front());
    bank_ = WwiseBank::fromBytes(vw.read(bnkPath));
    if (!wpkPath.isEmpty()) wpk_ = Wpk::fromBytes(vw.read(wpkPath));

    std::map<uint32_t, uint32_t> soundSrc;
    for (const auto &o : bank_.objects) if (o.type == 2) soundSrc[o.id] = WwiseBank::soundSource(o);

    events.removeDuplicates();
    for (const QString &ev : events) {
        if (!ev.contains("_vo_", Qt::CaseInsensitive)) continue;
        VoiceLine line;
        line.event = ev;
        line.eventId = wwiseFnv1(ev);
        if (!bank_.object(line.eventId)) continue;
        line.sounds = bank_.eventSounds(line.eventId);
        if (line.sounds.empty()) continue;
        line.group = voiceGroup(ev, &line.label);
        auto s = soundSrc.find(line.sounds.front());
        if (s != soundSrc.end()) {
            auto w = wpk_.wems.find(s->second);
            if (w != wpk_.wems.end()) line.original = decodeWem(w->second, &line.originalNote);
            else line.originalNote = "orijinal ses oyunun kendi dosyasında";
        }
        voice.push_back(std::move(line));
    }
    static const QString order = "PQWERAMDSKO";
    std::stable_sort(voice.begin(), voice.end(), [](const VoiceLine &a, const VoiceLine &b) {
        int x = order.indexOf(a.group), y = order.indexOf(b.group);
        return x != y ? x < y : a.label < b.label;
    });
}

QImage SkinProject::partImage(int i, int highlight) const {
    const ModelPart &p = parts[size_t(i)];
    if (!p.edited() && highlight < 0) return p.original;
    return applyRegions(p.original, p.regions, p.adjust, highlight);
}

QImage SkinProject::previewVfxTexture(const QString &key) const {
    for (const auto &g : vfx.groups()) {
        if (g.key != key) continue;
        for (const QString &t : g.textures) {
            QImage img = readTexture(xxh64Path(t));
            if (img.isNull()) continue;
            auto it = vfxTargets.find(key);
            if (it != vfxTargets.end() && it->second.enabled && g.hasColor)
                img = VfxEditor::recolorImage(img, g.themeHue, g.themeSat, it->second.color);
            return img;
        }
    }
    return {};
}

bool SkinProject::hasChanges() const {
    for (const auto &p : parts) if (p.edited()) return true;
    for (const auto &[k, t] : vfxTargets) if (t.enabled) return true;
    for (const auto &v : voice) if (v.mode != VoiceLine::Keep) return true;
    if (loadscreenChanged || coverChanged) return true;
    return name != fantome.info.value("Name").toString() || author != fantome.info.value("Author").toString() ||
           version != fantome.info.value("Version").toString() || description != fantome.info.value("Description").toString();
}

QString SkinProject::save(const QString &outPath) const {
    QStringList report;
    std::map<QString, Wad> out = wads;
    Wad &main = out.at(mainWadName);

    // 1. effects
    std::map<QString, QImage> vfxTex;
    BinFile bin = vfx.apply(skinBin, vfxTargets, [this](const QString &p) { return readTexture(xxh64Path(p)); }, vfxTex);
    for (const auto &[path, img] : vfxTex) {
        Texture t; t.image = img;
        uint8_t fmt = Texture::DXT5; bool mips = false;
        // keep the source texture's format when it is the same file
        if (!readTexture(xxh64Path(path), &fmt, &mips).isNull()) {}
        else {
            for (const auto &g : vfx.groups())
                for (const QString &src : g.textures)
                    if (path.startsWith(src.left(src.lastIndexOf('.')))) readTexture(xxh64Path(src), &fmt, &mips);
        }
        t.format = fmt; t.mips = mips;
        main.put(path, t.toTex());
    }
    int vfxGroups = 0;
    for (const auto &[k, t] : vfxTargets) if (t.enabled) ++vfxGroups;
    if (vfxGroups) report << QString("%1 efekt grubu, %2 doku yeniden renklendi").arg(vfxGroups).arg(vfxTex.size());

    // 2. model colours
    int partsDone = 0;
    for (size_t i = 0; i < parts.size(); ++i) {
        const ModelPart &p = parts[i];
        if (!p.edited()) continue;
        Texture t; t.image = applyRegions(p.original, p.regions, p.adjust); t.format = p.texFormat; t.mips = p.texMips;
        main.put(p.texHash, t.toTex());
        ++partsDone;
    }
    if (partsDone) report << QString("%1 model dokusu yeniden renklendi").arg(partsDone);

    // 3. loading screen
    if (loadscreenChanged && !newLoadscreen.isNull()) {
        const QString c = champion.toLower();
        const QString path = QString("assets/characters/%1/skins/studio/%1loadscreen_studio.tex").arg(c);
        Texture t; t.image = newLoadscreen.convertToFormat(QImage::Format_RGBA8888); t.format = Texture::DXT1; t.mips = false;
        main.put(path, t.toTex());
        BinEntry *props = skinProps(bin);
        BinField *ls = props->field(F("loadscreen"));
        if (ls) {
            if (BinField *img = ls->value.field(F("image"))) {
                if (img->value.type == BinType::File) img->value.setU64(xxh64Path(path));
                else img->value.setString(path);
            }
        }
        report << "yükleme ekranı görseli değişti";
    }
    main.put(skinBinPath, bin.toBytes());

    // 4. voice
    int voiceDone = 0;
    bool anyVoice = false;
    for (const auto &v : voice) if (v.mode != VoiceLine::Keep) anyVoice = true;
    if (anyVoice && !voiceBankPath.isEmpty()) {
        WwiseBank bank = bank_;
        Wpk wpk = wpk_;
        std::map<uint32_t, std::set<uint32_t>> users;   // source id -> sound objects
        for (const auto &o : bank.objects) if (o.type == 2) users[WwiseBank::soundSource(o)].insert(o.id);
        uint32_t nextId = 0x5D100000;
        auto freshId = [&]() {
            while (wpk.wems.count(nextId) || users.count(nextId)) ++nextId;
            return nextId++;
        };
        for (const auto &v : voice) {
            if (v.mode == VoiceLine::Keep) continue;
            const PcmClip clip = v.processed();
            if (clip.isEmpty()) continue;
            uint32_t hs = 0;
            const QByteArray wem = makePcmWem(clip, &hs);
            const std::set<uint32_t> mine(v.sounds.begin(), v.sounds.end());
            std::map<uint32_t, uint32_t> remapped;   // old source -> new source for this line
            for (uint32_t sid : v.sounds) {
                WwiseBank::Object *o = bank.object(sid);
                if (!o || o->type != 2) continue;
                const uint32_t src = WwiseBank::soundSource(*o);
                uint32_t dst = src;
                const auto &u = users[src];
                bool onlyMine = std::all_of(u.begin(), u.end(), [&](uint32_t x) { return mine.count(x) != 0; });
                if (!onlyMine) {
                    auto r = remapped.find(src);
                    dst = r != remapped.end() ? r->second : (remapped[src] = freshId());
                    users[src].erase(sid);
                    users[dst].insert(sid);
                }
                WwiseBank::setSoundSource(*o, kCodecPcm, dst, hs);
                wpk.wems[dst] = wem;
            }
            ++voiceDone;
        }
        const QByteArray bnkBytes = bank.toBytes(), wpkBytes = wpk.toBytes();
        QString wpkPath = voiceBankPath; wpkPath.replace("_vo_events.bnk", "_vo_audio.wpk");
        for (const QString &n : voiceWads_) {
            Wad &w = out.at(n);
            w.put(voiceBankPath, bnkBytes, false);
            w.put(wpkPath, wpkBytes, false);
        }
        report << QString("%1 ses satırı değişti").arg(voiceDone);
    }

    // 5. package
    Fantome f = fantome;
    for (auto &[n, w] : out) f.wads[n] = w.toBytes();
    f.info["Name"] = name; f.info["Author"] = author; f.info["Version"] = version; f.info["Description"] = description;
    if (coverChanged && !cover.isNull()) {
        QByteArray png; QBuffer b(&png); b.open(QIODevice::WriteOnly); cover.save(&b, "PNG"); f.image = png;
    }
    f.save(outPath);
    if (report.isEmpty()) report << "yalnızca paket bilgileri";
    return report.join(", ");
}

} // namespace lol
