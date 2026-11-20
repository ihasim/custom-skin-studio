// Core checks against a real package:  skincore_tests <file.fantome> <game dir> <out dir>
// Exits non-zero on the first failure. Writes edited packages to <out dir> for external cross-checks (pyritofile).
#include "core/AudioDsp.h"
#include "core/Hashes.h"
#include "core/SkinProject.h"
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#include <cstdio>

using namespace lol;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { std::printf("FAIL %s:%d  %s  ", __FILE__, __LINE__, #cond); std::printf(__VA_ARGS__); std::printf("\n"); ++failures; } } while (0)
#define INFO(...) do { std::printf(__VA_ARGS__); std::printf("\n"); std::fflush(stdout); } while (0)

static double imgDiff(const QImage &a, const QImage &b) {
    if (a.size() != b.size()) return 1e9;
    QImage x = a.convertToFormat(QImage::Format_RGBA8888), y = b.convertToFormat(QImage::Format_RGBA8888);
    double s = 0; qint64 n = 0;
    for (int r = 0; r < x.height(); ++r) {
        const uchar *p = x.constScanLine(r), *q = y.constScanLine(r);
        for (int i = 0; i < x.width() * 4; ++i) { s += std::abs(int(p[i]) - int(q[i])); ++n; }
    }
    return s / double(n);
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc < 4) { std::printf("usage: skincore_tests <fantome> <gamedir> <outdir>\n"); return 2; }
    const QString fan = argv[1], game = argv[2], outDir = argv[3];
    QDir().mkpath(outDir);
    QElapsedTimer t; t.start();

    // ---- raw format round trips
    Fantome f = Fantome::load(fan);
    INFO("fantome: %d WAD, info=%s", int(f.wads.size()), qPrintable(f.info.value("Name").toString()));
    for (const auto &[n, bytes] : f.wads) {
        Wad w = Wad::fromBytes(bytes, n);
        Wad w2 = Wad::fromBytes(w.toBytes(), n);
        CHECK(w.entries.size() == w2.entries.size(), "%s", qPrintable(n));
        int bad = 0;
        for (const auto &[h, e] : w.entries) if (w.read(h) != w2.read(h)) ++bad;
        CHECK(bad == 0, "%s: %d entries differ after rewrite", qPrintable(n), bad);
        INFO("WAD %s: %d entries round-trip ok", qPrintable(n), int(w.entries.size()));
        for (const auto &[h, e] : w.entries) {
            QByteArray d = w.read(h);
            if (d.startsWith("PROP") || d.startsWith("PTCH")) {
                BinFile b = BinFile::fromBytes(d);
                CHECK(b.toBytes() == d, "BIN not byte-exact (%d entries)", int(b.entries.size()));
                INFO("BIN byte-exact: %d entries, %d links", int(b.entries.size()), int(b.links.size()));
            }
            if (d.startsWith("BKHD")) {
                WwiseBank bk = WwiseBank::fromBytes(d);
                CHECK(bk.toBytes() == d, "BNK not byte-exact");
                INFO("BNK byte-exact: %d objects", int(bk.objects.size()));
            }
            if (d.startsWith("r3d2")) { Wpk p = Wpk::fromBytes(d); CHECK(p.toBytes() == d, "WPK not byte-exact"); INFO("WPK byte-exact: %d wems", int(p.wems.size())); }
        }
    }

    // ---- texture codec
    {
        QImage g(64, 64, QImage::Format_RGBA8888);
        for (int y = 0; y < 64; ++y) for (int x = 0; x < 64; ++x) g.setPixelColor(x, y, QColor(x * 4, y * 4, 128, 255));
        Texture tx; tx.image = g; tx.format = Texture::DXT1; tx.mips = true;
        Texture back = Texture::fromBytes(tx.toTex());
        double d = imgDiff(g, back.image);
        CHECK(d < 4.0, "DXT1 gradient error %.2f", d);
        tx.format = Texture::DXT5; back = Texture::fromBytes(tx.toTex());
        CHECK(imgDiff(g, back.image) < 4.0, "DXT5");
        INFO("DXT1/DXT5 round trip ok (mean abs err %.2f)", d);
    }

    // ---- project load
    SkinProject p;
    p.load(fan, game);
    INFO("load %.1fs  champion=%s  game wad=%s", t.elapsed() / 1000.0, qPrintable(p.champion), qPrintable(p.gameWadPath));
    CHECK(!p.gameWadPath.isEmpty(), "game WAD not found");
    INFO("mesh: %d verts, %d tris, submeshes:", int(p.mesh.positions.size()), int(p.mesh.indices.size() / 3));
    for (const auto &s : p.mesh.submeshes) INFO("   %s  v=%u i=%u", qPrintable(s.name), s.vcount, s.icount);
    CHECK(p.hasSkeleton, "no skeleton");
    INFO("skeleton: %d joints, %d influences", int(p.skeleton.joints.size()), int(p.skeleton.influences.size()));
    for (const auto &part : p.parts) {
        INFO("part %s  tex=%s %dx%d fmt=%d mips=%d  regions:", qPrintable(part.submesh), qPrintable(part.texLabel),
             part.original.width(), part.original.height(), part.texFormat, part.texMips);
        for (const auto &r : part.regions.regions) INFO("     %-18s %5.1f%%  %s", qPrintable(r.name), r.share * 100, qPrintable(r.mean.name()));
    }
    CHECK(p.parts.size() >= 3, "expected body/sword/head parts, got %d", int(p.parts.size()));
    INFO("animations: %d", int(p.animations.size()));
    int animOk = 0;
    for (const auto &clip : p.animations) {
        const QString path = clip.clip;
        try {
            Animation a = p.loadAnimation(clip.file);
            int matched = 0;
            for (const auto &tr : a.tracks) if (p.skeleton.indexOfHash(tr.jointHash) >= 0) ++matched;
            if (a.duration > 0 && matched > 10) ++animOk;
            else INFO("   weak anim %s: dur=%.2f tracks=%d matched=%d", qPrintable(path), a.duration, int(a.tracks.size()), matched);
        } catch (const std::exception &e) { INFO("   anim fail %s: %s", qPrintable(path), e.what()); }
    }
    INFO("animations decoded: %d / %d   first: %s", animOk, int(p.animations.size()),
         "");
    for (const auto &c : p.animations) INFO("   %-22s %s", qPrintable(c.label), qPrintable(c.clip));
    CHECK(animOk >= int(p.animations.size()) * 9 / 10 && animOk > 10, "too many animation failures");

    INFO("vfx groups:");
    for (const auto &g : p.vfx.groups())
        INFO("   %s %-18s systems=%d textures=%d colour=%s  (%s)", qPrintable(g.key), qPrintable(g.label), int(g.entries.size()),
             int(g.textures.size()), qPrintable(g.current().name()), qPrintable(g.detail));
    CHECK(!p.vfx.groups().empty(), "no vfx");

    INFO("loadscreen: %dx%d %s", p.loadscreenSize.width(), p.loadscreenSize.height(), p.loadscreen.isNull() ? "(not found)" : "ok");
    CHECK(!p.loadscreen.isNull(), "loadscreen missing");
    p.loadscreen.save(outDir + "/loadscreen_original.png");

    INFO("voice: bank=%s  lines=%d  %s", qPrintable(p.voiceBankPath), int(p.voice.size()), qPrintable(p.voiceNote));
    for (const auto &v : p.voice)
        INFO("   [%s] %-28s %-44s sounds=%d  %s", qPrintable(v.group), qPrintable(v.label), qPrintable(v.event), int(v.sounds.size()),
             v.original.isEmpty() ? qPrintable(v.originalNote) : qPrintable(QString::number(v.original.seconds(), 'f', 2) + " s"));
    CHECK(p.voice.size() >= 10, "few voice lines");

    // ---- unchanged save keeps every file identical
    {
        const QString o = outDir + "/unchanged.fantome";
        p.save(o);
        Fantome a = Fantome::load(fan), b = Fantome::load(o);
        for (const auto &[n, bytes] : a.wads) {
            Wad wa = Wad::fromBytes(bytes, n), wb = Wad::fromBytes(b.wads.at(n), n);
            int bad = 0;
            for (const auto &[h, e] : wa.entries) if (!wb.contains(h) || wa.read(h) != wb.read(h)) ++bad;
            CHECK(bad == 0 && wa.entries.size() == wb.entries.size(), "%s: %d differ", qPrintable(n), bad);
        }
        INFO("unchanged save: all files identical");
    }

    // ---- edited save
    {
        // body: shift the largest region's hue
        p.parts[0].adjust[1].hue = 120; p.parts[0].adjust[1].sat = 1.5f;
        // Q effects red
        for (auto &[k, tg] : p.vfxTargets) if (k == "Q") { tg.enabled = true; tg.color = QColor(230, 40, 40); }
        // loading screen: a gradient
        QImage ls(p.loadscreenSize, QImage::Format_RGBA8888);
        for (int y = 0; y < ls.height(); ++y) for (int x = 0; x < ls.width(); ++x) ls.setPixelColor(x, y, QColor(x * 255 / ls.width(), 60, y * 255 / ls.height()));
        p.newLoadscreen = ls; p.loadscreenChanged = true;
        // voice: Q line -> 0.5 s 440 Hz tone, W line muted
        PcmClip tone; tone.rate = 44100;
        for (int i = 0; i < 22050; ++i) tone.samples.push_back(int16_t(8000 * std::sin(2 * M_PI * 440 * i / 44100.0)));
        QString qEvent, wEvent;
        for (auto &v : p.voice) {
            if (v.group == "Q" && qEvent.isEmpty()) { v.mode = VoiceLine::Replace; v.source = tone; qEvent = v.event; }
            else if (v.group == "W" && wEvent.isEmpty()) { v.mode = VoiceLine::Mute; wEvent = v.event; }
        }
        p.version = "1.2.3-studio";
        const QString o = outDir + "/edited.fantome";
        INFO("save edited: %s", qPrintable(p.save(o)));

        SkinProject q;
        q.load(o, game);
        CHECK(q.version == "1.2.3-studio", "version");
        double d = imgDiff(q.parts[0].original, p.parts[0].original);
        CHECK(d > 2.0, "body texture did not change (%.2f)", d);
        INFO("body texture change: mean abs diff %.2f", d);
        CHECK(!q.loadscreen.isNull() && imgDiff(q.loadscreen, ls) < 8.0, "loadscreen not replaced");
        INFO("loadscreen replaced: diff to target %.2f", imgDiff(q.loadscreen, ls));
        for (const auto &v : q.voice) {
            if (v.event == qEvent) {
                CHECK(std::abs(v.original.seconds() - 0.5) < 0.1, "Q voice length %.2f", v.original.seconds());
                INFO("Q voice now %.2f s @%d Hz", v.original.seconds(), v.original.rate);
            }
            if (v.event == wEvent) CHECK(v.original.seconds() < 0.2, "W not muted (%.2f s)", v.original.seconds());
        }
        // other lines unchanged
        int same = 0, diff = 0;
        for (size_t i = 0; i < q.voice.size(); ++i) {
            const auto &a = p.voice[i], &b = q.voice[i];
            if (a.event == qEvent || a.event == wEvent) continue;
            (a.original.samples == b.original.samples ? same : diff)++;
        }
        CHECK(diff == 0, "%d untouched voice lines changed", diff);
        INFO("untouched voice lines identical: %d", same);
        for (const auto &g : q.vfx.groups()) if (g.key == "Q") INFO("Q effect colour after save: %s", qPrintable(g.current().name()));
        q.parts[0].original.save(outDir + "/body_edited.png");
        q.loadscreen.save(outDir + "/loadscreen_edited.png");
    }

    INFO("total %.1fs, %d failure(s)", t.elapsed() / 1000.0, failures);
    return failures ? 1 : 0;
}
