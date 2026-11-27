#include "CoverFlowWidget.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QTouchEvent>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace PiTunes {
namespace {
constexpr double sideHalfWidth = .20;
constexpr double centreOverlap = .02;
constexpr double firstSideOffset = .5 + sideHalfWidth - centreOverlap;
constexpr double sideSpacing = .27;
}

CoverFlowWidget::CoverFlowWidget(LibraryModel *model, ArtworkCache *artwork, QWidget *parent)
    : QWidget(parent), m_model(model), m_artwork(artwork) {
    setFocusPolicy(Qt::StrongFocus); setAttribute(Qt::WA_AcceptTouchEvents);
    setMinimumSize(240, 150); setAccessibleName("Album CoverFlow");
    m_timer.setInterval(33);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        const double t = qMin(1., m_clock.elapsed() / 420.);
        const double ease = 1. - std::pow(1. - t, 3.);
        m_position = m_start + (m_target - m_start) * ease;
        if (t >= 1.) { m_position = m_target; m_timer.stop(); prefetch(); }
        update();
    });
    connect(model, &QAbstractItemModel::modelReset, this, [this] {
        m_timer.stop(); m_target = 0; m_position = 0; prefetch(); update();
    });
    connect(model, &QAbstractItemModel::rowsInserted, this, [this] { prefetch(); update(); });
    connect(model, &LibraryModel::pageLoaded, this, QOverload<>::of(&CoverFlowWidget::update));
    connect(model, &LibraryModel::error, this, [this](const QString &) { update(); });
    connect(artwork, &ArtworkCache::ready, this, [this](const QString &) { update(); });
}
int CoverFlowWidget::visibleCoverCount() const {
    const auto [first, last] = visibleRows(m_position);
    return qMax(0, last - first + 1);
}
double CoverFlowWidget::coverSize() const {
    return qMin(width() * .33, height() * .65);
}
QPair<int, int> CoverFlowWidget::visibleRows(double position) const {
    // Cull once a cover's inner edge leaves the viewport. Fractional positions
    // let edge covers slide out naturally during navigation.
    const double halfViewport = width() / (2. * coverSize());
    const double screenReach = 1. + (halfViewport - (firstSideOffset - sideHalfWidth)) / sideSpacing;
    const int cacheReach = (m_artwork->prefetchCapacity() - 1) / 2;
    const double reach = qMin(screenReach, double(cacheReach));
    return {qMax(0, int(std::ceil(position - reach))),
            qMin(m_model->rowCount() - 1, int(std::floor(position + reach)))};
}
void CoverFlowWidget::select(int index, bool animate) {
    index = qBound(0, index, qMax(0, m_model->rowCount() - 1));
    if (m_target == index && (m_timer.isActive() || qFuzzyCompare(m_position + 1, index + 1.))) return;
    m_target = index; m_start = m_position;
    if (animate && !m_reduced && isVisible()) { m_clock.restart(); m_timer.start(); }
    else { m_timer.stop(); m_position = index; }
    prefetch(); update(); emit selectionChanged(index);
}
void CoverFlowWidget::setPlayingAlbum(QString id) {
    if (id == m_playingId) return;
    m_playingId = std::move(id); update();
}
void CoverFlowWidget::prefetch() {
    QStringList keys;
    const auto [first, last] = visibleRows(m_target);
    const int reach = qMax(m_target - first, last - m_target);
    for (int step = 0; step <= reach; ++step) {
        for (int sign : {1, -1}) {
            if (step == 0 && sign == -1) continue;
            const auto *album = m_model->album(m_target + step * sign);
            if (album && !album->artUrl.isEmpty()) keys.append(album->artUrl);
        }
    }
    m_artwork->prefetch(keys);
}
void CoverFlowWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    prefetch();
}
QPolygonF CoverFlowWidget::polygon(double distance, double size, double top) const {
    const double sign = distance < 0 ? -1 : 1;
    const double t = qMin(1., std::abs(distance));
    const double reduced = size * (1. - .16 * t);
    const double halfWidth = size * (.5 + (sideHalfWidth - .5) * t);
    // The centre is painted last at rest, hiding the inner edge of each neighbour.
    const double x = width() / 2. + sign * size * (t * firstSideOffset + qMax(0., std::abs(distance) - 1.) * sideSpacing);
    const double baseline = top + size;
    const double y = baseline - reduced;
    const double skew = -size * .065 * t * sign;
    return QPolygonF({QPointF(x - halfWidth, y - skew), QPointF(x + halfWidth, y + skew),
                      QPointF(x + halfWidth, baseline), QPointF(x - halfWidth, baseline)});
}
void CoverFlowWidget::paintEvent(QPaintEvent *) {
    ++m_paints;
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    m_hits.clear();
    if (m_model->rowCount() == 0) {
        painter.setPen(QColor("#a8a8b4"));
        painter.drawText(rect(), Qt::AlignCenter, m_model->loading() ? "Loading library…" : "No albums · choose a source in Settings");
        return;
    }
    const auto [first, last] = visibleRows(m_position);
    QVector<int> rows;
    for (int i = first; i <= last; ++i) rows.append(i);
    std::sort(rows.begin(), rows.end(), [this](int a, int b) { return std::abs(a - m_position) > std::abs(b - m_position); });
    const double size = coverSize();
    const double top = height() * .035;
    for (int row : rows) {
        const auto &album = *m_model->album(row);
        const auto poly = polygon(row - m_position, size, top);
        const auto *art = m_artwork->find(album.artUrl);
        const QRectF reflection(poly[3] + QPointF(0, 3), poly[2] + QPointF(0, size * .44));
        // Draw back to front. The nearer footprint replaces any farther
        // reflection so its vertical fade blends only with the black floor.
        painter.fillRect(reflection, Qt::black);
        if (art && !art->cover.isNull()) {
            painter.drawImage(reflection, art->reflection);
            const QPolygonF source({QPointF(0, 0), QPointF(art->cover.width(), 0),
                QPointF(art->cover.width(), art->cover.height()), QPointF(0, art->cover.height())});
            QTransform projection;
            if (QTransform::quadToQuad(source, poly, projection)) {
                painter.save();
                painter.setTransform(projection, true);
                painter.drawImage(QPointF(0, 0), art->cover);
                painter.restore();
            }
        } else {
            painter.setBrush(QColor::fromHsv((row * 37 + 210) % 360, 65, 42));
            painter.setPen(QColor("#55555d")); painter.drawPolygon(poly);
            painter.setPen(QColor("#d8d8df"));
            painter.drawText(poly.boundingRect().adjusted(8, 8, -8, -8), Qt::AlignCenter | Qt::TextWordWrap, album.title);
        }
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(album.id == m_playingId ? Qt::white : QColor("#62626b"), row == m_target ? 1.5 : .6));
        painter.drawPolygon(poly);
        m_hits.append({row, poly});
    }
    const auto *selected = m_model->album(m_target);
    if (selected) {
        const int y = int(top + size + 12);
        QFont font = painter.font(); font.setPixelSize(qBound(14, width() / 48, 24)); painter.setFont(font);
        painter.setPen(Qt::white);
        painter.drawText(QRect(10, y, width() - 20, 32), Qt::AlignCenter,
            painter.fontMetrics().elidedText(selected->title, Qt::ElideRight, width() - 24));
        font.setPixelSize(qBound(12, width() / 65, 18)); painter.setFont(font); painter.setPen(QColor("#a8a8b4"));
        painter.drawText(QRect(10, y + 32, width() - 20, 26), Qt::AlignCenter,
            painter.fontMetrics().elidedText(selected->artist + (selected->year.isEmpty() ? "" : " · " + selected->year), Qt::ElideRight, width() - 24));
    }
}
void CoverFlowWidget::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Left) select(m_target - 1);
    else if (e->key() == Qt::Key_Right) select(m_target + 1);
    else if (e->key() == Qt::Key_Home) select(0);
    else if (e->key() == Qt::Key_End) select(m_model->rowCount() - 1);
    else if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) emit activated(m_target);
    else QWidget::keyPressEvent(e);
}
void CoverFlowWidget::wheelEvent(QWheelEvent *e) {
    const auto delta = e->angleDelta().y() != 0 ? e->angleDelta().y() : e->angleDelta().x();
    if (delta != 0) select(m_target + (delta < 0 ? 1 : -1));
    e->accept();
}
void CoverFlowWidget::mousePressEvent(QMouseEvent *e) { m_press = e->localPos(); m_dragging = false; setFocus(); }
void CoverFlowWidget::mouseMoveEvent(QMouseEvent *e) {
    if ((e->buttons() & Qt::LeftButton) && (e->localPos() - m_press).manhattanLength() > 18) m_dragging = true;
}
void CoverFlowWidget::mouseReleaseEvent(QMouseEvent *e) {
    if (m_dragging) select(m_target + (e->localPos().x() < m_press.x() ? 1 : -1));
    else for (auto it = m_hits.crbegin(); it != m_hits.crend(); ++it) {
        if (!it->second.containsPoint(e->localPos(), Qt::OddEvenFill)) continue;
        if (it->first == m_target) emit activated(m_target); else select(it->first);
        break;
    }
}
bool CoverFlowWidget::event(QEvent *e) {
    if (e->type() == QEvent::TouchBegin || e->type() == QEvent::TouchEnd || e->type() == QEvent::TouchUpdate) {
        const auto *touch = static_cast<QTouchEvent *>(e);
        if (!touch->touchPoints().isEmpty()) {
            const auto point = touch->touchPoints().first().pos();
            if (e->type() == QEvent::TouchBegin) m_press = point;
            else if (e->type() == QEvent::TouchEnd) {
                const double dx = point.x() - m_press.x();
                if (std::abs(dx) > 24) select(m_target + (dx < 0 ? 1 : -1));
                else { // Use the same hit-test as a mouse click, without synthesized duplicate playback.
                    for (auto it = m_hits.crbegin(); it != m_hits.crend(); ++it) {
                        if (!it->second.containsPoint(point, Qt::OddEvenFill)) continue;
                        if (it->first == m_target) emit activated(m_target); else select(it->first);
                        break;
                    }
                }
            }
        }
        e->accept(); return true;
    }
    return QWidget::event(e);
}
} // namespace PiTunes
