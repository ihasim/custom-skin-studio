#include "core/Bin.h"
#include "core/Hashes.h"
#include <cstring>

namespace lol {

static int fixedSize(BinType t) {
    switch (t) {
    case BinType::None: return 6;
    case BinType::Bool: case BinType::I8: case BinType::U8: case BinType::Flag: return 1;
    case BinType::I16: case BinType::U16: return 2;
    case BinType::I32: case BinType::U32: case BinType::F32: case BinType::RGBA: case BinType::Hash: case BinType::Link: return 4;
    case BinType::I64: case BinType::U64: case BinType::Vec2: case BinType::File: return 8;
    case BinType::Vec3: return 12;
    case BinType::Vec4: return 16;
    case BinType::Mtx4: return 64;
    default: return -1;
    }
}

static BinType toType(uint8_t b) {
    if (b <= uint8_t(BinType::File) || (b >= 128 && b <= 135)) return BinType(b);
    throw FormatError("unknown BIN value type " + std::to_string(b));
}

static void readFields(Reader &r, std::vector<BinField> &out, int count);

static BinValue readValue(Reader &r, BinType t) {
    BinValue v; v.type = t;
    int fs = fixedSize(t);
    if (fs >= 0) { v.raw = r.bytes(fs); return v; }
    switch (t) {
    case BinType::String: { uint16_t n = r.get<uint16_t>(); v.raw = r.bytes(n); break; }
    case BinType::List: case BinType::List2: {
        v.elemType = toType(r.get<uint8_t>());
        r.get<uint32_t>();
        uint32_t n = r.get<uint32_t>();
        v.items.reserve(n);
        for (uint32_t i = 0; i < n; ++i) v.items.push_back(readValue(r, v.elemType));
        break;
    }
    case BinType::Pointer: case BinType::Embed: {
        v.cls = r.get<uint32_t>();
        if (v.cls) { r.get<uint32_t>(); uint16_t n = r.get<uint16_t>(); readFields(r, v.fields, n); }
        break;
    }
    case BinType::Option: {
        v.elemType = toType(r.get<uint8_t>());
        uint8_t n = r.get<uint8_t>();
        if (n) v.items.push_back(readValue(r, v.elemType));
        break;
    }
    case BinType::Map: {
        v.keyType = toType(r.get<uint8_t>()); v.elemType = toType(r.get<uint8_t>());
        r.get<uint32_t>();
        uint32_t n = r.get<uint32_t>();
        for (uint32_t i = 0; i < n; ++i) { v.keys.push_back(readValue(r, v.keyType)); v.items.push_back(readValue(r, v.elemType)); }
        break;
    }
    default: throw FormatError("bad BIN type");
    }
    return v;
}

static void readFields(Reader &r, std::vector<BinField> &out, int count) {
    out.reserve(size_t(count));
    for (int i = 0; i < count; ++i) {
        BinField f; f.name = r.get<uint32_t>();
        BinType t = toType(r.get<uint8_t>());
        f.value = readValue(r, t);
        out.push_back(std::move(f));
    }
}

BinFile BinFile::fromBytes(const QByteArray &data) {
    Reader r(data);
    BinFile b;
    QByteArray magic = r.bytes(4);
    if (magic == "PTCH") { b.isPatch = true; b.patchHeader = r.bytes(8); magic = r.bytes(4); }
    if (magic != "PROP") throw FormatError("not a BIN file");
    b.version = r.get<uint32_t>();
    if (b.version < 1 || b.version > 3) throw FormatError("unsupported BIN version");
    if (b.version >= 2) {
        uint32_t n = r.get<uint32_t>();
        for (uint32_t i = 0; i < n; ++i) { uint16_t l = r.get<uint16_t>(); b.links.push_back(r.bytes(l)); }
    }
    uint32_t n = r.get<uint32_t>();
    std::vector<uint32_t> types(n);
    for (auto &t : types) t = r.get<uint32_t>();
    b.entries.resize(n);
    for (uint32_t i = 0; i < n; ++i) {
        BinEntry &e = b.entries[i];
        e.cls = types[i];
        uint32_t size = r.get<uint32_t>();
        int start = r.pos();
        e.name = r.get<uint32_t>();
        uint16_t fc = r.get<uint16_t>();
        readFields(r, e.fields, fc);
        if (r.pos() - start != int(size)) throw FormatError("BIN entry size mismatch");
    }
    b.tail = data.mid(r.pos());
    return b;
}

static void writeFields(Writer &w, const std::vector<BinField> &fields);

static void writeValue(Writer &w, const BinValue &v) {
    int fs = fixedSize(v.type);
    if (fs >= 0) {
        if (v.raw.size() != fs) throw FormatError("BIN value has wrong size");
        w.bytes(v.raw); return;
    }
    switch (v.type) {
    case BinType::String: w.put<uint16_t>(uint16_t(v.raw.size())); w.bytes(v.raw); break;
    case BinType::List: case BinType::List2: {
        w.put<uint8_t>(uint8_t(v.elemType));
        int at = w.pos(); w.put<uint32_t>(0); int start = w.pos();
        w.put<uint32_t>(uint32_t(v.items.size()));
        for (const auto &it : v.items) writeValue(w, it);
        w.patch<uint32_t>(at, uint32_t(w.pos() - start));
        break;
    }
    case BinType::Pointer: case BinType::Embed: {
        w.put<uint32_t>(v.cls);
        if (v.cls) {
            int at = w.pos(); w.put<uint32_t>(0); int start = w.pos();
            w.put<uint16_t>(uint16_t(v.fields.size()));
            writeFields(w, v.fields);
            w.patch<uint32_t>(at, uint32_t(w.pos() - start));
        }
        break;
    }
    case BinType::Option:
        w.put<uint8_t>(uint8_t(v.elemType)); w.put<uint8_t>(uint8_t(v.items.empty() ? 0 : 1));
        if (!v.items.empty()) writeValue(w, v.items[0]);
        break;
    case BinType::Map: {
        w.put<uint8_t>(uint8_t(v.keyType)); w.put<uint8_t>(uint8_t(v.elemType));
        int at = w.pos(); w.put<uint32_t>(0); int start = w.pos();
        w.put<uint32_t>(uint32_t(v.items.size()));
        for (size_t i = 0; i < v.items.size(); ++i) { writeValue(w, v.keys[i]); writeValue(w, v.items[i]); }
        w.patch<uint32_t>(at, uint32_t(w.pos() - start));
        break;
    }
    default: throw FormatError("bad BIN type on write");
    }
}

static void writeFields(Writer &w, const std::vector<BinField> &fields) {
    for (const auto &f : fields) { w.put<uint32_t>(f.name); w.put<uint8_t>(uint8_t(f.value.type)); writeValue(w, f.value); }
}

QByteArray BinFile::toBytes() const {
    Writer w;
    if (isPatch) { w.bytes("PTCH", 4); w.bytes(patchHeader); }
    w.bytes("PROP", 4);
    w.put<uint32_t>(version);
    if (version >= 2) {
        w.put<uint32_t>(uint32_t(links.size()));
        for (const auto &l : links) { w.put<uint16_t>(uint16_t(l.size())); w.bytes(l); }
    }
    w.put<uint32_t>(uint32_t(entries.size()));
    for (const auto &e : entries) w.put<uint32_t>(e.cls);
    for (const auto &e : entries) {
        int at = w.pos(); w.put<uint32_t>(0); int start = w.pos();
        w.put<uint32_t>(e.name); w.put<uint16_t>(uint16_t(e.fields.size()));
        writeFields(w, e.fields);
        w.patch<uint32_t>(at, uint32_t(w.pos() - start));
    }
    w.bytes(tail);
    return w.buf;
}

BinEntry *BinFile::entry(uint32_t name) {
    for (auto &e : entries) if (e.name == name) return &e;
    return nullptr;
}

std::vector<BinEntry *> BinFile::entriesOfClass(uint32_t cls) {
    std::vector<BinEntry *> out;
    for (auto &e : entries) if (e.cls == cls) out.push_back(&e);
    return out;
}

BinField *BinValue::field(uint32_t n) { for (auto &f : fields) if (f.name == n) return &f; return nullptr; }
const BinField *BinValue::field(uint32_t n) const { for (auto &f : fields) if (f.name == n) return &f; return nullptr; }
BinField *BinEntry::field(uint32_t n) { for (auto &f : fields) if (f.name == n) return &f; return nullptr; }
const BinField *BinEntry::field(uint32_t n) const { for (auto &f : fields) if (f.name == n) return &f; return nullptr; }

float BinValue::f32() const { float f = 0; if (raw.size() >= 4) std::memcpy(&f, raw.constData(), 4); return f; }
uint32_t BinValue::u32() const { uint32_t f = 0; if (raw.size() >= 4) std::memcpy(&f, raw.constData(), 4); return f; }
uint64_t BinValue::u64() const { uint64_t f = 0; if (raw.size() >= 8) std::memcpy(&f, raw.constData(), 8); return f; }
void BinValue::setU64(uint64_t v) { raw = QByteArray(reinterpret_cast<const char *>(&v), 8); }

QVector4D BinValue::vec4() const {
    float f[4] = {0, 0, 0, 0};
    if (raw.size() == 16) std::memcpy(f, raw.constData(), 16);
    return {f[0], f[1], f[2], f[3]};
}

void BinValue::setVec4(const QVector4D &v) {
    float f[4] = {v.x(), v.y(), v.z(), v.w()};
    raw = QByteArray(reinterpret_cast<const char *>(f), 16);
}

void visitValues(BinValue &v, uint32_t fieldName, const std::function<void(BinValue &, uint32_t)> &fn) {
    fn(v, fieldName);
    for (auto &f : v.fields) visitValues(f.value, f.name, fn);
    for (auto &it : v.items) visitValues(it, 0, fn);
    for (auto &k : v.keys) visitValues(k, 0, fn);
}

void visitEntry(BinEntry &e, const std::function<void(BinValue &, uint32_t)> &fn) {
    for (auto &f : e.fields) visitValues(f.value, f.name, fn);
}

} // namespace lol
