#pragma once
#include "artwork/ArtworkCache.h"
#include "coverflow/CoverFlowGeometry.h"
#include "models/LibraryModel.h"
#include <QElapsedTimer>
#include <QPolygonF>
#include <QStringList>
#include <QTimer>
#include <QWidget>

namespace PiTunes {
class CoverFlowWidget : public QWidget {
    Q_OBJECT
public:
    CoverFlowWidget(LibraryModel *model, ArtworkCache *artwork, QWidget *parent = nullptr);
    int currentIndex() const { return m_target; }
    double visualPosition() const { return m_motion.position; }
    void select(int index, bool animate = true);
    void setReducedMotion(bool reduced);
    bool reducedMotion() const { return m_reduced; }
    bool animating() const { return m_timer.isActive(); }
    int visibleCoverCount() const;
    quint64 paintCount() const { return m_paints; }
    QRect centreCoverRect() const;
    QRect captionRect() const;
    int captionAlbumIndex() const { return m_captionRow; }
    void setTransportInset(int pixels);
    void paintArtworkBackdrop(QPainter &painter) const;
    void setPlayback(const PlayerState &state);
    void setCaptionClearance(int lowerEdge);
signals:
    void selectionChanged(int index);
    void navigationSettled();
    void artworkBackdropChanged();
    void activated(int index);
protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    bool event(QEvent *) override;
private:
    void prefetch();
    void sampleMotion();
    void settle();
    void moveTo(double position, bool animate, double response);
    void updateCaption();
    void paintCovers(QPainter &painter, double size, double top,
                     QVector<QPair<int, QPolygonF>> *hits = nullptr) const;
    double coverSize() const;
    QPair<int, int> visibleRows(double position) const;
    LibraryModel *m_model;
    ArtworkCache *m_artwork;
    QTimer m_timer;
    QTimer m_trackpadIdle;
    QElapsedTimer m_clock;
    CoverMotion m_motion, m_start;
    double m_goal = 0., m_response = 15.;
    double m_wheelRemainder = 0;
    bool m_trackpadActive = false;
    int m_target = 0;
    int m_captionRow = -1;
    bool m_reduced = false, m_dragging = false;
    QPointF m_press;
    QVector<QPair<int, QPolygonF>> m_hits;
    QImage m_motionSurface;
    QString m_playingId;
    QStringList m_playingCaption;
    QStringList m_caption;
    bool m_captionShowsPlayback = false;
    int m_captionClearance = 0;
    int m_transportInset = 0;
    quint64 m_paints = 0;
};
} // namespace PiTunes
