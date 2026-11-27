#pragma once

#include "models/Domain.h"
#include <QElapsedTimer>
#include <QFrame>
#include <QTimer>

class QLabel;
class QModelIndex;
class QListView;
class QPushButton;

namespace PiTunes {
class CoverFlowWidget;
class TrackRowDelegate;

class TrackDrawer : public QFrame {
    Q_OBJECT
public:
    TrackDrawer(CoverFlowWidget *stage, const QString &title, const QString &year,
                const QVector<Track> &tracks, bool albumContext);
    void setPlayback(const PlayerState &state);
    void setReducedMotion(bool reduced);
    void setAlbumFavourite(bool starred, bool ready);
    void open();

signals:
    void dismissRequested();
    void playRequested(int row);
    void trackActionsRequested(int row, const QPoint &position);
    void albumFavouriteRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void place();
    void syncPulse();
    void showTrackActions(const QModelIndex &index);
    CoverFlowWidget *m_stage;
    QListView *m_list;
    QLabel *m_title;
    QPushButton *m_heart = nullptr;
    TrackRowDelegate *m_rowDelegate;
    const QString m_heading;
    QVector<int> m_activeRows;
    QTimer m_pulse;
    QElapsedTimer m_pulseTime;
    bool m_playing = false;
    bool m_reduced = false;
};
} // namespace PiTunes
