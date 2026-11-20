#include "core/Wwise.h"
#include "core/Hashes.h"
#include <cstring>
#include <functional>

namespace lol {

WwiseBank WwiseBank::fromBytes(const QByteArray &data) {
    WwiseBank b;
    Reader r(data);
    while (r.left() >= 8) {
        QByteArray tag = r.bytes(4);
        uint32_t size = r.get<uint32_t>();
        b.sections.push_back({tag, r.bytes(int(size))});
    }
    for (auto &[tag, body] : b.sections) {
        if (tag != "HIRC") continue;
        Reader h(body);
        uint32_t n = h.get<uint32_t>();
        for (uint32_t i = 0; i < n; ++i) {
            Object o; o.type = h.get<uint8_t>();
            uint32_t size = h.get<uint32_t>();
            o.id = h.get<uint32_t>();
            o.body = h.bytes(int(size) - 4);
            b.objects.push_back(std::move(o));
        }
    }
    return b;
}

QByteArray WwiseBank::toBytes() const {
    Writer w;
    for (const auto &[tag, body] : sections) {
        QByteArray payload = body;
        if (tag == "HIRC") {
            Writer h; h.put<uint32_t>(uint32_t(objects.size()));
            for (const auto &o : objects) { h.put<uint8_t>(o.type); h.put<uint32_t>(uint32_t(o.body.size() + 4)); h.put<uint32_t>(o.id); h.bytes(o.body); }
            payload = h.buf;
        }
        w.bytes(tag); w.put<uint32_t>(uint32_t(payload.size())); w.bytes(payload);
    }
    return w.buf;
}

WwiseBank::Object *WwiseBank::object(uint32_t id) {
    for (auto &o : objects) if (o.id == id) return &o;
    return nullptr;
}

void WwiseBank::buildParents() {
    // Containers: 5 random/sequence, 6 switch, 7 actor-mixer, 9 layer. The direct parent id sits inside NodeBaseParams;
    // scan for the first u32 that names a container (the usual Wwise HIRC layout).
    std::set<uint32_t> containers;
    for (const auto &o : objects) if (o.type == 5 || o.type == 6 || o.type == 7 || o.type == 9) containers.insert(o.id);
    for (const auto &o : objects) {
        int start;
        if (o.type == 2) start = 14;
        else if (o.type == 5 || o.type == 6 || o.type == 9) start = 0;
        else continue;
        for (int off = start; off + 4 <= o.body.size(); ++off) {
            uint32_t v; std::memcpy(&v, o.body.constData() + off, 4);
            if (v != o.id && containers.count(v)) { parents_[o.id] = v; break; }
        }
    }
    parentsBuilt_ = true;
}

std::vector<uint32_t> WwiseBank::eventTargets(uint32_t eventId) {
    std::vector<uint32_t> out;
    Object *ev = object(eventId);
    if (!ev || ev->type != 4 || ev->body.isEmpty()) return out;
    const int n = uint8_t(ev->body[0]);
    for (int i = 0; i < n && 1 + 4 * i + 4 <= ev->body.size(); ++i) {
        uint32_t aid; std::memcpy(&aid, ev->body.constData() + 1 + 4 * i, 4);
        Object *a = object(aid);
        if (!a || a->type != 3 || a->body.size() < 6) continue;
        uint32_t target; std::memcpy(&target, a->body.constData() + 2, 4);
        out.push_back(target);
    }
    return out;
}

std::vector<uint32_t> WwiseBank::eventSounds(uint32_t eventId) {
    if (!parentsBuilt_) buildParents();
    std::vector<uint32_t> out;
    std::function<void(uint32_t)> walk = [&](uint32_t node) {
        Object *o = object(node);
        if (!o) return;
        if (o->type == 2) { out.push_back(node); return; }
        for (const auto &[child, parent] : parents_) if (parent == node) walk(child);
    };
    for (uint32_t t : eventTargets(eventId)) walk(t);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

uint32_t WwiseBank::soundSource(const Object &o) {
    uint32_t s = 0;
    if (o.body.size() >= 9) std::memcpy(&s, o.body.constData() + 5, 4);
    return s;
}

void WwiseBank::setSoundSource(Object &o, uint32_t plugin, uint32_t source, uint32_t memSize) {
    std::memcpy(o.body.data(), &plugin, 4);
    std::memcpy(o.body.data() + 5, &source, 4);
    std::memcpy(o.body.data() + 9, &memSize, 4);
}

Wpk Wpk::fromBytes(const QByteArray &data) {
    Reader r(data);
    if (r.bytes(4) != "r3d2") throw FormatError("not a WPK file");
    r.get<uint32_t>();
    uint32_t n = r.get<uint32_t>();
    std::vector<uint32_t> offs(n);
    for (auto &o : offs) o = r.get<uint32_t>();
    Wpk w;
    for (uint32_t off : offs) {
        if (!off) continue;
        r.seek(int(off));
        uint32_t dataOff = r.get<uint32_t>(), size = r.get<uint32_t>(), nameLen = r.get<uint32_t>();
        QByteArray nm = r.bytes(int(nameLen) * 2);
        QString name = QString::fromUtf16(reinterpret_cast<const char16_t *>(nm.constData()), nameLen);
        uint32_t id = name.section('.', 0, 0).toUInt();
        if (qsizetype(dataOff) + size > data.size()) throw FormatError("WPK entry out of range");
        w.wems[id] = data.mid(dataOff, size);
    }
    return w;
}

QByteArray Wpk::toBytes() const {
    // header, offset table, entries (data offset, size, name length, UTF-16 name), then 8-aligned data
    const uint32_t n = uint32_t(wems.size());
    std::vector<QByteArray> names;
    for (const auto &[id, d] : wems) {
        QString s = QString::number(id) + ".wem";
        names.push_back(QByteArray(reinterpret_cast<const char *>(s.utf16()), s.size() * 2));
    }
    uint32_t pos = 12 + 4 * n;
    std::vector<uint32_t> entryOff, dataOff;
    for (const auto &nm : names) { entryOff.push_back(pos); pos += 12 + uint32_t(nm.size()); }
    for (const auto &[id, d] : wems) { pos = (pos + 7) & ~7u; dataOff.push_back(pos); pos += uint32_t(d.size()); }
    QByteArray buf(qsizetype(pos), '\0');
    auto put32 = [&](uint32_t at, uint32_t v) { std::memcpy(buf.data() + at, &v, 4); };
    std::memcpy(buf.data(), "r3d2", 4); put32(4, 1); put32(8, n);
    size_t k = 0;
    for (const auto &[id, d] : wems) {
        put32(12 + 4 * uint32_t(k), entryOff[k]);
        put32(entryOff[k], dataOff[k]); put32(entryOff[k] + 4, uint32_t(d.size())); put32(entryOff[k] + 8, uint32_t(names[k].size() / 2));
        std::memcpy(buf.data() + entryOff[k] + 12, names[k].constData(), size_t(names[k].size()));
        std::memcpy(buf.data() + dataOff[k], d.constData(), size_t(d.size()));
        ++k;
    }
    return buf;
}

QByteArray makePcmWem(const PcmClip &clip, uint32_t *headerSize) {
    // 24-byte Wwise fmt (WAVE_FORMAT_EXTENSIBLE tag, mono channel mask), 16-bit PCM
    Writer fmt;
    fmt.put<uint16_t>(0xFFFE); fmt.put<uint16_t>(1); fmt.put<uint32_t>(uint32_t(clip.rate)); fmt.put<uint32_t>(uint32_t(clip.rate * 2));
    fmt.put<uint16_t>(2); fmt.put<uint16_t>(16); fmt.put<uint16_t>(6); fmt.put<uint16_t>(0); fmt.put<uint32_t>(0x4101);
    const QByteArray pcm(reinterpret_cast<const char *>(clip.samples.data()), qsizetype(clip.samples.size() * 2));
    Writer body;
    body.bytes("WAVE", 4); body.bytes("fmt ", 4); body.put<uint32_t>(uint32_t(fmt.buf.size())); body.bytes(fmt.buf);
    body.bytes("data", 4); body.put<uint32_t>(uint32_t(pcm.size())); body.bytes(pcm);
    Writer w; w.bytes("RIFF", 4); w.put<uint32_t>(uint32_t(body.buf.size())); w.bytes(body.buf);
    if (headerSize) *headerSize = uint32_t(12 + 8 + fmt.buf.size() + 8);
    return w.buf;
}

PcmClip decodeWem(const QByteArray &wem, QString *why) {
    PcmClip c;
    try {
        Reader r(wem);
        if (r.bytes(4) != "RIFF") { if (why) *why = "WEM değil"; return c; }
        r.get<uint32_t>();
        if (r.bytes(4) != "WAVE") { if (why) *why = "WEM değil"; return c; }
        uint16_t tag = 0, ch = 1, bits = 16; uint32_t rate = 48000;
        while (r.left() >= 8) {
            QByteArray id = r.bytes(4); uint32_t size = r.get<uint32_t>();
            int next = r.pos() + int(size) + int(size & 1);
            if (id == "fmt ") { tag = r.get<uint16_t>(); ch = r.get<uint16_t>(); rate = r.get<uint32_t>(); r.skip(6); bits = r.get<uint16_t>(); }
            else if (id == "data") {
                if (!(tag == 1 || tag == 0xFFFE) || bits != 16 || ch < 1) {
                    if (why) *why = tag == 0xFFFF ? "orijinal (Vorbis) ses, önizlenemiyor" : "desteklenmeyen ses biçimi";
                    return c;
                }
                const int frames = int(std::min<qsizetype>(size, r.left()) / (2 * ch));
                c.rate = int(rate); c.samples.resize(size_t(frames));
                const int16_t *p = reinterpret_cast<const int16_t *>(r.ptr());
                for (int i = 0; i < frames; ++i) {
                    int s = 0; for (int k = 0; k < ch; ++k) s += p[i * ch + k];
                    c.samples[size_t(i)] = int16_t(s / ch);
                }
                return c;
            }
            if (next > r.size()) break;
            r.seek(next);
        }
    } catch (const FormatError &) {}
    if (why && c.isEmpty()) *why = "ses okunamadı";
    return c;
}

} // namespace lol
