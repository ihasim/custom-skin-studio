#pragma once
// Plays mono PCM clips, records from the microphone and decodes audio files (any format the platform decodes).
#include "core/Wwise.h"
#include <QAudioFormat>
#include <QBuffer>
#include <QObject>
#include <QTimer>
#include <memory>

class QAudioSink;
class QAudioSource;
class QIODevice;

class AudioEngine : public QObject {
    Q_OBJECT
public:
    explicit AudioEngine(QObject *parent = nullptr);
    ~AudioEngine() override;

    void play(const lol::PcmClip &clip);
    void stop();
    bool isPlaying() const { return playing_; }
    double progress() const;             // 0..1 of the current clip
    double level() const { return level_; }

    // Microphone. `start` asks for permission when needed; returns false with a reason.
    bool startRecording(QString *why);
    lol::PcmClip stopRecording();
    bool isRecording() const { return source_ != nullptr; }

    static lol::PcmClip decodeFile(const QString &path, QString *why);

signals:
    void finished();
    void progressChanged();
    void levelChanged();

private:
    std::unique_ptr<QAudioSink> sink_;
    QBuffer buffer_;
    QByteArray data_;
    bool playing_ = false;
    std::unique_ptr<QAudioSource> source_;
    QIODevice *recDev_ = nullptr;
    QByteArray recorded_;
    QAudioFormat recFormat_;
    double level_ = 0;
    QTimer poll_;
};
