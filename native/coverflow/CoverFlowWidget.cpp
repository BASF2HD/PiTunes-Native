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
void drawProjected(QPainter &painter, const QImage &image, const QPolygonF &quad) {
    if (image.isNull()) return;
    const QPolygonF source({QPointF(0, 0), QPointF(image.width(), 0),
        QPointF(image.width(), image.height()), QPointF(0, image.height())});
    QTransform projection;
    if (!QTransform::quadToQuad(source, quad, projection)) return;
    painter.save();
    painter.setTransform(projection, true);
    painter.drawImage(QPointF(0, 0), image);
    painter.restore();
}
}

CoverFlowWidget::CoverFlowWidget(LibraryModel *model, ArtworkCache *artwork, QWidget *parent)
    : QWidget(parent), m_model(model), m_artwork(artwork) {
    setFocusPolicy(Qt::StrongFocus); setAttribute(Qt::WA_AcceptTouchEvents);
    setMinimumSize(240, 150); setAccessibleName("Album CoverFlow");
    m_timer.setTimerType(Qt::PreciseTimer);
#ifdef Q_OS_MACOS
    m_timer.setInterval(16);
#else
    m_timer.setInterval(33);
#endif
    connect(&m_timer, &QTimer::timeout, this, [this] {
        sampleMotion();
        if (std::abs(m_motion.position - m_target) < .0015 && std::abs(m_motion.velocity) < .023) settle();
        update();
    });
    connect(model, &QAbstractItemModel::modelReset, this, [this] {
        m_target = 0;
        m_wheelRemainder = 0;
        settle();
        update();
    });
    connect(model, &QAbstractItemModel::rowsInserted, this, [this] { prefetch(); update(); });
    connect(model, &LibraryModel::pageLoaded, this, QOverload<>::of(&CoverFlowWidget::update));
    connect(model, &LibraryModel::error, this, [this](const QString &) { update(); });
    connect(artwork, &ArtworkCache::ready, this, [this](const QString &) { update(); });
}
int CoverFlowWidget::visibleCoverCount() const {
    const auto [first, last] = visibleRows(m_motion.position);
    return qMax(0, last - first + 1);
}
double CoverFlowWidget::coverSize() const {
    return qMax(1., qMin(width() * .40, qMin(height() * .65, height() * .965 - 58.)));
}
QRect CoverFlowWidget::centreCoverRect() const {
    const double size = coverSize();
    return QRectF((width() - size) / 2., height() * .035, size, size).toAlignedRect();
}
QPair<int, int> CoverFlowWidget::visibleRows(double position) const {
    // Cull once a cover's inner edge leaves the viewport. Fractional positions
    // let edge covers slide out naturally during navigation.
    const double halfViewport = width() / (2. * coverSize());
    const double screenReach = coverReach(halfViewport);
    const int cacheReach = (m_artwork->prefetchCapacity() - 1) / 2;
    const double reach = qMin(screenReach, double(cacheReach));
    return {qMax(0, int(std::ceil(position - reach))),
            qMin(m_model->rowCount() - 1, int(std::floor(position + reach)))};
}
void CoverFlowWidget::select(int index, bool animate) {
    index = qBound(0, index, qMax(0, m_model->rowCount() - 1));
    if (m_target == index) return;
    if (m_timer.isActive()) sampleMotion();
    m_target = index;
    if (animate && !m_reduced && isVisible()) {
        m_start = m_motion;
        m_clock.restart();
        m_timer.start();
        prefetch();
    } else settle();
    update();
    emit selectionChanged(index);
}
void CoverFlowWidget::sampleMotion() {
    m_motion = advanceCoverMotion(m_start, m_target, m_clock.nsecsElapsed() / 1.e9);
    const double last = qMax(0, m_model->rowCount() - 1);
    if (m_motion.position < 0. || m_motion.position > last) {
        m_motion = {qBound(0., m_motion.position, last), 0.};
        m_start = m_motion;
        m_clock.restart();
    }
}
void CoverFlowWidget::settle() {
    m_timer.stop();
    m_motion = {double(m_target), 0.};
    prefetch();
}
void CoverFlowWidget::setReducedMotion(bool reduced) {
    m_reduced = reduced;
    if (reduced && m_timer.isActive()) {
        settle();
        update();
    }
}
void CoverFlowWidget::setPlayback(const PlayerState &state) {
    const bool active = (state.state == "play" || state.state == "pause") && !state.track.file.isEmpty();
    QStringList caption;
    if (active) {
        const auto &track = state.track;
        caption << (track.title.isEmpty() ? QString("Unknown song") : track.title)
                << (track.artist.isEmpty() ? QString("Unknown artist") : track.artist)
                << (track.album.isEmpty() ? QString("Unknown album") : albumWithYear(track.album, track.year));
    }
    const auto albumId = active ? state.track.albumId : QString();
    if (caption == m_playingCaption && albumId == m_playingId) return;
    m_playingCaption = caption;
    m_playingId = albumId;
    setAccessibleDescription(caption.join('\n'));
    update();
}
void CoverFlowWidget::setCaptionClearance(int lowerEdge) {
    if (lowerEdge == m_captionClearance) return;
    m_captionClearance = lowerEdge;
    update();
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
    const auto [first, last] = visibleRows(m_motion.position);
    QVector<int> rows;
    for (int i = first; i <= last; ++i) rows.append(i);
    std::sort(rows.begin(), rows.end(), [this](int a, int b) {
        const auto da = std::abs(a - m_motion.position);
        const auto db = std::abs(b - m_motion.position);
        if (da == db) return b == m_target && a != m_target;
        return da > db;
    });
    const double size = coverSize();
    const double top = height() * .035;
    const QPointF origin(width() / 2., top);
    struct Card {
        int row;
        CoverQuads quads;
        const Artwork *art;
    };
    QVector<Card> cards;
    cards.reserve(rows.size());
    for (int row : rows) {
        cards.append({row, projectCover(row - m_motion.position, size, origin),
                      m_artwork->find(m_model->album(row)->artUrl)});
    }

    // Mask neighbouring reflections back to front. Paint covers in a separate
    // pass so a sloping reflection never cuts through an adjacent cover face.
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    for (const auto &card : cards) {
        painter.drawPolygon(card.quads.reflection);
        if (card.art) drawProjected(painter, card.art->reflection, card.quads.reflection);
    }
    for (const auto &card : cards) {
        const auto &album = *m_model->album(card.row);
        const auto &poly = card.quads.face;
        if (card.art && !card.art->cover.isNull()) {
            drawProjected(painter, card.art->cover, poly);
        } else {
            painter.setBrush(QColor::fromHsv((card.row * 37 + 210) % 360, 65, 42));
            painter.setPen(QColor("#55555d")); painter.drawPolygon(poly);
            painter.setPen(QColor("#d8d8df"));
            painter.drawText(poly.boundingRect().adjusted(8, 8, -8, -8), Qt::AlignCenter | Qt::TextWordWrap, album.title);
        }
        painter.setBrush(Qt::NoBrush);
        const double focus = 1. - qMin(1., std::abs(card.row - m_motion.position));
        painter.setPen(QPen(album.id == m_playingId ? Qt::white : QColor("#62626b"), .6 + .9 * focus * focus));
        painter.drawPolygon(poly);
        m_hits.append({card.row, poly});
    }
    auto caption = m_playingCaption;
    if (caption.isEmpty()) {
        if (const auto *selected = m_model->album(qRound(m_motion.position))) {
            caption << albumWithYear(selected->title, selected->year);
            if (!selected->artist.isEmpty()) caption << selected->artist;
        }
    }
    const int textWidth = qMin(width() - 16, qMax(int(size * 1.65), 288));
    const int y = qMax(qRound(top + size), m_captionClearance) + 8;
    const int textLeft = (width() - textWidth) / 2;
    const int available = height() - y;
    QFont font = painter.font();
    int lineTop = y;
    for (int i = 0; i < caption.size(); ++i) {
        const int ideal = i == 0 ? qBound(16, width() / 48, 24) : qBound(13, width() / 68, 18);
        font.setPixelSize(qMin(ideal, qMax(11, (available - 9) / qMax(1, caption.size()))));
        font.setWeight(i == 0 ? QFont::DemiBold : QFont::Normal);
        painter.setFont(font);
        const auto metrics = painter.fontMetrics();
        const int lineHeight = metrics.height() + 1;
        const QRect line(textLeft, lineTop, textWidth, lineHeight);
        const auto text = metrics.elidedText(caption[i], i == 0 && !m_playingCaption.isEmpty() ? Qt::ElideRight : Qt::ElideMiddle, textWidth);
        painter.setPen(Qt::black);
        painter.drawText(line.translated(0, 1), Qt::AlignCenter, text);
        painter.setPen(i == 0 ? Qt::white : QColor("#c2c2cf"));
        painter.drawText(line, Qt::AlignCenter, text);
        lineTop += lineHeight;
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
    if (e->phase() == Qt::ScrollBegin) m_wheelRemainder = 0;
    const bool pixelScroll = !e->pixelDelta().isNull();
    const auto delta = pixelScroll ? e->pixelDelta() : e->angleDelta();
    const int dominant = std::abs(delta.y()) >= std::abs(delta.x()) ? delta.y() : delta.x();
    m_wheelRemainder -= dominant / (pixelScroll ? 52. : 120.);
    const int steps = int(m_wheelRemainder);
    m_wheelRemainder -= steps;
    if (steps != 0) select(m_target + steps);
    if (e->phase() == Qt::ScrollEnd) m_wheelRemainder = 0;
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
