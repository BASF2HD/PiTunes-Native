#include "CoverFlowWidget.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QTouchEvent>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace PiTunes {
namespace {
void drawProjected(QPainter &painter, const QImage &image, const QPolygonF &quad) {
    if (image.isNull()) return;
    if (quad[0].y() == quad[1].y() && quad[3].y() == quad[2].y()) {
        painter.drawImage(QRectF(quad[0], quad[2]), image);
        return;
    }
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
    m_startupClock.start();
    setFocusPolicy(Qt::StrongFocus); setAttribute(Qt::WA_AcceptTouchEvents);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(240, 150); setAccessibleName("Album CoverFlow");
    updateCaptionLayout();
    m_timer.setTimerType(Qt::PreciseTimer);
    setAnimationMode(AnimationMode::Full);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        sampleMotion();
        if (qRound(m_visualPosition) != m_captionRow) {
            updateCaption();
            prefetch();
        }
        if (m_seconds >= coverMoveDuration) {
            settle();
            emit navigationSettled();
        }
        update();
    });
    m_wheelIdle.setSingleShot(true);
    m_wheelIdle.setInterval(140);
    connect(&m_wheelIdle, &QTimer::timeout, this, [this] { m_wheelRemainder = 0.; m_wheelAxis = 0; });
    connect(model, &QAbstractItemModel::modelReset, this, [this] {
        m_wheelIdle.stop();
        m_target = 0;
        m_wheelRemainder = 0.;
        m_wheelAxis = 0;
        settle();
        update();
    });
    connect(model, &QAbstractItemModel::rowsInserted, this, [this] { updateCaption(); prefetch(); update(); });
    connect(model, &LibraryModel::pageLoaded, this, QOverload<>::of(&CoverFlowWidget::update));
    connect(model, &LibraryModel::error, this, [this](const QString &) { update(); });
    connect(artwork, &ArtworkCache::ready, this, [this](const QString &) {
        if (m_timer.isActive()) return;
        update();
        emit artworkBackdropChanged();
    });
}
int CoverFlowWidget::visibleCoverCount() const {
    const auto [first, last] = visibleRows(m_visualPosition);
    return qMax(0, last - first + 1);
}
double CoverFlowWidget::coverSize() const {
    const double fraction = m_transportInset > 0 ? .77 : .65;
    return qMax(1., qMin(width() * .46, qMin(height() * fraction,
                  height() - coverTop() - m_transportInset - m_captionHeight)));
}
QRect CoverFlowWidget::centreCoverRect() const {
    const double size = coverSize();
    return QRectF((width() - size) / 2., coverTop(), size, size).toAlignedRect();
}
QRect CoverFlowWidget::captionRect() const {
    const int desired = qMax(int(coverSize() * 1.05), m_captionPixels[0] * 12);
    const int textWidth = qMin(width() - 72, qBound(256, desired, 600));
    const int y = qMax(qRound(coverTop() + coverSize()), m_captionClearance) + 8;
    return {(width() - textWidth) / 2, y, textWidth, qMax(0, height() - m_transportInset - y)};
}
void CoverFlowWidget::setTransportInset(int pixels) {
    m_transportInset = pixels;
    updateCaptionLayout();
    prefetch();
    update();
}
void CoverFlowWidget::setCaptionFontSizes(CaptionFontSizes sizes) {
    const auto limit = [](int pixels) { return qBound(CaptionFontSizes::minimum, pixels, CaptionFontSizes::maximum); };
    m_captionFontSizes = {limit(sizes.title), limit(sizes.artist), limit(sizes.album)};
    updateCaptionLayout();
    prefetch();
    update();
    emit captionLayoutChanged();
    emit artworkBackdropChanged();
}
void CoverFlowWidget::updateCaptionLayout() {
    m_captionPixels = {m_captionFontSizes.title, m_captionFontSizes.artist, m_captionFontSizes.album};
    const auto lineHeight = [this](int pixels, bool title) {
        QFont type = font();
        type.setWeight(title ? QFont::DemiBold : QFont::Normal);
        type.setPixelSize(pixels);
        return QFontMetrics(type).height() + 1;
    };
    const auto totalHeight = [&] {
        return lineHeight(m_captionPixels[0], true) + lineHeight(m_captionPixels[1], false)
            + lineHeight(m_captionPixels[2], false);
    };
    const int budget = qMax(35, (height() - m_transportInset) * 3 / 10);
    const int requestedHeight = totalHeight();
    if (requestedHeight > budget) {
        for (auto &pixels : m_captionPixels) pixels = qMax(8, pixels * budget / requestedHeight);
    }
    m_captionHeight = qMax(55, totalHeight() + 12);
}
QPair<int, int> CoverFlowWidget::visibleRows(double position) const {
    // Cull once a cover's inner edge leaves the viewport. Fractional positions
    // let edge covers slide out naturally during navigation.
    const double halfViewport = width() / (2. * coverSize());
    const double screenReach = coverReach(halfViewport);
    const int cacheReach = (m_artwork->prefetchCapacity() - 1) / 2;
    const int modeReach = m_animationMode == AnimationMode::Lightweight ? qMin(3, cacheReach) : cacheReach;
    const double reach = qMin(screenReach, double(modeReach));
    return {qMax(0, int(std::ceil(position - reach))),
            qMin(m_model->rowCount() - 1, int(std::floor(position + reach)))};
}
void CoverFlowWidget::select(int index, bool animate) {
    index = qBound(0, index, qMax(0, m_model->rowCount() - 1));
    const bool moving = animate && !reducedMotion() && isVisible();
    if (index == m_target && (moving || !m_timer.isActive())) return;
    if (m_timer.isActive()) sampleMotion();
    const bool changed = m_target != index;
    if (moving) {
        // Retarget each cover from its current pose, as in the NaviGlassPlayer
        // renderer. Retain a bounded window around both ends of the move.
        QHash<int, CoverPose> from;
        for (const double centre : {m_visualPosition, double(index)}) {
            const auto [first, last] = visibleRows(centre);
            for (int row = first; row <= last; ++row) from.insert(row, poseAt(row));
        }
        m_startPoses = std::move(from);
        m_startPosition = m_visualPosition;
        m_seconds = 0.;
        m_target = index;
        m_clock.restart();
        if (!m_timer.isActive()) {
            m_timer.start();
            if (m_sampleFrames) m_activeFrameWindow.start();
        }
    } else {
        m_target = index;
        settle();
        update();
    }
    if (changed) emit selectionChanged(index);
}
CoverPose CoverFlowWidget::poseAt(int row) const {
    const auto to = coverPose(row - m_target);
    if (!m_timer.isActive()) return to;
    return animateCover(m_startPoses.value(row, coverPose(row - m_startPosition)), to, m_seconds);
}
void CoverFlowWidget::sampleMotion() {
    m_seconds = m_clock.nsecsElapsed() / 1.e9;
    m_visualPosition = m_startPosition + (m_target - m_startPosition) * coverMoveProgress(m_seconds);
}
void CoverFlowWidget::settle() {
    if (m_activeFrameWindow.isValid()) {
        m_frames.activeNs += m_activeFrameWindow.nsecsElapsed();
        m_activeFrameWindow.invalidate();
    }
    m_timer.stop();
    m_startPoses.clear();
    m_visualPosition = m_target;
    updateCaption();
    prefetch();
    emit artworkBackdropChanged();
}
void CoverFlowWidget::setReducedMotion(bool reduced) {
    setAnimationMode(reduced ? AnimationMode::Off : AnimationMode::Full);
}
void CoverFlowWidget::setAnimationMode(AnimationMode mode) {
    m_animationMode = mode;
    setFrameRate(m_frameRate);
    m_motionScale = 1.;
    m_fastFrames = 0;
    if (reducedMotion() && m_timer.isActive()) {
        settle();
        update();
        emit navigationSettled();
    } else {
        prefetch();
        update();
        emit artworkBackdropChanged();
    }
}
void CoverFlowWidget::setFrameRate(int framesPerSecond) {
    m_frameRate = framesPerSecond == 15 || framesPerSecond == 20 ? framesPerSecond : 30;
    m_timer.setInterval(qCeil(1000. / m_frameRate));
}

