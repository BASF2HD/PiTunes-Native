#pragma once
#include "coverflow/CoverFlowWidget.h"
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPointer>
#include <QPushButton>
#include <QSlider>
#include <QQueue>
namespace PiTunes {
class TrackDrawer;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QUrl api, bool demo = false, bool reduced = false);
    CoverFlowWidget *coverFlow() const { return m_flow; }
private:
    void refreshPlayer();
    void command(const QString &path, const QJsonObject &body = {});
    void sendNextCommand();
    void showAlbum(int row, bool play = false);
    void showTracks(const QString &title, const QVector<Track> &tracks, const QString &albumId = {}, const QString &year = {});
    void closeTrackDrawer();
    void showCollection(QString title, QString path, QString key, QString filterPrefix);
    void showRadio();
    void setMessage(const QString &message);
    ApiClient *m_api;
    LibraryModel *m_library;
    ArtworkCache *m_artwork;
    CoverFlowWidget *m_flow;
    QPointer<TrackDrawer> m_trackDrawer;
    QLabel *m_message, *m_elapsed, *m_duration;
    QPushButton *m_play, *m_repeat, *m_shuffle;
    QSlider *m_seek, *m_volume;
    QTimer m_poll, m_searchDelay;
    PlayerState m_player;
    bool m_demo = false, m_pollPending = false, m_commandPending = false;
    quint64 m_playerGeneration = 0, m_drawerGeneration = 0;
    struct Command { QString path; QJsonObject body; };
    QQueue<Command> m_commands;
};
}
