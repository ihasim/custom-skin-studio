#include "app/AppController.h"
#include "core/Hashes.h"
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QJSEngine>
#include <QQmlEngine>
#include <QSettings>
#include <QStandardPaths>
#include <QtConcurrent>
#include <cmath>

using namespace lol;

static AppController *g_instance = nullptr;

AppController *AppController::instance() { return g_instance; }
AppController *AppController::create(QQmlEngine *, QJSEngine *) {
    QJSEngine::setObjectOwnership(g_instance, QJSEngine::CppOwnership);
    return g_instance;
}

static QString defaultGameDir() {
    QStringList c;
#ifdef Q_OS_WIN
    c << "C:/Riot Games/League of Legends" << "D:/Riot Games/League of Legends" << "C:/Program Files/Riot Games/League of Legends";
#else
    c << "/Applications/League of Legends.app/Contents/LoL";
#endif
    for (const QString &d : c) if (QDir(d).exists()) return d;
    return {};
}

AppController::AppController(QObject *parent) : QObject(parent) {
    g_instance = this;
    QSettings s;
    gameDir_ = s.value("gameDir").toString();
    if (gameDir_.isEmpty()) gameDir_ = defaultGameDir();

    frameTimer_.setInterval(16);
    connect(&frameTimer_, &QTimer::timeout, this, &AppController::tick);
    textureTimer_.setSingleShot(true);
    textureTimer_.setInterval(30);
    connect(&textureTimer_, &QTimer::timeout, this, [this] {
        for (int i = 0; i < int(partStale_.size()); ++i)
            if (partStale_[size_t(i)]) rebuildPartTexture(i);
    });
    connect(&loadWatch_, &QFutureWatcher<QString>::finished, this, &AppController::finishLoad);
    connect(&audio_, &AudioEngine::finished, this, [this] { playLine_ = -1; emit playingLineChanged(); });
    connect(&audio_, &AudioEngine::progressChanged, this, &AppController::playProgressChanged);
    connect(&audio_, &AudioEngine::levelChanged, this, &AppController::recordLevelChanged);
}

AppController::~AppController() { if (g_instance == this) g_instance = nullptr; }

QString AppController::fileName() const { return project_ ? QFileInfo(project_->fantomePath).fileName() : QString(); }

// ------------------------------------------------------------------ open / save

void AppController::openFantome(const QUrl &url) {
    if (screen_ == "loading") return;
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (!path.toLower().endsWith(".fantome") && !path.toLower().endsWith(".zip")) {
        setError("Bu bir .fantome dosyası değil: " + QFileInfo(path).fileName());
        return;
    }
    stopVoice();
    setError({});
    setStatus("Açılıyor: " + QFileInfo(path).fileName());
    setScreen("loading");
    QString game = gameDir_;
    loading_ = std::make_unique<SkinProject>();
    SkinProject *p = loading_.get();
    loadWatch_.setFuture(QtConcurrent::run([p, path, game]() -> QString {
        try { p->load(path, game); return {}; }
        catch (const std::exception &e) { return QString::fromUtf8(e.what()); }
    }));
}

void AppController::finishLoad() {
    const QString err = loadWatch_.result();
    if (!err.isEmpty()) {
        loading_.reset();
        setScreen(project_ ? "editor" : "start");
        setError("Paket açılamadı: " + err);
        setStatus({});
        return;
    }
    project_ = std::move(loading_);
    partImages_.clear();
    for (const auto &pt : project_->parts) partImages_.push_back(pt.original);
    partStale_.assign(project_->parts.size(), false);
    selPart_ = 0; selRegion_ = -1;
    loadscreenSource_ = {};
    float maxY = 0;
    for (const auto &v : project_->mesh.positions) maxY = std::max(maxY, v.y());
    modelHeight_ = maxY > 1 ? maxY : 200;
    anim_ = -1; clip_.reset();
    skin_.clear();
    emit projectChanged();
    emit editsChanged();
    emit selectionChanged();
    emit voiceChanged();
    ++revision_; emit imagesChanged();
    // start on the idle animation
    int idle = -1;
    for (int i = 0; i < int(project_->animations.size()); ++i)
        if (project_->animations[size_t(i)].clip.toLower().startsWith("idle1")) { idle = i; break; }
    setAnimation(idle >= 0 ? idle : (project_->animations.empty() ? -1 : 0));
    computePose();
    setScreen("editor");
    QString st = project_->gameWadPath.isEmpty()
        ? "Oyun dosyaları bulunamadı: animasyonlar ve paketin taşımadığı dokular gösterilemiyor."
        : QString("%1 açıldı").arg(fileName());
    setStatus(st);
    emit loaded();
}

