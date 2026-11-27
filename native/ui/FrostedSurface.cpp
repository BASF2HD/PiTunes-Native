#include "FrostedSurface.h"
#include "coverflow/CoverFlowWidget.h"
#include <QEvent>
#include <QPainter>
#include <QPainterPath>

namespace PiTunes {
namespace {
constexpr int tapRadius = 2;

QImage blurPass(const QImage &source, bool horizontal) {
    const QSize size = source.size();
    QImage result(size, QImage::Format_RGB32);
    if (result.isNull()) return source;

    constexpr int weights[] = {1, 4, 6, 4, 1};
    for (int y = 0; y < size.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(result.scanLine(y));
        for (int x = 0; x < size.width(); ++x) {
            int red = 0, green = 0, blue = 0;
            for (int tap = -tapRadius; tap <= tapRadius; ++tap) {
                const auto pixel = source.pixel(qBound(0, x + (horizontal ? tap : 0), size.width() - 1),
                                                qBound(0, y + (horizontal ? 0 : tap), size.height() - 1));
                const int weight = weights[tap + tapRadius];
                red += qRed(pixel) * weight;
                green += qGreen(pixel) * weight;
                blue += qBlue(pixel) * weight;
            }
            line[x] = qRgb(red / 16, green / 16, blue / 16);
        }
    }
    return result;
}

QImage sampleArtwork(CoverFlowWidget *stage, const QWidget *surface) {
    if (surface->size().isEmpty()) return {};
    const int step = qMax(5, (qMax(surface->width(), surface->height()) + 239) / 240);
    const QSize sampleSize((surface->width() + step - 1) / step, (surface->height() + step - 1) / step);
    const QPoint origin = surface->mapToGlobal(QPoint()) - QPoint(tapRadius * step, tapRadius * step);
    QImage image(sampleSize + QSize(2 * tapRadius, 2 * tapRadius), QImage::Format_RGB32);
    if (image.isNull()) return {};
    image.fill(Qt::black);
    {
        QPainter painter(&image);
        painter.scale(1. / step, 1. / step);
        painter.translate(stage->mapToGlobal(QPoint()) - origin);
        stage->paintArtworkBackdrop(painter);
    }
    return blurPass(blurPass(image, true), false).copy(QRect(QPoint(tapRadius, tapRadius), sampleSize));
}

void paintGlass(QWidget *widget, const QImage &backdrop, const QColor &tint, int edgeAlpha, int radius) {
    QPainter painter(widget);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF bounds(widget->rect());
    QPainterPath clip;
    clip.addRoundedRect(bounds, radius, radius);
    painter.setClipPath(clip);
    if (backdrop.isNull()) painter.fillRect(bounds, Qt::black);
    else painter.drawImage(bounds, backdrop);
    painter.fillRect(bounds, tint);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QColor(255, 255, 255, edgeAlpha));
    painter.drawRoundedRect(bounds.adjusted(.5, .5, -.5, -.5), radius, radius);
}

bool backdropGeometryChanged(const QEvent *event) {
    return event->type() == QEvent::Show || event->type() == QEvent::Move || event->type() == QEvent::Resize;
}
}

FrostedPanel::FrostedPanel(CoverFlowWidget *stage, const QColor &tint, int edgeAlpha, int radius)
    : QFrame(stage), m_stage(stage), m_tint(tint), m_edgeAlpha(edgeAlpha), m_radius(radius) {
    m_refresh.setSingleShot(true);
    connect(&m_refresh, &QTimer::timeout, this, [this] {
        if (!isVisible() || !m_stage) return;
        m_backdrop = sampleArtwork(m_stage, this);
        update();
    });
    connect(stage, &CoverFlowWidget::artworkBackdropChanged, this, [this] {
        if (isVisible() && !m_refresh.isActive()) m_refresh.start();
    });
}

bool FrostedPanel::event(QEvent *event) {
    if (backdropGeometryChanged(event) && (isVisible() || event->type() == QEvent::Show) &&
        !m_refresh.isActive()) m_refresh.start();
    return QFrame::event(event);
}

void FrostedPanel::paintEvent(QPaintEvent *) {
    paintGlass(this, m_backdrop, m_tint, m_edgeAlpha, m_radius);
}

FrostedMenu::FrostedMenu(CoverFlowWidget *stage, QWidget *parent) : QMenu(parent), m_stage(stage) {
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet(
        "QMenu { background: transparent; color: #f0f2fa; border: 1px solid transparent; padding: 4px; }"
        "QMenu::item { padding: 6px 20px 6px 25px; border-radius: 4px; }"
        "QMenu::item:selected { background: rgba(206,219,248,46); }"
        "QMenu::separator { height: 1px; margin: 4px 8px; background: rgba(240,245,255,30); }");
    m_refresh.setSingleShot(true);
    connect(&m_refresh, &QTimer::timeout, this, [this] {
        if (!isVisible() || !m_stage) return;
        m_backdrop = sampleArtwork(m_stage, this);
        update();
    });
    connect(stage, &CoverFlowWidget::artworkBackdropChanged, this, [this] {
        if (isVisible() && !m_refresh.isActive()) m_refresh.start();
    });
}

bool FrostedMenu::event(QEvent *event) {
    if (backdropGeometryChanged(event) && (isVisible() || event->type() == QEvent::Show) &&
        !m_refresh.isActive()) m_refresh.start();
    if (event->type() == QEvent::Hide) m_backdrop = QImage();
    return QMenu::event(event);
}

void FrostedMenu::paintEvent(QPaintEvent *event) {
    paintGlass(this, m_backdrop, QColor(12, 15, 24, 168), 35, 6);
    QMenu::paintEvent(event);
}
} // namespace PiTunes
