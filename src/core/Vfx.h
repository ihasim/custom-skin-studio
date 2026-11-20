#pragma once
// Ability effect colours: VfxSystemDefinitionData entries of the skin BIN grouped by ability (P/Q/W/E/R/attack/idle),
// their ValueColor values and particle textures recoloured by rotating the group's main hue to a target colour.
#include "core/Bin.h"
#include <QColor>
#include <QImage>
#include <QString>
#include <functional>
#include <map>
#include <set>
#include <vector>

namespace lol {

struct VfxGroup {
    QString key;                      // "P", "Q", "W", "E", "R", "A", "I", "O"
    QString label;                    // shown in the UI
    QString detail;                   // short description (effect names)
    std::vector<uint32_t> entries;    // VfxSystemDefinitionData entry names
    std::set<QString> textures;       // lower-case texture paths referenced by these systems
    float themeHue = 0, themeSat = 0; // main colour now
    bool hasColor = false;
    QColor current() const;
};

struct VfxTarget {
    bool enabled = false;
    QColor color;
};

class VfxEditor {
public:
    using TextureLoader = std::function<QImage(const QString &lowerPath)>;

    void analyze(BinFile &bin, const TextureLoader &load);
    const std::vector<VfxGroup> &groups() const { return groups_; }

    // Returns a recoloured copy of `bin`; recoloured textures (path -> image) go to `textures`, keeping each
    // texture's original format (the caller encodes). Shared textures are duplicated per group when needed.
    BinFile apply(const BinFile &bin, const std::map<QString, VfxTarget> &targets, const TextureLoader &load,
                  std::map<QString, QImage> &textures) const;

    static QColor recolor(const QColor &c, float themeHue, float themeSat, const QColor &target);
    static QImage recolorImage(const QImage &img, float themeHue, float themeSat, const QColor &target);

private:
    std::vector<VfxGroup> groups_;
};

} // namespace lol
