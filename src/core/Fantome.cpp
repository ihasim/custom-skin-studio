#include "core/Fantome.h"
#include "core/Hashes.h"
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <miniz.h>

namespace lol {

Fantome Fantome::load(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) throw FormatError(("dosya açılamadı: " + path).toStdString());
    const QByteArray zip = f.readAll();
    mz_zip_archive za{};
    if (!mz_zip_reader_init_mem(&za, zip.constData(), size_t(zip.size()), 0))
        throw FormatError("bu dosya bir .fantome (zip) değil");
    Fantome fm;
    const mz_uint n = mz_zip_reader_get_num_files(&za);
    for (mz_uint i = 0; i < n; ++i) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&za, i, &st) || st.m_is_directory) continue;
        size_t sz = 0;
        void *p = mz_zip_reader_extract_to_heap(&za, i, &sz, 0);
        if (!p) { mz_zip_reader_end(&za); throw FormatError("zip içinden okunamadı"); }
        QByteArray data(static_cast<const char *>(p), qsizetype(sz));
        mz_free(p);
        QString name = QString::fromUtf8(st.m_filename).replace('\\', '/');
        const QString lower = name.toLower();
        if (lower == "meta/info.json") fm.info = QJsonDocument::fromJson(data).object();
        else if (lower == "meta/image.png") fm.image = data;
        else if (lower.startsWith("wad/") && lower.endsWith(".wad.client")) fm.wads[name.mid(4)] = data;
        else fm.otherFiles[name] = data;
    }
    mz_zip_reader_end(&za);
    if (fm.wads.empty()) throw FormatError("paketin içinde WAD dosyası yok");
    return fm;
}

void Fantome::save(const QString &path) const {
    mz_zip_archive za{};
    if (!mz_zip_writer_init_heap(&za, 0, 0)) throw FormatError("zip oluşturulamadı");
    auto add = [&](const QString &name, const QByteArray &data, mz_uint level) {
        const QByteArray n = name.toUtf8();
        if (!mz_zip_writer_add_mem(&za, n.constData(), data.constData(), size_t(data.size()), level))
            throw FormatError("zip'e eklenemedi");
    };
    add("META/info.json", QJsonDocument(info).toJson(QJsonDocument::Indented), MZ_DEFAULT_LEVEL);
    if (!image.isEmpty()) add("META/image.png", image, MZ_DEFAULT_LEVEL);
    for (const auto &[name, data] : wads) add("WAD/" + name, data, MZ_DEFAULT_LEVEL);
    for (const auto &[name, data] : otherFiles) add(name, data, MZ_DEFAULT_LEVEL);
    void *buf = nullptr; size_t sz = 0;
    if (!mz_zip_writer_finalize_heap_archive(&za, &buf, &sz)) { mz_zip_writer_end(&za); throw FormatError("zip bitirilemedi"); }
    QByteArray out(static_cast<const char *>(buf), qsizetype(sz));
    mz_free(buf);
    mz_zip_writer_end(&za);
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(out) != out.size() || !f.commit())
        throw FormatError(("dosya yazılamadı: " + path).toStdString());
}

} // namespace lol
