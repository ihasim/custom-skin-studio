#include "app/AudioEngine.h"
#include "core/AudioDsp.h"
#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QAudioSink>
#include <QAudioSource>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QMediaDevices>
#include <QPermissions>
#include <QTimer>
#include <QUrl>
#include <cmath>

AudioEngine::AudioEngine(QObject *parent) : QObject(parent) {
    poll_.setInterval(40);
    connect(&poll_, &QTimer::timeout, this, [this] {
        if (playing_) {
            emit progressChanged();
            if (sink_ && (sink_->state() == QAudio::IdleState || sink_->state() == QAudio::StoppedState)) {
                playing_ = false;
                sink_->stop();
                emit progressChanged();
                emit finished();
            }
        }
        if (recDev_) {
            const QByteArray chunk = recDev_->readAll();
            if (!chunk.isEmpty()) {
                // convert whatever the device gives to mono int16
                const int ch = recFormat_.channelCount();
                const int bps = recFormat_.bytesPerSample();
                const int frames = int(chunk.size() / (bps * ch));
                double peak = 0;
                QByteArray mono(qsizetype(frames) * 2, '\0');
                auto *out = reinterpret_cast<int16_t *>(mono.data());
                for (int i = 0; i < frames; ++i) {
                    double s = 0;
                    for (int c = 0; c < ch; ++c) {
                        const char *p = chunk.constData() + (i * ch + c) * bps;
                        s += recFormat_.normalizedSampleValue(p);
                    }
                    s /= ch;
                    peak = std::max(peak, std::abs(s));
                    out[i] = int16_t(std::lround(std::clamp(s, -1.0, 1.0) * 32767));
                }
                recorded_.append(mono);
                level_ = std::max(peak, level_ * 0.8);
                emit levelChanged();
            }
        }
        if (!playing_ && !recDev_) poll_.stop();
    });
}

AudioEngine::~AudioEngine() { stop(); }

void AudioEngine::play(const lol::PcmClip &clip) {
    stop();
    if (clip.isEmpty()) return;
    QAudioFormat f;
    f.setSampleRate(clip.rate);
    f.setChannelCount(1);
    f.setSampleFormat(QAudioFormat::Int16);
    const QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    sink_ = std::make_unique<QAudioSink>(dev, f);
    data_ = QByteArray(reinterpret_cast<const char *>(clip.samples.data()), qsizetype(clip.samples.size() * 2));
    buffer_.close();
    buffer_.setData(data_);
    buffer_.open(QIODevice::ReadOnly);
    sink_->start(&buffer_);
    playing_ = true;
    poll_.start();
}

void AudioEngine::stop() {
    if (sink_) { sink_->stop(); sink_.reset(); }
    if (playing_) { playing_ = false; emit progressChanged(); }
}

double AudioEngine::progress() const {
    if (!playing_ || data_.isEmpty()) return 0;
    return std::clamp(double(buffer_.pos()) / double(data_.size()), 0.0, 1.0);
}

bool AudioEngine::startRecording(QString *why) {
#if QT_CONFIG(permissions)
    QMicrophonePermission perm;
    switch (qApp->checkPermission(perm)) {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(perm, this, [](const QPermission &) {});
        if (why) *why = "Mikrofon izni istendi; izin verdikten sonra tekrar kayda bas.";
        return false;
    case Qt::PermissionStatus::Denied:
        if (why) *why = "Mikrofon izni kapalı. Sistem ayarlarından Custom Skin Studio'ya mikrofon izni ver.";
        return false;
    case Qt::PermissionStatus::Granted: break;
    }
#endif
    const QAudioDevice dev = QMediaDevices::defaultAudioInput();
    if (dev.isNull()) { if (why) *why = "Mikrofon bulunamadı."; return false; }
    QAudioFormat f;
    f.setSampleRate(48000);
    f.setChannelCount(1);
    f.setSampleFormat(QAudioFormat::Int16);
    if (!dev.isFormatSupported(f)) f = dev.preferredFormat();
    recFormat_ = f;
    source_ = std::make_unique<QAudioSource>(dev, f);
    recorded_.clear();
    level_ = 0;
    recDev_ = source_->start();
    if (!recDev_) { source_.reset(); if (why) *why = "Kayıt başlatılamadı."; return false; }
    poll_.start();
    return true;
}

lol::PcmClip AudioEngine::stopRecording() {
    lol::PcmClip c;
    if (!source_) return c;
    if (recDev_) { recDev_ = nullptr; }
    source_->stop();
    source_.reset();
    c.rate = recFormat_.sampleRate();
    c.samples.resize(size_t(recorded_.size() / 2));
    std::memcpy(c.samples.data(), recorded_.constData(), c.samples.size() * 2);
    level_ = 0;
    emit levelChanged();
    return c;
}

static lol::PcmClip parseWav(const QByteArray &d) {
    // plain RIFF/WAVE fallback (also reads Wwise-style PCM WEM)
    return lol::decodeWem(d, nullptr);
}

lol::PcmClip AudioEngine::decodeFile(const QString &path, QString *why) {
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) {
        const QByteArray head = f.peek(64);
        if (head.startsWith("RIFF")) {
            lol::PcmClip c = parseWav(f.readAll());
            if (!c.isEmpty()) return c;
        }
    }
    QAudioDecoder dec;
    QAudioFormat want;
    want.setSampleRate(48000);
    want.setChannelCount(1);
    want.setSampleFormat(QAudioFormat::Int16);
    dec.setAudioFormat(want);
    dec.setSource(QUrl::fromLocalFile(path));
    lol::PcmClip out;
    out.rate = 48000;
    QString err;
    QEventLoop loop;
    QObject::connect(&dec, &QAudioDecoder::bufferReady, &loop, [&] {
        const QAudioBuffer b = dec.read();
        const QAudioFormat bf = b.format();
        const int ch = bf.channelCount(), bps = bf.bytesPerSample();
        if (out.samples.empty()) out.rate = bf.sampleRate();
        const char *p = b.constData<char>();
        const int frames = int(b.frameCount());
        for (int i = 0; i < frames; ++i) {
            double s = 0;
            for (int c = 0; c < ch; ++c) s += bf.normalizedSampleValue(p + (i * ch + c) * bps);
            out.samples.push_back(int16_t(std::lround(std::clamp(s / ch, -1.0, 1.0) * 32767)));
        }
    });
    QObject::connect(&dec, &QAudioDecoder::finished, &loop, &QEventLoop::quit);
    QObject::connect(&dec, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error), &loop, [&](QAudioDecoder::Error) {
        err = dec.errorString(); loop.quit();
    });
    QTimer::singleShot(30000, &loop, &QEventLoop::quit);
    dec.start();
    loop.exec();
    if (out.samples.empty() && why) *why = err.isEmpty() ? "ses dosyası okunamadı" : "ses dosyası okunamadı: " + err;
    return out;
}