void AppController::closeProject() {
    stopVoice();
    if (recLine_ >= 0) stopRecording();
    project_.reset();
    frameTimer_.stop();
    setScreen("start");
    emit projectChanged();
}

void AppController::setGameDir(const QUrl &url) {
    gameDir_ = url.isLocalFile() ? url.toLocalFile() : url.toString();
    QSettings().setValue("gameDir", gameDir_);
    emit gameDirChanged();
    if (project_) openFantome(QUrl::fromLocalFile(project_->fantomePath));   // reload with the new folder
}

QString AppController::suggestedSaveName() const {
    if (!project_) return {};
    QString n = project_->name.isEmpty() ? QFileInfo(project_->fantomePath).completeBaseName() : project_->name;
    n.replace(QRegularExpression("[^A-Za-z0-9ğüşıöçĞÜŞİÖÇ._-]+"), "_");
    return n + "_v" + (project_->version.isEmpty() ? "1.0" : project_->version) + ".fantome";
}

QUrl AppController::saveFolder() const {
    return project_ ? QUrl::fromLocalFile(QFileInfo(project_->fantomePath).absolutePath()) : QUrl();
}

QString AppController::changeSummary() const {
    if (!project_) return {};
    QStringList s;
    int parts = 0, regions = 0;
    for (const auto &p : project_->parts) {
        int r = 0; for (const auto &a : p.adjust) if (!a.isIdentity()) ++r;
        if (r) { ++parts; regions += r; }
    }
    if (regions) s << QString("Renkler: %1 bölge").arg(regions);
    QStringList fx;
    for (const auto &g : project_->vfx.groups()) {
        auto it = project_->vfxTargets.find(g.key);
        if (it != project_->vfxTargets.end() && it->second.enabled) fx << g.label;
    }
    if (!fx.isEmpty()) s << "Efektler: " + fx.join(", ");
    int rep = 0, mute = 0;
    for (const auto &v : project_->voice) { if (v.mode == VoiceLine::Replace) ++rep; if (v.mode == VoiceLine::Mute) ++mute; }
    if (rep || mute) s << QString("Sesler: %1 yeni, %2 sessiz").arg(rep).arg(mute);
    if (project_->loadscreenChanged) s << "Yükleme ekranı";
    if (project_->coverChanged) s << "Kapak görseli";
    return s.join("\n");
}

bool AppController::save(const QUrl &url) {
    if (!project_) return false;
    QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (!path.toLower().endsWith(".fantome")) path += ".fantome";
    if (QFileInfo(path).absoluteFilePath() == QFileInfo(project_->fantomePath).absoluteFilePath()) {
        setError("Açık paketin üzerine yazılmıyor; yeni bir ad seç.");
        return false;
    }
    setStatus("Kaydediliyor…");
    try {
        const QString report = project_->save(path);
        setStatus("Kaydedildi: " + QFileInfo(path).fileName());
        emit saved(path, report);
        return true;
    } catch (const std::exception &e) {
        setError(QString("Kaydedilemedi: ") + e.what());
        setStatus({});
        return false;
    }
}

// ------------------------------------------------------------------ package info

void AppController::setPkgName(const QString &s) { if (project_ && s != project_->name) { project_->name = s; edited(); } }
void AppController::setPkgAuthor(const QString &s) { if (project_ && s != project_->author) { project_->author = s; edited(); } }
void AppController::setPkgVersion(const QString &s) { if (project_ && s != project_->version) { project_->version = s; edited(); } }
void AppController::setPkgDescription(const QString &s) { if (project_ && s != project_->description) { project_->description = s; edited(); } }

