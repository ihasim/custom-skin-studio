#include "app/ImageProvider.h"
#include "app/AppController.h"
#include <QMetaObject>
#include <QThread>

QImage ImageProvider::requestImage(const QString &id, QSize *size, const QSize &requested) {
    QImage img;
    AppController *c = AppController::instance();
    if (!c) return img;
    // the controller lives on the GUI thread; image providers may run elsewhere
    if (QThread::currentThread() == c->thread()) img = c->image(id);
    else QMetaObject::invokeMethod(c, [&] { img = c->image(id); }, Qt::BlockingQueuedConnection);
    if (size) *size = img.size();
    if (!img.isNull() && requested.isValid() && (requested.width() < img.width() || requested.height() < img.height()))
        img = img.scaled(requested, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return img;
}
