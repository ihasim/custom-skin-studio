#pragma once
// One opened .fantome: what the editor shows and changes, and how it is written back as a new .fantome.
// Champion files the package does not carry (skeleton, animations, original textures) come from the game's
// <Champion>.wad.client, found in a "game folder" (the League of Legends install folder).
#include "core/Anm.h"
#include "core/AudioDsp.h"
#include "core/Bin.h"
#include "core/Fantome.h"
#include "core/Mesh.h"
#include "core/Recolor.h"
#include "core/Tex.h"
#include "core/Vfx.h"
#include "core/Wad.h"
#include "core/Wwise.h"
#include <QImage>
#include <QString>
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace lol {

struct ModelPart {
    QString submesh;             // submesh name in the SKN
    uint64_t texHash = 0;        // texture used by this submesh
    QString texLabel;            // file name for display
    uint8_t texFormat = Texture::DXT1;
    bool texMips = false;
    QImage original;             // RGBA8888
    RegionMap regions;
    std::vector<RegionAdjust> adjust;
    bool edited() const { for (auto &a : adjust) if (!a.isIdentity()) return true; return false; }
};

struct VoiceLine {
    QString event;               // Wwise event name, e.g. Play_vo_Garen_GarenQ_cast3D
    QString group;               // "Q", "W", "E", "R", "P", "A" (attack), "M" (move), "D" (death), "S" (social), "O"
    QString label;               // Turkish, shown in the UI
    uint32_t eventId = 0;
    std::vector<uint32_t> sounds; // Sound objects the event can play (random containers -> several)
    PcmClip original;            // first sound, decoded (empty if not PCM)
    QString originalNote;        // why `original` is empty
    // edit
    enum Mode { Keep, Replace, Mute } mode = Keep;
    PcmClip source;              // user's recording / file (mono)
    VoiceEdit edit;
    PcmClip processed() const;   // what will be written (Replace)
};

class SkinProject {
public:
    // Throws FormatError with a Turkish message.
    void load(const QString &fantomePath, const QString &gameDir);
    QString save(const QString &outPath) const;   // returns a short report; throws on failure

    static QString findGameWad(const QString &gameDir, const QString &champion);   // "" if not found

    // ---- what is open
    QString fantomePath, champion, gameWadPath;
    Fantome fantome;
    QString mainWadName;                         // "Garen.wad.client" inside the fantome
    std::map<QString, Wad> wads;                 // parsed fantome WADs
    QString skinBinPath;                         // data/characters/<c>/skins/skinN.bin
    BinFile skinBin;

    // ---- model
    SkinnedMesh mesh;
    Skeleton skeleton;
    bool hasSkeleton = false;
    std::vector<ModelPart> parts;
    struct AnimClip { QString label; QString clip; uint64_t file = 0; int order = 100; };
    std::vector<AnimClip> animations;
    Animation loadAnimation(uint64_t file) const;

    // ---- effects
    VfxEditor vfx;
    std::map<QString, VfxTarget> vfxTargets;
    QImage previewVfxTexture(const QString &groupKey) const;   // one representative texture, recoloured

    // ---- loading screen
    QImage loadscreen;                 // current (original or replaced), RGBA
    QSize loadscreenSize{308, 560};
    bool loadscreenChanged = false;
    QImage newLoadscreen;              // exactly loadscreenSize

    // ---- voice
    std::vector<VoiceLine> voice;
    QString voiceBankPath;             // ..._vo_events.bnk (lower-case)
    QString voiceNote;                 // shown when no voice bank is available

    // ---- package info
    QString name, author, version, description;
    QImage cover;
    bool coverChanged = false;

    // reads a file from the fantome WADs first, then the game WAD
    QByteArray readFile(uint64_t hash) const;
    QByteArray readFile(const QString &path) const;
    QImage readTexture(uint64_t hash, uint8_t *fmt = nullptr, bool *mips = nullptr) const;

    bool hasChanges() const;
    QImage partImage(int i, int highlight = -1) const;

private:
    std::unique_ptr<WadFile> game_;
    WwiseBank bank_;
    Wpk wpk_;
    std::vector<QString> voiceWads_;   // fantome WADs that carry the voice bank
    void loadModel();
    void loadVoice();
    void loadLoadscreen();
};

QString voiceGroup(const QString &event, QString *label);

} // namespace lol