// ------------------------------------------------------------------ colours

static QColor adjusted(const QColor &c, const RegionAdjust &a) {
    float h, s, v; rgbToHsv(c.redF(), c.greenF(), c.blueF(), h, s, v);
    h = std::fmod(h + a.hue + 720.0f, 360.0f);
    s = std::clamp(s * a.sat, 0.0f, 1.0f);
    v = std::clamp(v * a.val, 0.0f, 1.0f);
    float r, g, b; hsvToRgb(h, s, v, r, g, b);
    return QColor::fromRgbF(r, g, b);
}

QVariantList AppController::parts() const {
    QVariantList out;
    if (!project_) return out;
    for (size_t i = 0; i < project_->parts.size(); ++i) {
        const ModelPart &p = project_->parts[i];
        QVariantList regions;
        for (size_t r = 0; r < p.regions.regions.size(); ++r) {
            const ColorRegion &cr = p.regions.regions[r];
            const RegionAdjust &a = p.adjust[r];
            regions << QVariantMap{
                {"name", cr.name}, {"original", cr.mean}, {"color", adjusted(cr.mean, a)}, {"share", cr.share},
                {"hue", a.hue}, {"sat", a.sat}, {"val", a.val}, {"contrast", a.contrast}, {"edited", !a.isIdentity()}};
        }
        QString label = p.submesh;
        const QString low = p.submesh.toLower();
        if (low == "body") label = "Gövde";
        else if (low == "head") label = "Kafa";
        else if (low == "parts" || low.contains("weapon") || low.contains("sword")) label = "Silah";
        out << QVariantMap{{"name", p.submesh}, {"label", label}, {"texture", p.texLabel},
                           {"size", QString("%1×%2").arg(p.original.width()).arg(p.original.height())},
                           {"edited", p.edited()}, {"regions", regions}};
    }
    return out;
}

void AppController::setSelectedPart(int p) {
    if (!project_ || p == selPart_ || p < 0 || p >= partCount()) return;
    const int old = selPart_;
    selPart_ = p; selRegion_ = -1;
    emit selectionChanged();
    emit partTextureChanged(old);
    emit partTextureChanged(p);
    ++revision_; emit imagesChanged();
}

void AppController::setSelectedRegion(int r) {
    if (r == selRegion_) return;
    selRegion_ = r;
    emit selectionChanged();
    emit partTextureChanged(selPart_);
    ++revision_; emit imagesChanged();
}

void AppController::touchPart(int part) {
    partStale_[size_t(part)] = true;
    textureTimer_.start();
    edited();
}

void AppController::rebuildPartTexture(int part) {
    partStale_[size_t(part)] = false;
    const ModelPart &p = project_->parts[size_t(part)];
    partImages_[size_t(part)] = p.edited() ? applyRegions(p.original, p.regions, p.adjust) : p.original;
    emit partTextureChanged(part);
    ++revision_; emit imagesChanged();
}

void AppController::setRegion(int part, int region, const QString &key, double value) {
    if (!project_ || part < 0 || part >= partCount()) return;
    auto &adj = project_->parts[size_t(part)].adjust;
    if (region < 0 || region >= int(adj.size())) return;
    RegionAdjust &a = adj[size_t(region)];
    if (key == "hue") a.hue = float(value);
    else if (key == "sat") a.sat = float(value);
    else if (key == "val") a.val = float(value);
    else if (key == "contrast") a.contrast = float(value);
    touchPart(part);
}

