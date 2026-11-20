#pragma once
// Wwise sound banks as League ships them: events .bnk (HIRC objects) + audio .wpk (WEM files by media id).
#include <QByteArray>
#include <QString>
#include <map>
#include <set>
#include <vector>

namespace lol {

struct WwiseBank {
    struct Object { uint8_t type = 0; uint32_t id = 0; QByteArray body; };   // body excludes the id
    std::vector<std::pair<QByteArray, QByteArray>> sections;                // tag -> payload (HIRC rebuilt on save)
    std::vector<Object> objects;

    static WwiseBank fromBytes(const QByteArray &data);
    QByteArray toBytes() const;
    Object *object(uint32_t id);

    // Sounds (type 2) an event plays, following Event -> Action -> container tree.
    std::vector<uint32_t> eventSounds(uint32_t eventId);
    std::vector<uint32_t> eventTargets(uint32_t eventId);
    // Sound object source access (v145 layout: plugin u32, stream u8, source id u32, media size u32).
    static uint32_t soundSource(const Object &o);
    static void setSoundSource(Object &o, uint32_t plugin, uint32_t source, uint32_t memSize);
private:
    std::map<uint32_t, uint32_t> parents_;   // object -> parent container
    bool parentsBuilt_ = false;
    void buildParents();
};

struct Wpk {
    std::map<uint32_t, QByteArray> wems;
    static Wpk fromBytes(const QByteArray &data);
    QByteArray toBytes() const;
};

constexpr uint32_t kCodecPcm = 0x00010001;

struct PcmClip {
    std::vector<int16_t> samples;   // mono
    int rate = 48000;
    bool isEmpty() const { return samples.empty(); }
    double seconds() const { return rate > 0 ? double(samples.size()) / rate : 0.0; }
};

QByteArray makePcmWem(const PcmClip &clip, uint32_t *headerSize);
PcmClip decodeWem(const QByteArray &wem, QString *why = nullptr);   // PCM only; empty + reason otherwise

} // namespace lol
