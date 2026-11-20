#include "app/WaveformItem.h"
#include <QPainter>

WaveformItem::WaveformItem(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    connect(this, &WaveformItem::changed, this, [this] { update(); });
}

void WaveformItem::paint(QPainter *p) {
    const int n = int(peaks_.size());
    if (n == 0) return;
    const double w = width(), h = height(), mid = h / 2;
    const double bw = w / n;
    p->setPen(Qt::NoPen);
    for (int i = 0; i < n; ++i) {
        const double v = std::max(0.02, std::min(1.0, peaks_[i].toDouble()));
        const double bh = v * (h - 2);
        p->setBrush((i + 0.5) / n <= progress_ ? played_ : color_);
        p->drawRoundedRect(QRectF(i * bw + bw * 0.15, mid - bh / 2, std::max(1.0, bw * 0.7), bh), 1, 1);
    }
}