void AppController::setRegionColor(int part, int region, const QColor &c) {
    if (!project_ || part < 0 || part >= partCount()) return;
    ModelPart &p = project_->parts[size_t(part)];
    if (region < 0 || region >= int(p.adjust.size())) return;
    const QColor m = p.regions.regions[size_t(region)].mean;
    float h0, s0, v0, h1, s1, v1;
    rgbToHsv(m.redF(), m.greenF(), m.blueF(), h0, s0, v0);
    rgbToHsv(c.redF(), c.greenF(), c.blueF(), h1, s1, v1);
    RegionAdjust &a = p.adjust[size_t(region)];
    a.hue = std::fmod(h1 - h0 + 540.0f, 360.0f) - 180.0f;
    a.sat = std::clamp(s1 / std::max(s0, 0.04f), 0.0f, 4.0f);
    a.val = std::clamp(v1 / std::max(v0, 0.03f), 0.0f, 6.0f);
    touchPart(part);
}

void AppController::resetRegion(int part, int region) {
    if (!project_ || part < 0 || part >= partCount()) return;
    auto &adj = project_->parts[size_t(part)].adjust;
    if (region < 0 || region >= int(adj.size())) return;
    adj[size_t(region)] = RegionAdjust{};
    touchPart(part);
}

void AppController::resetPart(int part) {
    if (!project_ || part < 0 || part >= partCount()) return;
    for (auto &a : project_->parts[size_t(part)].adjust) a = RegionAdjust{};
    touchPart(part);
}

void AppController::pickUv(int part, double u, double v) {
    if (!project_ || part < 0 || part >= partCount()) return;
    const RegionMap &m = project_->parts[size_t(part)].regions;
    int x = std::clamp(int(u * m.w), 0, m.w - 1), y = std::clamp(int(v * m.h), 0, m.h - 1);
    int r = m.regionAt(x, y);
    if (part != selPart_) { selPart_ = part; selRegion_ = -2; }
    setSelectedRegion(r);
}

// ------------------------------------------------------------------ images

QImage AppController::image(const QString &id) {
    if (!project_) return {};
    if (id.startsWith("part/")) {
        int i = id.mid(5).section('/', 0, 0).toInt();
        if (i < 0 || i >= int(partImages_.size())) return {};
        if (i == selPart_ && selRegion_ >= 0)
            return applyRegions(project_->parts[size_t(i)].original, project_->parts[size_t(i)].regions,
                                project_->parts[size_t(i)].adjust, selRegion_);
        return partImages_[size_t(i)];
    }
    if (id.startsWith("texture/")) {   // plain current texture (no highlight)
        int i = id.mid(8).section('/', 0, 0).toInt();
        return i >= 0 && i < int(partImages_.size()) ? partImages_[size_t(i)] : QImage();
    }
    if (id.startsWith("loadscreen")) return project_->loadscreenChanged ? project_->newLoadscreen : project_->loadscreen;
    if (id.startsWith("lsoriginal")) return project_->loadscreen;
    if (id.startsWith("lsource")) return loadscreenSource_;
    if (id.startsWith("cover")) return project_->cover;
    if (id.startsWith("vfx/")) return project_->previewVfxTexture(id.mid(4).section('/', 0, 0));
    return {};
}

// ------------------------------------------------------------------ animation

QVariantList AppController::animations() const {
    QVariantList out;
    if (!project_) return out;
    for (const auto &a : project_->animations) out << QVariantMap{{"label", a.label}, {"clip", a.clip}};
    return out;
}

void AppController::setAnimation(int i) {
    if (!project_) return;
    if (i < 0 || i >= int(project_->animations.size()) || !project_->hasSkeleton) {
        anim_ = -1; clip_.reset(); frameTimer_.stop(); emit animationChanged(); computePose(); return;
    }
    try {
        clip_ = std::make_unique<Animation>(project_->loadAnimation(project_->animations[size_t(i)].file));
    } catch (const std::exception &e) {
        clip_.reset();
        setStatus(QString("Animasyon okunamadı: ") + e.what());
    }
    anim_ = i;
    trackJoint_.clear();
    if (clip_) for (const auto &t : clip_->tracks) trackJoint_.push_back(project_->skeleton.indexOfHash(t.jointHash));
    time_ = 0;
    clock_.restart();
    if (playing_ && clip_) frameTimer_.start();
    emit animationChanged();
    emit timeChanged();
    updateMarkers();
    computePose();
}

