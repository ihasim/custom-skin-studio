#pragma once
// Everything QML talks to: the open project, edits, the animated pose, audio, saving.
#include "app/AudioEngine.h"
#include "core/SkinProject.h"
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QImage>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QQmlEngine>
#include <QJSEngine>
#include <QtQml/qqmlregistration.h>
#include <memory>

class AppController : public QObject {
    Q_OBJECT

    // state
    Q_PROPERTY(QString screen READ screen NOTIFY screenChanged)              // "start" | "loading" | "editor"
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY editsChanged)
    Q_PROPERTY(QString gameDir READ gameDir NOTIFY gameDirChanged)
    Q_PROPERTY(bool gameFound READ gameFound NOTIFY projectChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY projectChanged)
    Q_PROPERTY(QString champion READ champion NOTIFY projectChanged)
    Q_PROPERTY(int revision READ revision NOTIFY imagesChanged)            // bump to refresh image:// urls
    // model
    Q_PROPERTY(int partCount READ partCount NOTIFY projectChanged)
    Q_PROPERTY(QVariantList parts READ parts NOTIFY editsChanged)
    Q_PROPERTY(int selectedPart READ selectedPart WRITE setSelectedPart NOTIFY selectionChanged)
    Q_PROPERTY(int selectedRegion READ selectedRegion WRITE setSelectedRegion NOTIFY selectionChanged)
    Q_PROPERTY(float modelHeight READ modelHeight NOTIFY projectChanged)
    // animation
    Q_PROPERTY(QVariantList animations READ animations NOTIFY projectChanged)
    Q_PROPERTY(int animation READ animation WRITE setAnimation NOTIFY animationChanged)
    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY animationChanged)
    Q_PROPERTY(double animTime READ animTime WRITE setAnimTime NOTIFY timeChanged)
    Q_PROPERTY(double animDuration READ animDuration NOTIFY animationChanged)
    Q_PROPERTY(QVariantList voiceMarkers READ voiceMarkers NOTIFY markersChanged)
    // effects
    Q_PROPERTY(QVariantList effects READ effects NOTIFY editsChanged)
    // voice
    Q_PROPERTY(QVariantList voice READ voice NOTIFY voiceChanged)
    Q_PROPERTY(QString voiceNote READ voiceNote NOTIFY projectChanged)
    Q_PROPERTY(int recordingLine READ recordingLine NOTIFY recordingChanged)
    Q_PROPERTY(double recordLevel READ recordLevel NOTIFY recordLevelChanged)
    Q_PROPERTY(int playingLine READ playingLine NOTIFY playingLineChanged)
    Q_PROPERTY(double playProgress READ playProgress NOTIFY playProgressChanged)
    // loading screen
    Q_PROPERTY(QSize loadscreenSize READ loadscreenSize NOTIFY projectChanged)
    Q_PROPERTY(bool loadscreenChanged READ loadscreenChanged NOTIFY editsChanged)
    Q_PROPERTY(bool hasLoadscreen READ hasLoadscreen NOTIFY projectChanged)
    // package
    Q_PROPERTY(QString pkgName READ pkgName WRITE setPkgName NOTIFY editsChanged)
    Q_PROPERTY(QString pkgAuthor READ pkgAuthor WRITE setPkgAuthor NOTIFY editsChanged)
    Q_PROPERTY(QString pkgVersion READ pkgVersion WRITE setPkgVersion NOTIFY editsChanged)
    Q_PROPERTY(QString pkgDescription READ pkgDescription WRITE setPkgDescription NOTIFY editsChanged)
    Q_PROPERTY(QString suggestedSaveName READ suggestedSaveName NOTIFY editsChanged)
    Q_PROPERTY(QString changeSummary READ changeSummary NOTIFY editsChanged)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;
    static AppController *instance();
    static AppController *create(QQmlEngine *, QJSEngine *);

    QString screen() const { return screen_; }
    QString error() const { return error_; }
    QString status() const { return status_; }
    bool dirty() const { return project_ && project_->hasChanges(); }
    QString gameDir() const { return gameDir_; }
    bool gameFound() const { return project_ && !project_->gameWadPath.isEmpty(); }
    QString fileName() const;
    QString champion() const { return project_ ? project_->champion : QString(); }
    int revision() const { return revision_; }

    int partCount() const { return project_ ? int(project_->parts.size()) : 0; }
    QVariantList parts() const;
    int selectedPart() const { return selPart_; }
    void setSelectedPart(int p);
    int selectedRegion() const { return selRegion_; }
    void setSelectedRegion(int r);
    float modelHeight() const { return modelHeight_; }

    QVariantList animations() const;
    int animation() const { return anim_; }
    void setAnimation(int i);
    bool playing() const { return playing_; }
    void setPlaying(bool p);
    double animTime() const { return time_; }
    void setAnimTime(double t);
    double animDuration() const { return clip_ ? clip_->duration : 0; }
    QVariantList voiceMarkers() const { return markers_; }

    QVariantList effects() const;
    QVariantList voice() const;
    QString voiceNote() const { return project_ ? project_->voiceNote : QString(); }
    int recordingLine() const { return recLine_; }
    double recordLevel() const { return audio_.level(); }
    int playingLine() const { return playLine_; }
    double playProgress() const { return audio_.progress(); }

    QSize loadscreenSize() const { return project_ ? project_->loadscreenSize : QSize(308, 560); }
    bool loadscreenChanged() const { return project_ && project_->loadscreenChanged; }
    bool hasLoadscreen() const { return project_ && !project_->loadscreen.isNull(); }

    QString pkgName() const { return project_ ? project_->name : QString(); }
    QString pkgAuthor() const { return project_ ? project_->author : QString(); }
    QString pkgVersion() const { return project_ ? project_->version : QString(); }
    QString pkgDescription() const { return project_ ? project_->description : QString(); }
    void setPkgName(const QString &s);
    void setPkgAuthor(const QString &s);
    void setPkgVersion(const QString &s);
    void setPkgDescription(const QString &s);
    QString suggestedSaveName() const;
    QString changeSummary() const;

    // ---- for the C++ scene items
    const lol::SkinProject *project() const { return project_.get(); }
    const std::vector<QMatrix4x4> &skinMatrices() const { return skin_; }   // per SKN influence
    QImage image(const QString &id);

    // ---- QML API
    Q_INVOKABLE void openFantome(const QUrl &url);
    Q_INVOKABLE void closeProject();
    Q_INVOKABLE void setGameDir(const QUrl &url);
    Q_INVOKABLE bool save(const QUrl &url);
    Q_INVOKABLE QUrl saveFolder() const;
    Q_INVOKABLE void clearError() { setError({}); }

    Q_INVOKABLE void setRegion(int part, int region, const QString &key, double value);
    Q_INVOKABLE void setRegionColor(int part, int region, const QColor &c);
    Q_INVOKABLE void resetRegion(int part, int region);
    Q_INVOKABLE void resetPart(int part);
    Q_INVOKABLE void pickUv(int part, double u, double v);

    Q_INVOKABLE void setEffect(const QString &key, bool enabled, const QColor &color);
    Q_INVOKABLE void setAllEffects(const QColor &color);
    Q_INVOKABLE void resetEffects();
    Q_INVOKABLE void previewEffect(const QString &key);   // plays the matching animation

    Q_INVOKABLE void voiceFromFile(int line, const QUrl &url);
    Q_INVOKABLE void startRecording(int line);
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE void setVoiceMode(int line, const QString &mode);   // "keep" | "mute" | "replace"
    Q_INVOKABLE void setVoiceEdit(int line, const QString &key, double value);
    Q_INVOKABLE void autoTrim(int line);
    Q_INVOKABLE void playVoice(int line, bool withAnimation = true);
    Q_INVOKABLE void stopVoice();
    Q_INVOKABLE QVariantList voicePeaks(int line, int buckets) const;
    Q_INVOKABLE QVariantList sourcePeaks(int line, int buckets) const;

    Q_INVOKABLE QSize imageSize(const QUrl &url) const;
    Q_INVOKABLE bool setLoadscreen(const QUrl &url, double x, double y, double w, double h);
    Q_INVOKABLE void resetLoadscreen();
    Q_INVOKABLE void coverFromLoadscreen();
    Q_INVOKABLE bool setCover(const QUrl &url);

    // test hook: render without user input
    Q_INVOKABLE void selectAnimationByClip(const QString &prefix, double time);

