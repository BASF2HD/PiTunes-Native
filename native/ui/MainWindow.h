#pragma once
#include "coverflow/CoverFlowWidget.h"
#include <QLabel>
#include <QHash>
#include <QLineEdit>
#include <QMainWindow>
#include <QPointer>
#include <QPushButton>
#include <QSlider>
#include <QToolButton>
#include <QQueue>
#include <QSet>

class QHBoxLayout;
class QJsonArray;
class QMenu;
namespace PiTunes {
class TrackDrawer;
class MediaInfoPanel;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QUrl api, bool demo = false, bool reduced = false);
    CoverFlowWidget *coverFlow() const { return m_flow; }
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    void refreshPlayer();
    void command(const QString &path, const QJsonObject &body = {});
    void sendNextCommand();
    void showAlbum(int row, bool play = false);
    void showTracks(const QString &title, const QVector<Track> &tracks, const QString &albumId = {}, const QString &year = {});
    void closeTrackDrawer();
    void showCollection(QString title, QString path, QString key, QString filterPrefix, QString sort = {});
    void buildBrowseMenus(QHBoxLayout *navigation, QWidget *root, bool reduced);
    void openBrowseMenu(QToolButton *button);
    void browseAlbums(const QString &filter, const QString &sort);
    void browseSongs();
    void showRadio();
    void setMessage(const QString &message);
    void selectAlbumOnStrip(int row);
    void syncAlbumStrip();
    void focusInitialPlaybackAlbum();
    int playingAlbumRow() const;
    int returnAlbumRow() const;
    void scheduleReturnToPlayback();
    void watchBrowsePopup(QMenu *menu);
    void placeOverlays();
    void refreshFavourites();
    void updateFavouriteHearts();
    void syncCaptionHeart();
    QPair<QString, bool> captionFavouriteSubject() const;
    void toggleFavourite(const QString &id, bool album);
    void showCaptionActions();
    void showSubjectActions(const QPoint &anchor, const Album &album, const Track &track,
                            const QVector<Track> &list = {}, int row = -1);
    void showMediaInfo(const Album &album, const Track &track, int row);
    void closeMediaInfo();
    void choosePlaylist(const QJsonArray &ids);
    void createPlaylist(const QJsonArray &ids);
    void appendToPlaylist(const QString &id, const QJsonArray &tracks, int row);
    ApiClient *m_api;
    LibraryModel *m_library;
    ArtworkCache *m_artwork;
    CoverFlowWidget *m_flow;
    QPointer<TrackDrawer> m_trackDrawer;
    QPointer<MediaInfoPanel> m_infoPanel;
    QHash<QToolButton *, QMenu *> m_browseMenus;
    QPointer<QToolButton> m_pressedBrowseTab;
    QString m_drawerAlbumId;
    QLabel *m_message, *m_time;
    QToolButton *m_play, *m_repeat, *m_shuffle, *m_volumeButton, *m_mute;
    QToolButton *m_captionActions;
    QToolButton *m_captionHeart;
    QFrame *m_searchPanel, *m_transportPanel;
    QSlider *m_seek, *m_volume, *m_albumStrip;
    int m_volumeBeforeMute = 25;
    QTimer m_poll, m_searchDelay, m_snapBack;
    PlayerState m_player;
    bool m_demo = false, m_pollPending = false, m_commandPending = false;
    bool m_initialAlbumFocused = false;
    bool m_searchResults = false, m_favouritesReady = false, m_favouritesLoading = false;
    QString m_preSearchFilter, m_preSearchSort;
    QString m_albumScope, m_albumSort = "year-desc";
    QString m_songScope = "all", m_songSort, m_artistSort = "year-desc", m_playlistSort;
    QString m_currentPlaylistId, m_currentPlaylistName;
    QSet<QString> m_albumFavourites, m_trackFavourites, m_pendingFavourites;
    int m_pendingAlbumRow = -1;
    quint64 m_playerGeneration = 0, m_drawerGeneration = 0, m_favouritesRevision = 0;
    quint64 m_pendingInspection = 0;
    struct Command { QString path; QJsonObject body; };
    QQueue<Command> m_commands;
};
}