void AppController::setPlaying(bool p) {
    if (p == playing_) return;
    playing_ = p;
    if (p && clip_) { clock_.restart(); frameTimer_.start(); } else frameTimer_.stop();
    emit animationChanged();
}

void AppController::setAnimTime(double t) {
    if (!clip_) return;
    time_ = std::clamp(t, 0.0, double(clip_->duration));
    clock_.restart();
    emit timeChanged();
    computePose();
}

void AppController::selectAnimationByClip(const QString &prefix, double time) {
    if (!project_) return;
    for (int i = 0; i < int(project_->animations.size()); ++i)
        if (project_->animations[size_t(i)].clip.toLower().startsWith(prefix.toLower())) {
            setAnimation(i); setPlaying(false); setAnimTime(time); return;
        }
}

void AppController::tick() {
    if (!clip_ || !playing_) return;
    const double dt = clock_.restart() / 1000.0;
    time_ += std::min(dt, 0.1);
    if (clip_->duration > 0) time_ = std::fmod(time_, double(clip_->duration));
    emit timeChanged();
    computePose();
}

void AppController::computePose() {
    if (!project_ || !project_->hasSkeleton) { skin_.clear(); emit poseChanged(); return; }
    const Skeleton &sk = project_->skeleton;
    const size_t n = sk.joints.size();
    std::vector<QVector3D> T(n), S(n);
    std::vector<QQuaternion> R(n);
    for (size_t j = 0; j < n; ++j) { T[j] = sk.joints[j].localT; R[j] = sk.joints[j].localR; S[j] = sk.joints[j].localS; }
    if (clip_) {
        for (size_t k = 0; k < clip_->tracks.size(); ++k) {
            int j = trackJoint_[k];
            if (j < 0) continue;
            clip_->sample(clip_->tracks[k], float(time_), T[size_t(j)], R[size_t(j)], S[size_t(j)]);
        }
    }
    std::vector<QMatrix4x4> G(n);
    for (size_t j = 0; j < n; ++j) {
        QMatrix4x4 local = trs(T[j], R[j], S[j]);
        const int p = sk.joints[j].parent;
        G[j] = (p >= 0 && size_t(p) < j) ? G[size_t(p)] * local : (p >= 0 ? local : local);
    }
    // parents may come after children in rare skeletons: second pass fixes those
    for (size_t j = 0; j < n; ++j) {
        const int p = sk.joints[j].parent;
        if (p >= 0 && size_t(p) > j) G[j] = G[size_t(p)] * trs(T[j], R[j], S[j]);
    }
    skin_.resize(sk.influences.size());
    for (size_t i = 0; i < sk.influences.size(); ++i) {
        const size_t j = sk.influences[i];
        skin_[i] = j < n ? G[j] * sk.joints[j].inverseBind : QMatrix4x4();
    }
    emit poseChanged();
}

// ------------------------------------------------------------------ effects

QVariantList AppController::effects() const {
    QVariantList out;
    if (!project_) return out;
    for (const auto &g : project_->vfx.groups()) {
        auto it = project_->vfxTargets.find(g.key);
        const bool en = it != project_->vfxTargets.end() && it->second.enabled;
        out << QVariantMap{{"key", g.key}, {"label", g.label}, {"detail", g.detail}, {"hasColor", g.hasColor},
                           {"original", g.current()}, {"color", en ? it->second.color : g.current()}, {"enabled", en},
                           {"systems", int(g.entries.size())}, {"textures", int(g.textures.size())}};
    }
    return out;
}

void AppController::setEffect(const QString &key, bool enabled, const QColor &color) {
    if (!project_) return;
    project_->vfxTargets[key] = VfxTarget{enabled, color};
    ++revision_; emit imagesChanged();
    edited();
}

void AppController::setAllEffects(const QColor &color) {
    if (!project_) return;
    for (const auto &g : project_->vfx.groups())
        if (g.hasColor && g.key != "O") project_->vfxTargets[g.key] = VfxTarget{true, color};
    ++revision_; emit imagesChanged();
    edited();
}

