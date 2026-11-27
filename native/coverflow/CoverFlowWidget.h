#pragma once
#include "artwork/ArtworkCache.h"
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
    void select(int index, bool animate = true);
    void setReducedMotion(bool reduced);
    bool animating() const { return m_timer.isActive(); }
    int visibleCoverCount() const;
    quint64 paintCount() const { return m_paints; }
    QRect centreCoverRect() const;
    void setPlayback(const PlayerState &state);
    void setCaptionClearance(int lowerEdge);
signals:
    void selectionChanged(int index);
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
    struct Motion {
        double slide = 0;
        double turn = 0;
        double scale = 0;
    };
    void prefetch();
    void sampleMotion();
    void settle();
    double coverSize() const;
    QPair<int, int> visibleRows(double position) const;
    QPolygonF polygon(int row, double size, double top) const;
    LibraryModel *m_model;
    ArtworkCache *m_artwork;
    QTimer m_timer;
    QElapsedTimer m_clock;
    Motion m_motion, m_start;
    double m_wheelRemainder = 0;
    int m_target = 0;
    bool m_reduced = false, m_dragging = false;
    QPointF m_press;
    QVector<QPair<int, QPolygonF>> m_hits;
    QString m_playingId;
    QStringList m_playingCaption;
    int m_captionClearance = 0;
    quint64 m_paints = 0;
};
} // namespace PiTunes