void CoverFlowWidget::setFrameSampling(bool enabled) {
    m_sampleFrames = enabled;
    m_frames = {};
    m_activeFrameWindow.invalidate();
    if (enabled && animating()) m_activeFrameWindow.start();
}
FrameSample CoverFlowWidget::takeFrameSample() {
    if (m_activeFrameWindow.isValid()) {
        m_frames.activeNs += m_activeFrameWindow.nsecsElapsed();
        m_activeFrameWindow.restart();
    }
    const auto sample = m_frames;
    m_frames = {};
    return sample;
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
    if (caption == m_playingCaption && state.track.albumId == m_captionPlayback.track.albumId &&
        state.track.file == m_captionPlayback.track.file && state.source == m_captionPlayback.source) return;
    m_playingCaption = caption;
    m_captionPlayback = state;
    updateCaption();
    update();
}
void CoverFlowWidget::updateCaption() {
    const int previousRow = m_captionRow;
    const bool previousPlayback = m_captionShowsPlayback;
    m_captionRow = qRound(m_visualPosition);
    const auto *album = m_model->album(m_captionRow);
    const auto *entry = m_model->entry(m_captionRow);
    m_captionShowsPlayback = !m_playingCaption.isEmpty() && (!entry || entry->matchesPlayback(m_captionPlayback));
    QStringList caption;
    if (m_captionShowsPlayback) caption = m_playingCaption;
    else if (album) {
        auto title = albumWithYear(album->title, album->year);
        QString subtitle = album->artist;
        const int separator = title.lastIndexOf(" | ");
        if (separator >= 0 && title.mid(separator + 3).toInt() > 0) {
            if (!subtitle.isEmpty()) subtitle += " | ";
            subtitle += title.mid(separator + 3);
            title.truncate(separator);
        }
        caption << title;
        if (!subtitle.isEmpty()) caption << subtitle;
        if (entry && entry->kind == CoverEntry::TrackCover && !entry->track.album.isEmpty())
            caption << albumWithYear(entry->track.album, entry->track.year);
    }
    const bool textChanged = caption != m_caption;
    if (textChanged) {
        m_caption = caption;
        setAccessibleDescription(caption.join('\n'));
    }
    if (textChanged || previousRow != m_captionRow || previousPlayback != m_captionShowsPlayback) emit captionChanged();
}
void CoverFlowWidget::setCaptionClearance(int lowerEdge) {
    if (lowerEdge == m_captionClearance) return;
    m_captionClearance = lowerEdge;
    update();
}
void CoverFlowWidget::prefetch() {
    QStringList keys;
    const int center = qRound(m_visualPosition);
    const auto [first, last] = visibleRows(center);
    const int reach = qMax(center - first, last - center);
    for (int step = 0; step <= reach; ++step) {
        for (int sign : {1, -1}) {
            if (step == 0 && sign == -1) continue;
            const auto *album = m_model->album(center + step * sign);
            if (album && !album->artUrl.isEmpty()) keys.append(album->artUrl);
        }
    }
    m_artwork->prefetch(keys);
}
void CoverFlowWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updateCaptionLayout();
    prefetch();
    emit captionLayoutChanged();
}
void CoverFlowWidget::paintEvent(QPaintEvent *) {
    ++m_paints;
    QElapsedTimer drawTime;
    if (m_timer.isActive()) drawTime.start();
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    if (m_model->rowCount() == 0) {
        m_hits.clear();
        painter.setPen(QColor("#a8a8b4"));
        painter.drawText(rect(), Qt::AlignCenter, m_model->loading() ? "Loading library…" : "No albums · choose a source in Settings");
        return;
    }
    const double size = coverSize();
    const double top = coverTop();
    // Pixel work, independent of the number of covers and their projections.
    // Keep the full scene and composite it at a lower sampling density only
    // when movement exceeds the paint budget. Static frames stay native-size.
    const int sceneHeight = qMin(height(), qCeil(top + size * 1.55) + 2);
    const QRect scene(0, 0, width(), sceneHeight);
    const double scale = qMin(m_motionScale, std::sqrt(2000000. / qMax(1., double(width()) * sceneHeight)));
    if (drawTime.isValid() && (scale < .99 || devicePixelRatioF() > 1.)) {
        const QSize surfaceSize(qMax(1, qCeil(width() * scale)), qMax(1, qCeil(sceneHeight * scale)));
        if (m_motionSurface.size() != surfaceSize) m_motionSurface = QImage(surfaceSize, QImage::Format_RGB32);
        if (!m_motionSurface.isNull()) {
            m_motionSurface.fill(Qt::black);
            {
                QPainter covers(&m_motionSurface);
                covers.scale(double(surfaceSize.width()) / width(), double(surfaceSize.height()) / sceneHeight);
                paintCovers(covers, size, top, &m_hits);
            }
            painter.drawImage(scene, m_motionSurface);
        } else paintCovers(painter, size, top, &m_hits);
    } else {
        m_motionSurface = QImage();
        paintCovers(painter, size, top, &m_hits);
    }
    if (m_firstCoverPaintMs < 0) {
        const int centre = qRound(m_visualPosition);
        const auto *album = m_model->album(centre);
        const auto *art = album ? m_artwork->find(album->artUrl) : nullptr;
        if (art && !art->cover.isNull() &&
            std::any_of(m_hits.cbegin(), m_hits.cend(), [centre](const auto &hit) { return hit.first == centre; }))
            m_firstCoverPaintMs = m_startupClock.elapsed();
    }
    const auto &caption = m_caption;
    const auto textArea = captionRect();
    painter.setClipRect(textArea);
    const int textWidth = textArea.width();
    QFont font = painter.font();
    int lineTop = textArea.y();
    for (int i = 0; i < caption.size(); ++i) {
        font.setPixelSize(m_captionPixels[i]);
        font.setWeight(i == 0 ? QFont::DemiBold : QFont::Normal);
        painter.setFont(font);
        const auto metrics = painter.fontMetrics();
        const int lineHeight = metrics.height() + 1;
        const QRect line(textArea.x(), lineTop, textWidth, lineHeight);
        const auto text = metrics.elidedText(caption[i], i == 0 && m_captionShowsPlayback ? Qt::ElideRight : Qt::ElideMiddle, textWidth);
        painter.setPen(Qt::black);
        painter.drawText(line.translated(0, 1), Qt::AlignCenter, text);
        painter.setPen(i == 0 ? Qt::white : QColor("#c2c2cf"));
        painter.drawText(line, Qt::AlignCenter, text);
        lineTop += lineHeight;
    }
    painter.end();
    if (drawTime.isValid()) {
        const auto ns = drawTime.nsecsElapsed();
        if (m_sampleFrames) {
            ++m_frames.count;
            m_frames.totalNs += ns;
            m_frames.peakNs = qMax(m_frames.peakNs, ns);
        }
        // Hysteresis avoids sharpening and softening on successive frames.
        if (ns > 28'000'000 && m_motionScale > .45) {
            m_motionScale = qMax(.45, m_motionScale * (ns > 65'000'000 ? .68 : .82));
            m_fastFrames = 0;
        } else if (ns < 10'000'000 && m_motionScale < 1.) {
            if (++m_fastFrames == 12) {
                m_motionScale = qMin(1., m_motionScale * 1.10);
                m_fastFrames = 0;
            }
        } else m_fastFrames = 0;
    }
}
void CoverFlowWidget::paintArtworkBackdrop(QPainter &painter) const {
    if (m_model->rowCount() == 0) return;
    painter.save();
    painter.setClipRect(rect(), Qt::IntersectClip);
    paintCovers(painter, coverSize(), coverTop());
    painter.restore();
}
void CoverFlowWidget::paintCovers(QPainter &painter, double size, double top,
                                  QVector<QPair<int, QPolygonF>> *hits) const {
    painter.setRenderHint(QPainter::Antialiasing);
    const bool lightMotion = m_timer.isActive() && m_animationMode == AnimationMode::Lightweight;
    painter.setRenderHint(QPainter::SmoothPixmapTransform, !lightMotion);
    if (hits) hits->clear();
    const auto [first, last] = visibleRows(m_visualPosition);
    const QPointF origin(width() / 2., top);
    struct Card {
        int row;
        CoverQuads quads;
        const Artwork *art;
    };
    QVector<Card> cards;
    cards.reserve(last - first + 1 + m_startPoses.size());
    const auto addCard = [&](int row) {
        const auto *album = m_model->album(row);
        if (!album) return;
        const auto pose = poseAt(row);
        auto quads = projectCover(pose, size, origin);
        if (lightMotion) {
            // Shorten the reflection while moving, keeping its projected floor edge.
            quads.reflection[2] = quads.reflection[1] + .45 * (quads.reflection[2] - quads.reflection[1]);
            quads.reflection[3] = quads.reflection[0] + .45 * (quads.reflection[3] - quads.reflection[0]);
        }
        if (!quads.face.boundingRect().intersects(QRectF(rect())) &&
            !quads.reflection.boundingRect().intersects(QRectF(rect()))) return;
        cards.append({row, std::move(quads), m_artwork->find(album->artUrl)});
    };
    for (int row = first; row <= last; ++row) addCard(row);
    if (!lightMotion) {
        for (auto it = m_startPoses.cbegin(); it != m_startPoses.cend(); ++it) {
            if (it.key() < first || it.key() > last) addCard(it.key());
        }
    }
    std::sort(cards.begin(), cards.end(), [this](const Card &a, const Card &b) {
        const auto distanceA = std::abs(a.row - m_visualPosition);
        const auto distanceB = std::abs(b.row - m_visualPosition);
        if (distanceA != distanceB) return distanceA > distanceB;
        return a.row < b.row;
    });

    // Mask neighbouring reflections back to front. Paint covers in a separate
    // pass so a sloping reflection never cuts through an adjacent cover face.
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    for (const auto &card : cards) {
        const bool drawArt = card.art && !card.art->reflection.isNull() &&
            (!lightMotion || std::abs(card.row - m_visualPosition) < 1.6);
        if (!drawArt || card.art->reflection.hasAlphaChannel()) painter.drawPolygon(card.quads.reflection);
        if (drawArt) drawProjected(painter, card.art->reflection, card.quads.reflection);
    }
    for (const auto &card : cards) {
        const auto &album = *m_model->album(card.row);
        const auto &poly = card.quads.face;
        if (card.art && !card.art->cover.isNull()) {
            drawProjected(painter, card.art->cover, poly);
        } else {
            painter.setBrush(QColor::fromHsv((card.row * 37 + 210) % 360, 65, 42));
            painter.setPen(Qt::NoPen);
            painter.drawPolygon(poly);
            painter.setPen(QColor("#d8d8df"));
            painter.drawText(poly.boundingRect().adjusted(8, 8, -8, -8), Qt::AlignCenter | Qt::TextWordWrap, album.title);
        }
        if (hits) hits->append({card.row, poly});
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
    if (e->phase() == Qt::ScrollBegin || e->phase() == Qt::ScrollEnd) {
        m_wheelRemainder = 0.;
        m_wheelAxis = 0;
    }
    if (e->phase() == Qt::ScrollEnd && e->pixelDelta().isNull()) {
        e->accept();
        return;
    }
    const bool pixelScroll = !e->pixelDelta().isNull();
    const auto delta = pixelScroll ? e->pixelDelta() : e->angleDelta();
    const int axis = std::abs(delta.y()) >= std::abs(delta.x()) ? 2 : 1;
    if (pixelScroll && !m_wheelAxis) m_wheelAxis = axis;
    const int dominant = (pixelScroll ? m_wheelAxis : axis) == 2 ? delta.y() : delta.x();
    // NaviGlassPlayer accumulates whole-cover steps from clamped wheel
    // packets. A burst updates the destination of the running transition.
    const double step = pixelScroll ? qBound(-1.2, -dominant * .018, 1.2) : -dominant / 120.;
    m_wheelRemainder += step;
    const int steps = int(m_wheelRemainder + std::copysign(1.e-9, m_wheelRemainder));
    m_wheelRemainder -= steps;
    if (steps != 0) select(m_target + steps);
    if (dominant != 0) m_wheelIdle.start();
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
        if (it->first != m_target) select(it->first);
        else if (const auto *entry = m_model->entry(m_target); entry && entry->kind != CoverEntry::StationCover)
            emit activated(m_target);
        break;
    }
}
void CoverFlowWidget::mouseDoubleClickEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton) return;
    for (auto it = m_hits.crbegin(); it != m_hits.crend(); ++it) {
        if (!it->second.containsPoint(e->localPos(), Qt::OddEvenFill)) continue;
        const auto *entry = m_model->entry(it->first);
        if (entry && entry->kind == CoverEntry::StationCover) {
            select(it->first);
            emit activated(it->first);
        }
        break;
    }
    e->accept();
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