void AppController::resetEffects() {
    if (!project_) return;
    for (auto &[k, t] : project_->vfxTargets) t.enabled = false;
    ++revision_; emit imagesChanged();
    edited();
}

static int findAnim(const SkinProject &p, const QStringList &prefixes) {
    for (const QString &pre : prefixes)
        for (int i = 0; i < int(p.animations.size()); ++i)
            if (p.animations[size_t(i)].clip.toLower().startsWith(pre)) return i;
    return -1;
}

void AppController::previewEffect(const QString &key) {
    if (!project_) return;
    static const std::map<QString, QStringList> m = {
        {"P", {"run"}}, {"Q", {"spell1"}}, {"W", {"spell2", "channel"}}, {"E", {"spell3_0", "spell3"}},
        {"R", {"spell4"}}, {"A", {"attack1", "crit"}}, {"I", {"idle1"}}, {"O", {"idle1"}}};
    auto it = m.find(key);
    int a = it != m.end() ? findAnim(*project_, it->second) : -1;
    if (a >= 0) { setAnimation(a); setPlaying(true); }
}

// ------------------------------------------------------------------ voice

static bool isSilent(const PcmClip &c) {
    if (c.isEmpty()) return false;
    int peak = 0; for (int16_t s : c.samples) peak = std::max(peak, std::abs(int(s)));
    return peak < 200;
}

QVariantList AppController::voice() const {
    QVariantList out;
    if (!project_) return out;
    for (size_t i = 0; i < project_->voice.size(); ++i) {
        const VoiceLine &v = project_->voice[i];
        const char *mode = v.mode == VoiceLine::Replace ? "replace" : v.mode == VoiceLine::Mute ? "mute" : "keep";
        double outDur = 0;
        if (v.mode == VoiceLine::Replace && !v.source.isEmpty()) {
            double end = v.edit.trimEnd < 0 ? v.source.seconds() : v.edit.trimEnd;
            outDur = std::max(0.0, end - v.edit.trimStart);
        }
        out << QVariantMap{
            {"index", int(i)}, {"group", v.group}, {"label", v.label}, {"event", v.event}, {"mode", QString(mode)},
            {"hasOriginal", !v.original.isEmpty()}, {"originalSilent", isSilent(v.original)}, {"originalNote", v.originalNote}, {"originalDur", v.original.seconds()},
            {"hasSource", !v.source.isEmpty()}, {"sourceDur", v.source.seconds()},
            {"trimStart", v.edit.trimStart}, {"trimEnd", v.edit.trimEnd < 0 ? v.source.seconds() : v.edit.trimEnd},
            {"gainDb", v.edit.gainDb}, {"fadeIn", v.edit.fadeInMs}, {"fadeOut", v.edit.fadeOutMs}, {"normalize", v.edit.normalize},
            {"outDur", outDur}, {"variants", int(v.sounds.size())}, {"anim", animForVoice(v)}};
    }
    return out;
}

int AppController::animForVoice(const VoiceLine &v) const {
    if (!project_) return -1;
    const QString l = v.label.toLower();
    QStringList pre;
    if (v.group == "Q") pre = {"spell1"};
    else if (v.group == "W") pre = {"spell2", "channel"};
    else if (v.group == "E") pre = {"spell3_0", "spell3"};
    else if (v.group == "R") pre = {"spell4"};
    else if (v.group == "A") pre = l.contains("kritik") ? QStringList{"crit"} : QStringList{"attack1"};
    else if (v.group == "M") pre = l.contains("geri") ? QStringList{"recall"} : QStringList{"run"};
    else if (v.group == "D") pre = l.contains("doğ") ? QStringList{"respawn"} : QStringList{"death"};
    else if (l.startsWith("şaka")) pre = {"joke"};
    else if (l.startsWith("kışkırt")) pre = {"taunt"};
    else if (l.startsWith("gülme")) pre = {"laugh"};
    else if (l.startsWith("dans")) pre = {"dance"};
    else pre = {"taunt", "idle1"};
    return findAnim(*project_, pre);
}

