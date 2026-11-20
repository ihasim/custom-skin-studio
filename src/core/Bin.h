#pragma once
// League property BIN ("PROP" v1-3): typed tree of entries -> fields -> values. Read and written byte-exact.
#include <QByteArray>
#include <QString>
#include <QVector4D>
#include <cstdint>
#include <functional>
#include <vector>

namespace lol {

enum class BinType : uint8_t {
    None = 0, Bool, I8, U8, I16, U16, I32, U32, I64, U64, F32, Vec2, Vec3, Vec4, Mtx4, RGBA, String, Hash, File,
    List = 128, List2, Pointer, Embed, Link, Option, Map, Flag
};

struct BinField;

struct BinValue {
    BinType type = BinType::None;
    QByteArray raw;                 // payload of fixed-size types; UTF-8 text for String
    BinType elemType = BinType::None, keyType = BinType::None;
    uint32_t cls = 0;               // Pointer/Embed class hash, 0 = null pointer
    std::vector<BinField> fields;   // Pointer/Embed
    std::vector<BinValue> items;    // List/List2/Option items, Map values
    std::vector<BinValue> keys;     // Map keys

    bool isStruct() const { return type == BinType::Pointer || type == BinType::Embed; }
    bool isContainer() const { return type == BinType::List || type == BinType::List2 || type == BinType::Option || type == BinType::Map; }
    BinField *field(uint32_t name);
    const BinField *field(uint32_t name) const;

    float f32() const;
    uint32_t u32() const;
    uint64_t u64() const;
    QString string() const { return QString::fromUtf8(raw); }
    QVector4D vec4() const;
    void setVec4(const QVector4D &v);
    void setString(const QString &s) { raw = s.toUtf8(); }
    void setU64(uint64_t v);

    static BinValue makeString(const QString &s);
};

struct BinField {
    uint32_t name = 0;
    BinValue value;
};

inline BinValue BinValue::makeString(const QString &s) { BinValue v; v.type = BinType::String; v.setString(s); return v; }

struct BinEntry {
    uint32_t cls = 0;    // entry class (type) hash
    uint32_t name = 0;   // entry path hash
    std::vector<BinField> fields;
    BinField *field(uint32_t n);
    const BinField *field(uint32_t n) const;
};

struct BinFile {
    uint32_t version = 3;
    bool isPatch = false;
    QByteArray patchHeader;           // 8 bytes after "PTCH"
    std::vector<QByteArray> links;
    std::vector<BinEntry> entries;
    QByteArray tail;                  // patch section (kept as-is)

    static BinFile fromBytes(const QByteArray &data);
    QByteArray toBytes() const;

    BinEntry *entry(uint32_t name);
    std::vector<BinEntry *> entriesOfClass(uint32_t cls);
};

// Visit every value below `v` (depth first); the callback gets the value and the name of the field holding it
// (0 inside lists/maps).
void visitValues(BinValue &v, uint32_t fieldName, const std::function<void(BinValue &, uint32_t)> &fn);
void visitEntry(BinEntry &e, const std::function<void(BinValue &, uint32_t)> &fn);

} // namespace lol
