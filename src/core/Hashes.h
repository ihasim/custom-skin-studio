#pragma once
// Hash functions League files use to name things.
#include <QByteArray>
#include <QString>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace lol {

uint32_t fnv1a32(const QString &s);        // BIN class/field/entry names (lower-case)
uint32_t fnv1a32(const char *s);
uint32_t wwiseFnv1(const QString &s);      // Wwise event names (FNV-1, lower-case)
uint64_t xxh64Path(const QString &path);   // WAD entry and BIN "file" hashes (lower-case path)
uint64_t xxh3(const QByteArray &data);     // WAD entry checksum

struct FormatError : std::runtime_error { using std::runtime_error::runtime_error; };

// Little-endian cursor over a byte buffer with bounds checks.
class Reader {
public:
    Reader(const QByteArray &d, int pos = 0) : d_(d), p_(pos) {}
    template <class T> T get() {
        need(sizeof(T)); T v; std::memcpy(&v, d_.constData() + p_, sizeof(T)); p_ += int(sizeof(T)); return v;
    }
    QByteArray bytes(int n) { need(n); QByteArray b = d_.mid(p_, n); p_ += n; return b; }
    const char *ptr() const { return d_.constData() + p_; }
    void skip(int n) { need(n); p_ += n; }
    void seek(int p) { if (p < 0 || p > d_.size()) throw FormatError("seek out of range"); p_ = p; }
    int pos() const { return p_; }
    int size() const { return int(d_.size()); }
    int left() const { return int(d_.size()) - p_; }
    std::string cstr() { std::string s; while (true) { char c = get<char>(); if (!c) break; s.push_back(c); } return s; }
    void need(qsizetype n) const { if (n < 0 || p_ + n > d_.size()) throw FormatError("unexpected end of data"); }
private:
    const QByteArray &d_;
    int p_;
};

class Writer {
public:
    template <class T> void put(T v) { buf.append(reinterpret_cast<const char *>(&v), sizeof(T)); }
    void bytes(const QByteArray &b) { buf.append(b); }
    void bytes(const char *p, int n) { buf.append(p, n); }
    template <class T> void patch(int at, T v) { std::memcpy(buf.data() + at, &v, sizeof(T)); }
    int pos() const { return int(buf.size()); }
    void pad(int align) { while (buf.size() % align) buf.append('\0'); }
    QByteArray buf;
};

} // namespace lol