void AppController::updateMarkers() {
    markers_.clear();
    if (project_)
        for (const auto &v : project_->voice)
            if (anim_ >= 0 && animForVoice(v) == anim_)
                markers_ << QVariantMap{{"label", v.label}, {"mode", v.mode == VoiceLine::Replace ? "replace" : v.mode == VoiceLine::Mute ? "mute" : "keep"}};
    emit markersChanged();
}

void AppController::voiceFromFile(int line, const QUrl &url) {
    if (!project_ || line < 0 || line >= int(project_->voice.size())) return;
    QString why;
    PcmClip c = AudioEngine::decodeFile(url.toLocalFile(), &why);
    if (c.isEmpty()) { setError(why); return; }
    VoiceLine &v = project_->voice[size_t(line)];
    v.source = c;
    v.mode = VoiceLine::Replace;
    auto r = detectVoiceRange(c);
    v.edit = VoiceEdit{};
    v.edit.trimStart = r.first; v.edit.trimEnd = r.second;
    emit voiceChanged(); updateMarkers(); edited();
    setStatus(QString("%1: %2 s ses yüklendi").arg(v.label).arg(c.seconds(), 0, 'f', 1));
}

void AppController::startRecording(int line) {
    if (!project_ || line < 0 || line >= int(project_->voice.size())) return;
    stopVoice();
    if (recLine_ >= 0) stopRecording();
    QString why;
    if (!audio_.startRecording(&why)) { setError(why); return; }
    recLine_ = line;
    emit recordingChanged();
}

void AppController::stopRecording() {
    if (recLine_ < 0) return;
    PcmClip c = audio_.stopRecording();
    const int line = recLine_;
    recLine_ = -1;
    emit recordingChanged();
    if (c.seconds() < 0.15) { setError("Kayıt çok kısa ya da boş."); return; }
    VoiceLine &v = project_->voice[size_t(line)];
    v.source = c;
    v.mode = VoiceLine::Replace;
    v.edit = VoiceEdit{};
    auto r = detectVoiceRange(c);
    v.edit.trimStart = r.first; v.edit.trimEnd = r.second;
    emit voiceChanged(); updateMarkers(); edited();
}

void AppController::setVoiceMode(int line, const QString &mode) {
    if (!project_ || line < 0 || line >= int(project_->voice.size())) return;
    VoiceLine &v = project_->voice[size_t(line)];
    if (mode == "mute") v.mode = VoiceLine::Mute;
    else if (mode == "replace" && !v.source.isEmpty()) v.mode = VoiceLine::Replace;
    else { v.mode = VoiceLine::Keep; }
    emit voiceChanged(); updateMarkers(); edited();
}

void AppController::setVoiceEdit(int line, const QString &key, double value) {
    if (!project_ || line < 0 || line >= int(project_->voice.size())) return;
    VoiceEdit &e = project_->voice[size_t(line)].edit;
    const double len = project_->voice[size_t(line)].source.seconds();
    if (key == "trimStart") e.trimStart = std::clamp(value, 0.0, std::max(0.0, (e.trimEnd < 0 ? len : e.trimEnd) - 0.05));
    else if (key == "trimEnd") e.trimEnd = std::clamp(value, e.trimStart + 0.05, len);
    else if (key == "gainDb") e.gainDb = value;
    else if (key == "fadeIn") e.fadeInMs = value;
    else if (key == "fadeOut") e.fadeOutMs = value;
    else if (key == "normalize") e.normalize = value != 0;
    emit voiceChanged(); edited();
}

void AppController::autoTrim(int line) {
    if (!project_ || line < 0 || line >= int(project_->voice.size())) return;
    VoiceLine &v = project_->voice[size_t(line)];
    if (v.source.isEmpty()) return;
    auto r = detectVoiceRange(v.source);
    v.edit.trimStart = r.first; v.edit.trimEnd = r.second;
    emit voiceChanged(); edited();
}

