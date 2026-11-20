#include "app/AppController.h"
#include "app/ImageProvider.h"
#include <QCommandLineParser>
#include <cstdio>
#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QMouseEvent>
#include <QWheelEvent>
#include <memory>
#include <functional>
#include <QtQuick3D/qquick3d.h>

// Test / screenshot mode:
//   --open <f.fantome> [--game <dir>] [--mode colors|effects|voice|loadscreen|package] [--anim <clip>:<sec>]
//   [--select <part>:<region>] [--size 1440x900] --screenshot <out.png>
// Real input events for tests: "wheel:x,y,dy; drag:x1,y1,x2,y2[,right]; click:x,y; dbl:x,y; wait:ms; shot:file.png; call:name"
static void runEvents(QQuickWindow *win, QStringList steps, std::function<void()> done) {
    if (steps.isEmpty()) { done(); return; }
    const QString step = steps.takeFirst().trimmed();
    const QString kind = step.section(':', 0, 0);
    const QStringList a = step.section(':', 1).split(',');
    auto num = [&](int i) { return i < a.size() ? a[i].toDouble() : 0.0; };
    int delay = 120;
    auto send = [win](QEvent *e) { QCoreApplication::sendEvent(win, e); };
    if (kind == "wheel") {
        QPointF p(num(0), num(1));
        QWheelEvent ev(p, win->mapToGlobal(p), QPoint(), QPoint(0, int(num(2))), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        send(&ev);
    } else if (kind == "click" || kind == "dbl") {
        QPointF p(num(0), num(1));
        for (int k = 0; k < (kind == "dbl" ? 2 : 1); ++k) {
            QMouseEvent pr(QEvent::MouseButtonPress, p, win->mapToGlobal(p), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier); send(&pr);
            QMouseEvent re(QEvent::MouseButtonRelease, p, win->mapToGlobal(p), Qt::LeftButton, Qt::NoButton, Qt::NoModifier); send(&re);
        }
    } else if (kind == "drag") {
        const Qt::MouseButton b = a.size() > 4 && a[4] == "right" ? Qt::RightButton : Qt::LeftButton;
        QPointF p0(num(0), num(1)), p1(num(2), num(3));
        QMouseEvent pr(QEvent::MouseButtonPress, p0, win->mapToGlobal(p0), b, b, Qt::NoModifier); send(&pr);
        for (int i = 1; i <= 20; ++i) {
            QPointF p = p0 + (p1 - p0) * (i / 20.0);
            QMouseEvent mv(QEvent::MouseMove, p, win->mapToGlobal(p), Qt::NoButton, b, Qt::NoModifier); send(&mv);
        }
        QMouseEvent re(QEvent::MouseButtonRelease, p1, win->mapToGlobal(p1), b, Qt::NoButton, Qt::NoModifier); send(&re);
    } else if (kind == "wait") {
        delay = int(num(0));
    } else if (kind == "shot") {
        win->grabWindow().save(step.section(':', 1));
        std::printf("shot %s\n", qPrintable(step.section(':', 1)));
    } else if (kind == "call") {
        QMetaObject::invokeMethod(win, qPrintable(step.section(':', 1)));
    }
    QTimer::singleShot(delay, win, [win, steps, done] { runEvents(win, steps, done); });
}

int main(int argc, char *argv[]) {
    QSurfaceFormat::setDefaultFormat(QQuick3D::idealSurfaceFormat(4));
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName("CustomSkinStudio");
    QGuiApplication::setApplicationName("CustomSkinStudio");
    QGuiApplication::setApplicationDisplayName("Custom Skin Studio");
    QQuickStyle::setStyle("Basic");
    QGuiApplication::setWindowIcon(QIcon(":/qt/qml/SkinStudio/platform/icon/icon.png"));

    QCommandLineParser cli;
    cli.addHelpOption();
    QCommandLineOption openOpt("open", "fantome", "file"), gameOpt("game", "game dir", "dir"), modeOpt("mode", "panel", "name"),
        animOpt("anim", "clip:sec", "a"), selOpt("select", "part:region", "s"), shotOpt("screenshot", "png", "file"),
        sizeOpt("size", "WxH", "size"), voiceOpt("expand-voice", "voice line index", "i"), cropOpt("crop", "image to crop", "file"),
        voiceFileOpt("voice-file", "line:path", "v"), recolorOpt("recolor", "part:region:#hex", "r"), fxOpt("effects", "#hex", "c"), saveOpt("save-to", "out.fantome", "f"), eventsOpt("events", "input script", "e");
    cli.addOptions({openOpt, gameOpt, modeOpt, animOpt, selOpt, shotOpt, sizeOpt, voiceOpt, cropOpt, voiceFileOpt, recolorOpt, fxOpt, saveOpt, eventsOpt});
    cli.addPositionalArgument("file", ".fantome to open");
    cli.process(app);

    AppController controller;
    if (cli.isSet(gameOpt)) controller.setGameDir(QUrl::fromLocalFile(QDir(cli.value(gameOpt)).absolutePath()));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("AppController", &controller);
    engine.addImageProvider("studio", new ImageProvider);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("SkinStudio", "Main");
    if (engine.rootObjects().isEmpty()) return 1;
    auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().first());

    if (cli.isSet(sizeOpt)) {
        const QStringList wh = cli.value(sizeOpt).split('x');
        if (wh.size() == 2) win->resize(wh[0].toInt(), wh[1].toInt());
    }
    QString open = cli.value(openOpt);
    if (open.isEmpty() && !cli.positionalArguments().isEmpty()) open = cli.positionalArguments().first();
    if (!open.isEmpty()) controller.openFantome(QUrl::fromLocalFile(QDir(open).absolutePath()));

    const QString shot = cli.value(shotOpt);
    if (!shot.isEmpty()) {
        auto take = [win, shot, &controller] {
            std::printf("state screen=%s error=%s status=%s\n", qPrintable(controller.screen()), qPrintable(controller.error()), qPrintable(controller.status()));
            QTimer::singleShot(1200, win, [win, shot] {
                QImage img = win->grabWindow();
                img.save(shot);
                std::printf("screenshot %s %dx%d\n", qPrintable(shot), img.width(), img.height());
                QCoreApplication::quit();
            });
        };
        auto apply = [&, win, take] {
            if (cli.isSet(modeOpt)) win->setProperty("mode", cli.value(modeOpt));
            if (cli.isSet(animOpt)) {
                const QStringList a = cli.value(animOpt).split(':');
                controller.selectAnimationByClip(a[0], a.size() > 1 ? a[1].toDouble() : 0.5);
            }
            if (cli.isSet(selOpt)) {
                const QStringList s = cli.value(selOpt).split(':');
                controller.setSelectedPart(s[0].toInt());
                if (s.size() > 1) controller.setSelectedRegion(s[1].toInt());
            }
            if (cli.isSet(voiceFileOpt)) {
                const QString v = cli.value(voiceFileOpt);
                controller.voiceFromFile(v.section(':', 0, 0).toInt(), QUrl::fromLocalFile(QDir(v.section(':', 1)).absolutePath()));
            }
            if (cli.isSet(recolorOpt)) {
                const QStringList r = cli.value(recolorOpt).split(':');
                controller.setRegionColor(r[0].toInt(), r[1].toInt(), QColor(r[2]));
            }
            if (cli.isSet(fxOpt)) controller.setAllEffects(QColor(cli.value(fxOpt)));
            if (cli.isSet(saveOpt)) controller.save(QUrl::fromLocalFile(QDir(cli.value(saveOpt)).absolutePath()));
            if (cli.isSet(voiceOpt)) win->setProperty("expandVoice", cli.value(voiceOpt).toInt());
            if (cli.isSet(cropOpt)) QMetaObject::invokeMethod(win, "openCrop", Q_ARG(QVariant, QUrl::fromLocalFile(QDir(cli.value(cropOpt)).absolutePath())));
            if (cli.isSet(eventsOpt)) {
                const QStringList steps = cli.value(eventsOpt).split(';', Qt::SkipEmptyParts);
                QTimer::singleShot(1500, win, [win, steps, take] { runEvents(win, steps, take); });
            } else take();
        };
        if (open.isEmpty()) take();
        else {
            QObject::connect(&controller, &AppController::loaded, win, apply, Qt::SingleShotConnection);
            QObject::connect(&controller, &AppController::errorChanged, win, [&controller, take] { if (!controller.error().isEmpty()) take(); });
        }
        QTimer::singleShot(120000, &app, [] { std::printf("screenshot timeout\n"); QCoreApplication::exit(3); });
    }
    return app.exec();
}
