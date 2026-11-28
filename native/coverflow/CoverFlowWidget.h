#pragma once
#include "artwork/ArtworkCache.h"
#include "coverflow/CoverFlowGeometry.h"
#include "models/LibraryModel.h"
#include <QElapsedTimer>
#include <QPolygonF>
#include <QStringList>
#include <QTimer>
#include <QWidget>
#include <array>

namespace PiTunes {
enum class AnimationMode { Full, Lightweight, Off };

struct FrameSample {
    int count = 0;
    qint64 totalNs = 0;
    qint64 peakNs = 0;
    qint64 activeNs = 0;
};

class CoverFlowWidget : public QWidget {
    Q_OBJECT
public:
    CoverFlowWidget(LibraryModel *model, ArtworkCache *artwork, QWidget *parent = nullptr);
    int currentIndex() const { return m_target; }
    double visualPosition() const { return m_visualPosition; }
    void select(int index, bool animate = true);
    void setReducedMotion(bool reduced);
    void setAnimationMode(AnimationMode mode);
    void setFrameRate(int framesPerSecond);
    int frameRate() const { return m_frameRate; }
    AnimationMode animationMode() const { return m_animationMode; }
    bool reducedMotion() const { return m_animationMode == AnimationMode::Off; }
    void setFrameSampling(bool enabled);
    FrameSample takeFrameSample();
    qint64 firstCoverPaintMs() const { return m_firstCoverPaintMs; }
    bool animating() const { return m_timer.isActive(); }
    int visibleCoverCount() const;
    quint64 paintCount() const { return m_paints; }
    QRect centreCoverRect() const;
    QRect captionRect() const;
    int captionAlbumIndex() const { return m_captionRow; }
    bool captionShowsPlayback() const { return m_captionShowsPlayback; }
    void setTransportInset(int pixels);
    CaptionFontSizes captionFontSizes() const { return m_captionFontSizes; }
    void setCaptionFontSizes(CaptionFontSizes sizes);
    void paintArtworkBackdrop(QPainter &painter) const;
    void setPlayback(const PlayerState &state);
    void setCaptionClearance(int lowerEdge);
signals:
    void selectionChanged(int index);
    void navigationSettled();
    void artworkBackdropChanged();
    void captionLayoutChanged();
    void captionChanged();
    void activated(int index);
protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    bool event(QEvent *) override;
private:
    void prefetch();
    void sampleMotion();
    void settle();
    CoverPose poseAt(int row) const;
    void updateCaption();
    void updateCaptionLayout();
    void paintCovers(QPainter &painter, double size, double top,
                     QVector<QPair<int, QPolygonF>> *hits = nullptr) const;
    double coverSize() const;
    double coverTop() const { return height() * .012; }
    QPair<int, int> visibleRows(double position) const;
    LibraryModel *m_model;
    ArtworkCache *m_artwork;
    QTimer m_timer;
    QTimer m_wheelIdle;
    QElapsedTimer m_clock, m_activeFrameWindow, m_startupClock;
    qint64 m_firstCoverPaintMs = -1;
    QHash<int, CoverPose> m_startPoses;
    double m_visualPosition = 0., m_startPosition = 0., m_seconds = 0.;
    double m_wheelRemainder = 0;
    int m_target = 0;
    int m_captionRow = -1;
    AnimationMode m_animationMode = AnimationMode::Full;
    bool m_dragging = false, m_sampleFrames = false;
    FrameSample m_frames;
    int m_wheelAxis = 0;
    QPointF m_press;
    QVector<QPair<int, QPolygonF>> m_hits;
    QImage m_motionSurface;
    double m_motionScale = 1.;
    int m_fastFrames = 0, m_frameRate = 30;
    PlayerState m_captionPlayback;
    QStringList m_playingCaption;
    QStringList m_caption;
    bool m_captionShowsPlayback = false;
    int m_captionClearance = 0;
    int m_transportInset = 0;
    CaptionFontSizes m_captionFontSizes;
    std::array<int, 3> m_captionPixels{};
    int m_captionHeight = 0;
    quint64 m_paints = 0;
};
} // namespace PiTunes
