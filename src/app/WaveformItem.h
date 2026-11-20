#pragma once
#include <QColor>
#include <QQuickPaintedItem>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// Mirrored peak bars; `progress` (0..1) tints the played part.
class WaveformItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList peaks READ peaks WRITE setPeaks NOTIFY changed)
    Q_PROPERTY(QColor color MEMBER color_ NOTIFY changed)
    Q_PROPERTY(QColor playedColor MEMBER played_ NOTIFY changed)
    Q_PROPERTY(double progress MEMBER progress_ NOTIFY changed)
public:
    explicit WaveformItem(QQuickItem *parent = nullptr);
    QVariantList peaks() const { return peaks_; }
    void setPeaks(const QVariantList &p) { peaks_ = p; emit changed(); }
    void paint(QPainter *p) override;
signals:
    void changed();
private:
    QVariantList peaks_;
    QColor color_{120, 130, 150}, played_{60, 210, 220};
    double progress_ = 0;
};
