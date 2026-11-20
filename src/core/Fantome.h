#pragma once
// .fantome = zip with META/info.json, META/image.png and WAD/*.wad.client (ROSE / LTK mod format).
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <map>

namespace lol {

struct Fantome {
    QJsonObject info;                          // Name, Author, Version, Description
    QByteArray image;                          // META/image.png (may be empty)
    std::map<QString, QByteArray> wads;        // "Garen.wad.client" -> WAD bytes
    std::map<QString, QByteArray> otherFiles;  // anything else in the zip, kept as-is

    static Fantome load(const QString &path);
    void save(const QString &path) const;
};

} // namespace lol