void AppController::playVoice(int line, bool withAnimation) {
    if (!project_ || line < 0 || line >= int(project_->voice.size())) return;
    const VoiceLine &v = project_->voice[size_t(line)];
    PcmClip c = v.mode == VoiceLine::Keep ? v.original : v.processed();
    if (withAnimation) {
        int a = animForVoice(v);
        if (a >= 0) { setAnimation(a); setPlaying(true); }
    }
    if (c.isEmpty()) { setStatus(v.mode == VoiceLine::Keep ? "Bu sesin önizlemesi yok (" + v.originalNote + ")" : "Sessiz"); return; }
    audio_.play(c);
    playLine_ = line;
    emit playingLineChanged();
}

void AppController::stopVoice() {
    audio_.stop();
    if (playLine_ >= 0) { playLine_ = -1; emit playingLineChanged(); }
}

static QVariantList toList(const std::vector<float> &p) {
    QVariantList l; l.reserve(qsizetype(p.size()));
    for (float x : p) l << x;
    return l;
}

QVariantList AppController::voicePeaks(int line, int buckets) const {
    if (!project_ || line < 0 || line >= int(project_->voice.size())) return {};
    const VoiceLine &v = project_->voice[size_t(line)];
    PcmClip c = v.mode == VoiceLine::Replace ? v.processed() : v.mode == VoiceLine::Mute ? PcmClip{} : v.original;
    return c.isEmpty() ? QVariantList{} : toList(peaks(c, buckets));
}

QVariantList AppController::sourcePeaks(int line, int buckets) const {
    if (!project_ || line < 0 || line >= int(project_->voice.size())) return {};
    const VoiceLine &v = project_->voice[size_t(line)];
    return v.source.isEmpty() ? QVariantList{} : toList(peaks(v.source, buckets));
}

// ------------------------------------------------------------------ loading screen / cover

QSize AppController::imageSize(const QUrl &url) const {
    QImage img(url.toLocalFile());
    if (!img.isNull()) {
        const_cast<AppController *>(this)->loadscreenSource_ = img.convertToFormat(QImage::Format_RGBA8888);
        const_cast<AppController *>(this)->revision_++;
        emit const_cast<AppController *>(this)->imagesChanged();
    }
    return img.size();
}

bool AppController::setLoadscreen(const QUrl &url, double x, double y, double w, double h) {
    if (!project_) return false;
    QImage src = loadscreenSource_;
    if (src.isNull() || !url.isEmpty()) src = QImage(url.toLocalFile());
    if (src.isNull()) { setError("Görsel açılamadı."); return false; }
    const QRect r(int(std::lround(x * src.width())), int(std::lround(y * src.height())),
                  std::max(1, int(std::lround(w * src.width()))), std::max(1, int(std::lround(h * src.height()))));
    QImage crop = src.copy(r.intersected(src.rect()));
    project_->newLoadscreen = crop.scaled(project_->loadscreenSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                  .convertToFormat(QImage::Format_RGBA8888);
    project_->loadscreenChanged = true;
    ++revision_; emit imagesChanged();
    edited();
    return true;
}

void AppController::resetLoadscreen() {
    if (!project_) return;
    project_->loadscreenChanged = false;
    project_->newLoadscreen = {};
    ++revision_; emit imagesChanged();
    edited();
}

void AppController::coverFromLoadscreen() {
    if (!project_) return;
    const QImage ls = project_->loadscreenChanged ? project_->newLoadscreen : project_->loadscreen;
    if (ls.isNull()) return;
    project_->cover = ls;
    project_->coverChanged = true;
    ++revision_; emit imagesChanged();
    edited();
}

bool AppController::setCover(const QUrl &url) {
    if (!project_) return false;
    QImage img(url.toLocalFile());
    if (img.isNull()) { setError("Görsel açılamadı."); return false; }
    project_->coverChanged = true;
    project_->cover = img.width() > 1024 ? img.scaledToWidth(1024, Qt::SmoothTransformation) : img;
    ++revision_; emit imagesChanged();
    edited();
    return true;
}