signals:
    void screenChanged();
    void errorChanged();
    void statusChanged();
    void gameDirChanged();
    void projectChanged();
    void editsChanged();
    void imagesChanged();
    void selectionChanged();
    void animationChanged();
    void timeChanged();
    void poseChanged();
    void partTextureChanged(int part);
    void voiceChanged();
    void recordingChanged();
    void recordLevelChanged();
    void playingLineChanged();
    void playProgressChanged();
    void markersChanged();
    void saved(const QString &path, const QString &report);
    void loaded();

private:
    void setScreen(const QString &s) { if (s != screen_) { screen_ = s; emit screenChanged(); } }
    void setError(const QString &e) { error_ = e; emit errorChanged(); }
    void setStatus(const QString &s) { status_ = s; emit statusChanged(); }
    void finishLoad();
    void tick();
    void computePose();
    void touchPart(int part);
    void rebuildPartTexture(int part);
    void edited() { emit editsChanged(); }
    int animForVoice(const lol::VoiceLine &v) const;
    void updateMarkers();

    std::unique_ptr<lol::SkinProject> project_;
    QFutureWatcher<QString> loadWatch_;
    std::unique_ptr<lol::SkinProject> loading_;
    QString screen_ = "start", error_, status_, gameDir_;
    int revision_ = 0;
    int selPart_ = 0, selRegion_ = -1;
    float modelHeight_ = 200;
    std::vector<QImage> partImages_;   // current (edited) textures, without highlight
    std::vector<bool> partStale_;
    QTimer textureTimer_;

    // animation
    int anim_ = -1;
    std::unique_ptr<lol::Animation> clip_;
    std::vector<int> trackJoint_;
    bool playing_ = true;
    double time_ = 0;
    QTimer frameTimer_;
    QElapsedTimer clock_;
    std::vector<QMatrix4x4> skin_;
    QVariantList markers_;

    // audio
    AudioEngine audio_;
    int recLine_ = -1, playLine_ = -1;
    QImage loadscreenSource_;
};
