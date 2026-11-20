#pragma once
#include <QQuickImageProvider>

// image://studio/<id>/<revision>  ->  AppController::image(id)
class ImageProvider : public QQuickImageProvider {
public:
    ImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requested) override;
};
