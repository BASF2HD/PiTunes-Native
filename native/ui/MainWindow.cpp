#include "MainWindow.h"
#include "SettingsDialog.h"
#include "TouchKeyboard.h"
#include "TrackDrawer.h"
#include "PlayerIcons.h"
#include <QButtonGroup>
#include <QSignalBlocker>
#include <QWidgetAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QListView>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QStandardItemModel>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QSettings>
#include <QShortcut>
#include <QStyleOptionSlider>

namespace PiTunes {
namespace {
class AlbumStrip : public QSlider {
public:
    explicit AlbumStrip(QWidget *parent) : QSlider(Qt::Horizontal, parent) {}

protected:
    void mousePressEvent(QMouseEvent *event) override {
        QStyleOptionSlider option;
        initStyleOption(&option);
        const auto handle = style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this);
        if (event->button() == Qt::LeftButton && !handle.contains(event->pos())) {
            const auto groove = style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderGroove, this);
            const int span = groove.width() - handle.width();
            if (span > 0) {
                const int position = qBound(0, event->x() - groove.left() - handle.width() / 2, span);
                setValue(QStyle::sliderValueFromPosition(minimum(), maximum(), position, span, option.upsideDown));
                event->accept();
                return;
            }
        }
        QSlider::mousePressEvent(event);
    }
};
}

MainWindow::MainWindow(QUrl api, bool demo, bool reduced) : m_demo(demo) {
    setWindowTitle("PiTunes Native"); setWindowIcon(QIcon(":/brand/icon.png")); resize(1100, 720);
    m_api = new ApiClient(std::move(api), this);
    m_library = new LibraryModel(m_api, this);
    reduced = reduced || QSettings().value("native/reducedMotion", false).toBool();
    m_artwork = new ArtworkCache(m_api, reduced ? 6 : 12, this);
    auto *root = new QWidget(this); auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(20, 12, 20, 10); layout->setSpacing(4); setCentralWidget(root);
    root->setStyleSheet(
        "QToolButton { background: transparent; border: 1px solid transparent; color: #f4f5fa; padding: 0; }"
        "QToolButton:hover, QToolButton:focus { color: white; background: #1a1d25; border: 1px solid #414651; border-radius: 5px; }"
        "QToolButton::menu-indicator { width: 0; }"
        "QToolButton#roundControl { border: 1px solid #383b45; border-radius: 16px; }"
        "QToolButton#mainPlay { border: 1px solid #686b78; border-radius: 23px;"
          "background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #373942,stop:1 #20222a); }"
        "QToolButton#mainPlay:hover, QToolButton#mainPlay:focus { border-color: #b5bac8; background: #343846; }"
        "QToolButton#browseTab { border-radius: 0; border: 1px solid transparent; font-size: 10px; font-weight: 600; }"
        "QToolButton#browseTab:checked { border-bottom: 2px solid #e0e3ee; }"
        "QToolButton#browseTab:focus { background: #20232d; }"
        "QMenu { background: #151722; color: #ecedf5; border: 1px solid #424653; padding: 4px; }"
        "QMenu::item { padding: 6px 20px 6px 26px; }"
        "QMenu::item:selected { background: #343c53; }");
    const auto iconButton = [root](PlayerIcon icon, const QString &label, int side = 32) {
        auto *button = new QToolButton(root);
        button->setFixedSize(side, side);
        button->setIconSize(QSize(20, 20));
        button->setIcon(playerIcon(icon));
        button->setAccessibleName(label);
        button->setToolTip(label);
        return button;
    };
    auto *header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 3);
    header->setSpacing(9);
    layout->addLayout(header);
    m_repeat = iconButton(PlayerIcon::Repeat, "Repeat off");
    m_repeat->setIcon(playerIcon(PlayerIcon::Repeat, false));
    header->addWidget(m_repeat);
    connect(m_repeat, &QToolButton::clicked, this, [this] {
        const auto mode = m_player.repeatMode == "all" ? "song" : m_player.repeatMode == "song" ? "off" : "all";
        command("/api/player/options", {{"repeatMode", mode}});
    });
    m_time = new QLabel("0:00 / --:--", root);
    m_time->setMinimumWidth(81);
    m_time->setAccessibleName("Playback time");
    header->addWidget(m_time);
    m_seek = new QSlider(Qt::Horizontal, root);
    m_seek->setAccessibleName("Playback position");
    m_seek->setMinimumHeight(26);
    m_seek->setEnabled(false);
    m_seek->setStyleSheet(
        "QSlider::groove:horizontal { height: 5px; background: #32343b; border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: #a9adba; border-radius: 2px; }"
        "QSlider::handle:horizontal { width: 15px; margin: -5px 0; border-radius: 7px; background: white; }");
    header->addWidget(m_seek, 1);
    auto *searchToggle = iconButton(PlayerIcon::Search, "Open search");
    searchToggle->setObjectName("roundControl");
    searchToggle->setCheckable(true);
    header->addWidget(searchToggle);
    m_volumeButton = iconButton(PlayerIcon::Volume, "Volume");
    m_volumeButton->setObjectName("roundControl");
    auto *volumeMenu = new QMenu(m_volumeButton);
    auto *volumeAction = new QWidgetAction(volumeMenu);
    m_volume = new QSlider(Qt::Vertical, volumeMenu);
    m_volume->setRange(0, 100);
    m_volume->setFixedSize(36, 112);
    m_volume->setAccessibleName("Volume level");
    m_volume->setStyleSheet(
        "QSlider::groove:vertical { width: 4px; background: #393c48; border-radius: 2px; }"
        "QSlider::handle:vertical { height: 14px; margin: 0 -5px; border-radius: 7px; background: #ecedf5; }");
    volumeAction->setDefaultWidget(m_volume);
    volumeMenu->addAction(volumeAction);
    m_volumeButton->setMenu(volumeMenu);
    m_volumeButton->setPopupMode(QToolButton::InstantPopup);
    header->addWidget(m_volumeButton);
    m_shuffle = iconButton(PlayerIcon::Shuffle, "Shuffle off");
    m_shuffle->setIcon(playerIcon(PlayerIcon::Shuffle, false));
    m_shuffle->setCheckable(true);
    header->addWidget(m_shuffle);
    connect(m_shuffle, &QToolButton::clicked, this, [this] {
        command("/api/player/options", {{"shuffle", !m_player.shuffle}});
    });

    auto *searchPanel = new QWidget(root);
    auto *searchLayout = new QHBoxLayout(searchPanel);
    searchLayout->setContentsMargins(0, 0, 0, 0);
    auto *search = new QLineEdit(searchPanel);
    search->setPlaceholderText("Search albums and songs");
    search->setAccessibleName("Search library");
    search->setClearButtonEnabled(true);
    searchLayout->addWidget(search, 1);
    auto *keyboardButton = new QPushButton("⌨", searchPanel);
    keyboardButton->setAccessibleName("Open search keyboard");
    searchLayout->addWidget(keyboardButton);
    auto *closeSearch = iconButton(PlayerIcon::Close, "Close search");
    searchLayout->addWidget(closeSearch);
    layout->addWidget(searchPanel);
    searchPanel->hide();

    m_flow = new CoverFlowWidget(m_library, m_artwork, this);
    m_flow->setReducedMotion(reduced);
    layout->addWidget(m_flow, 1);
    auto *transport = new QHBoxLayout;
    transport->setSpacing(8);
    layout->addLayout(transport);
    auto *previous = iconButton(PlayerIcon::Previous, "Previous song", 36);
    connect(previous, &QToolButton::clicked, this, [this] { command("/api/player/previous"); });
    transport->addWidget(previous);
    m_play = iconButton(PlayerIcon::Play, "Play or pause", 46);
    m_play->setObjectName("mainPlay");
    m_play->setIconSize(QSize(27, 27));
    m_play->setProperty("playing", false);
    connect(m_play, &QToolButton::clicked, this, [this] {
        if (m_player.track.file.isEmpty()) showAlbum(m_flow->currentIndex(), true);
        else command(m_player.state == "play" ? "/api/player/pause" : "/api/player/play");
    });
    transport->addWidget(m_play);
    auto *next = iconButton(PlayerIcon::Next, "Next song", 36);
    connect(next, &QToolButton::clicked, this, [this] { command("/api/player/next"); });
    transport->addWidget(next);

    auto *albumRail = new QFrame(root);
    albumRail->setObjectName("albumRail");
    albumRail->setStyleSheet(
        "QFrame#albumRail { border: 1px solid #5c606b; border-radius: 10px; background:"
          "qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #333641,stop:0.48 #292b35,stop:1 #1c1e26); }"
        "QToolButton { border: 0; } QToolButton:hover { background: #424653; }");
    albumRail->setFixedHeight(25);
    auto *rail = new QHBoxLayout(albumRail);
    rail->setContentsMargins(5, 1, 5, 1);
    rail->setSpacing(2);
    auto *coverBack = iconButton(PlayerIcon::Left, "Previous album", 25);
    coverBack->setFixedHeight(21);
    coverBack->setIconSize(QSize(12, 12));
    auto *coverForward = iconButton(PlayerIcon::Right, "Next album", 25);
    coverForward->setFixedHeight(21);
    coverForward->setIconSize(QSize(12, 12));
    m_albumStrip = new AlbumStrip(albumRail);
    m_albumStrip->setRange(0, 0);
    m_albumStrip->setEnabled(false);
    m_albumStrip->setAccessibleName("Browse albums");
    m_albumStrip->setStyleSheet(
        "QSlider { background: transparent; }"
        "QSlider::groove:horizontal { height: 14px; background: transparent; border-left: 1px solid #676a75; border-right: 1px solid #676a75; }"
        "QSlider::handle:horizontal { width: 30px; margin: -1px 0; border: 1px solid #676c79; border-radius: 7px;"
          "background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #353b49,stop:1 #09090d); }");
    rail->addWidget(coverBack);
    rail->addWidget(m_albumStrip, 1);
    rail->addWidget(coverForward);
    transport->addWidget(albumRail, 1);
    connect(m_albumStrip, &QSlider::valueChanged, this, &MainWindow::selectAlbumOnStrip);
    connect(coverBack, &QToolButton::clicked, this, [this] { m_albumStrip->setValue(m_albumStrip->value() - 1); });
    connect(coverForward, &QToolButton::clicked, this, [this] { m_albumStrip->setValue(m_albumStrip->value() + 1); });
    auto *drawer = iconButton(PlayerIcon::Drawer, "Open songs drawer", 36);
    connect(drawer, &QToolButton::clicked, this, [this] {
        if (m_trackDrawer) closeTrackDrawer();
        else showAlbum(m_flow->currentIndex());
    });
    transport->addWidget(drawer);

    auto *navigation = new QHBoxLayout;
    navigation->setSpacing(2);
    layout->addLayout(navigation);
    auto *browseTabs = new QButtonGroup(this);
    const auto tab = [root, navigation](PlayerIcon icon, const QString &label) {
        auto *button = new QToolButton(root);
        button->setObjectName("browseTab");
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIcon(playerIcon(icon));
        button->setIconSize(QSize(20, 20));
        button->setText(label.toUpper());
        button->setAccessibleName(label);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setMinimumHeight(44);
        navigation->addWidget(button, 1);
        return button;
    };
    const auto browse = [this, tab, browseTabs](PlayerIcon icon, const QString &label, std::function<void()> action) {
        auto *button = tab(icon, label);
        button->setCheckable(true);
        browseTabs->addButton(button);
        connect(button, &QToolButton::clicked, this, std::move(action));
        return button;
    };
    browse(PlayerIcon::Albums, "Albums", [this] {
        closeTrackDrawer();
        if (!m_demo) m_library->reload();
    })->setChecked(true);
    browse(PlayerIcon::Songs, "Songs", [this] { showCollection("Songs", "/api/library/tracks?offset=0&limit=200", "tracks", {}); });
    browse(PlayerIcon::Artists, "Artists", [this] { showCollection("Artists", "/api/library/artists", "artists", "artist:"); });
    browse(PlayerIcon::Playlist, "Playlists", [this] { showCollection("Playlists", "/api/library/playlists", "playlists", "playlist:"); });
    auto *more = tab(PlayerIcon::More, "More");
    auto *menu = new QMenu(more);
    more->setMenu(menu);
    more->setPopupMode(QToolButton::InstantPopup);
    const auto menuAction = [this, menu](PlayerIcon icon, const QString &name, std::function<void()> action) {
        connect(menu->addAction(playerIcon(icon), name), &QAction::triggered, this, std::move(action));
    };
    menuAction(PlayerIcon::Favourite, "Favourite albums", [this] { if (!m_demo) m_library->reload("favourite"); });
    menuAction(PlayerIcon::Favourite, "Favourite songs", [this] { showCollection("Favourite songs", "/api/library/starred/tracks", "tracks", {}); });
    menuAction(PlayerIcon::Artists, "Composers", [this] { showCollection("Composers", "/api/library/composers", "composers", "composer:"); });
    menuAction(PlayerIcon::Songs, "Genres", [this] { showCollection("Genres", "/api/library/genres", "genres", "genre:"); });
    menuAction(PlayerIcon::Albums, "Years", [this] { showCollection("Years", "/api/library/years", "years", "year:"); });
    menuAction(PlayerIcon::Albums, "Highest rated albums", [this] { if (!m_demo) m_library->reload("toprated", "rating"); });
    menuAction(PlayerIcon::Volume, "Radio", [this] { showRadio(); });
    menu->addSeparator();
    menuAction(PlayerIcon::Playlist, "Sort by title", [this] { if (!m_demo) m_library->reload({}, "title"); });
    menuAction(PlayerIcon::Playlist, "Sort by artist", [this] { if (!m_demo) m_library->reload({}, "artist"); });
    menuAction(PlayerIcon::Playlist, "Sort by year", [this] { if (!m_demo) m_library->reload({}, "year-desc"); });
    auto *reducedAction = menu->addAction("Reduced animations");
    reducedAction->setCheckable(true);
    reducedAction->setChecked(reduced);
    connect(reducedAction, &QAction::toggled, this, [this](bool enabled) {
        m_flow->setReducedMotion(enabled);
        if (m_trackDrawer) m_trackDrawer->setReducedMotion(enabled);
        QSettings().setValue("native/reducedMotion", enabled);
    });
    menuAction(PlayerIcon::Stop, "Stop playback", [this] { command("/api/player/stop"); });
    connect(tab(PlayerIcon::Settings, "Settings"), &QToolButton::clicked, this, [this] {
        if (m_demo) { setMessage("Demo mode · connect an API for appliance settings"); return; }
        auto *dialog = new SettingsDialog(m_api, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->open();
    });
    m_message = new QLabel; m_message->setWordWrap(true); m_message->setAlignment(Qt::AlignCenter); layout->addWidget(m_message);
    auto *keyboard = new TouchKeyboard(this); layout->addWidget(keyboard); keyboard->watch(search, false);
    connect(keyboardButton, &QPushButton::clicked, this, [=] { search->setFocus(); keyboard->show(); });
    connect(closeSearch, &QToolButton::clicked, searchToggle, [searchToggle] { searchToggle->setChecked(false); });
    connect(searchToggle, &QToolButton::toggled, this, [=](bool shown) {
        searchPanel->setVisible(shown);
        if (shown) { search->setFocus(); search->selectAll(); }
        else { keyboard->hide(); search->clear(); m_flow->setFocus(); }
    });
    auto *dismissSearch = new QShortcut(QKeySequence(Qt::Key_Escape), searchPanel);
    dismissSearch->setContext(Qt::WidgetWithChildrenShortcut);
    connect(dismissSearch, &QShortcut::activated, closeSearch, &QToolButton::click);
    // Keep physical-keyboard search compact; the keyboard is explicitly opened above.
    connect(search, &QLineEdit::textChanged, this, [this] { m_searchDelay.start(220); });
    m_searchDelay.setSingleShot(true);
    connect(&m_searchDelay, &QTimer::timeout, this, [this, search] { if (!m_demo) m_library->reload({}, "year-desc", search->text().trimmed()); });
    connect(search, &QLineEdit::returnPressed, this, [this, search, keyboard] {
        keyboard->hide();
        if (!search->text().trimmed().isEmpty()) showCollection("Matching songs", "/api/search?limit=120&q="
            + QString::fromUtf8(QUrl::toPercentEncoding(search->text().trimmed())), "tracks", {});
    });
    connect(m_flow, &CoverFlowWidget::activated, this, [this](int row) { showAlbum(row); });
    connect(m_flow, &CoverFlowWidget::selectionChanged, this, [this](int row) {
        ++m_drawerGeneration;
        m_initialAlbumFocused = true;
        closeTrackDrawer();
        m_pendingAlbumRow = -1;
        syncAlbumStrip();
        if (!m_demo && row >= m_library->rowCount() - 24) m_library->loadNext();
    });
    connect(m_library, &LibraryModel::error, this, [this](const QString &message) {
        m_pendingAlbumRow = -1;
        syncAlbumStrip();
        setMessage(message);
    });
    connect(m_library, &QAbstractItemModel::modelReset, this, [this] {
        ++m_drawerGeneration;
        m_pendingAlbumRow = -1;
        closeTrackDrawer();
        syncAlbumStrip();
    });
    connect(m_library, &LibraryModel::pageLoaded, this, [this] {
        if (m_pendingAlbumRow >= 0) selectAlbumOnStrip(qMin(m_pendingAlbumRow, qMax(0, m_library->total() - 1)));
        else focusInitialPlaybackAlbum();
        syncAlbumStrip();
        setMessage(m_demo ? "Demo mode · controls do not play audio" : QString("Albums | %1").arg(m_library->total()));
    });
    connect(m_seek, &QSlider::sliderReleased, this, [this] { command("/api/player/seek", {{"seconds", m_seek->value()}}); });
    connect(m_volume, &QSlider::sliderReleased, this, [this] { command("/api/player/volume", {{"volume", m_volume->value()}}); });
    auto *fullscreen = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(fullscreen, &QShortcut::activated, this, [this] { isFullScreen() ? showNormal() : showFullScreen(); });
    m_poll.setInterval(1500); connect(&m_poll, &QTimer::timeout, this, &MainWindow::refreshPlayer);
    if (m_demo) m_library->setDemo();
    else { m_library->reload(); refreshPlayer(); m_poll.start(); }
    m_flow->setFocus();
}
void MainWindow::setMessage(const QString &message) { m_message->setText(message); }
void MainWindow::command(const QString &path, const QJsonObject &body) {
    if (m_demo) { setMessage("Demo mode · no audio or system changes"); return; }
    if (m_commands.size() >= 8) { setMessage("Waiting for the previous controls to finish…"); return; }
    ++m_playerGeneration;
    m_commands.enqueue({path, body});
    sendNextCommand();
}
void MainWindow::sendNextCommand() {
    if (m_commandPending || m_commands.isEmpty()) return;
    m_commandPending = true;
    const auto intent = m_commands.dequeue();
    m_api->post(intent.path, intent.body, [this](QJsonObject, QString error) {
        m_commandPending = false;
        if (!error.isEmpty()) setMessage(error);
        if (m_commands.isEmpty()) refreshPlayer(); else sendNextCommand();
    });
}
void MainWindow::refreshPlayer() {
    if (m_demo || m_pollPending || m_commandPending) return;
    m_pollPending = true;
    const auto generation = m_playerGeneration;
    m_api->get("/api/player/state", [this, generation](QJsonObject object, QString error) {
        m_pollPending = false;
        if (generation != m_playerGeneration || m_commandPending) return;
        if (!error.isEmpty()) { setMessage(error + " · retrying automatically"); return; }
        m_player = PlayerState::fromJson(object);
        const bool playing = m_player.state == "play";
        if (playing != m_play->property("playing").toBool()) {
            m_play->setProperty("playing", playing);
            m_play->setIcon(playerIcon(playing ? PlayerIcon::Pause : PlayerIcon::Play));
            m_play->setToolTip(playing ? "Pause" : "Play");
        }
        const auto repeat = "Repeat " + m_player.repeatMode;
        m_repeat->setIcon(playerIcon(m_player.repeatMode == "song" ? PlayerIcon::RepeatOne : PlayerIcon::Repeat,
                                     m_player.repeatMode != "off"));
        m_repeat->setAccessibleName(repeat);
        m_repeat->setToolTip(repeat);
        m_shuffle->setChecked(m_player.shuffle);
        m_shuffle->setIcon(playerIcon(PlayerIcon::Shuffle, m_player.shuffle));
        const auto shuffle = m_player.shuffle ? "Shuffle on" : "Shuffle off";
        m_shuffle->setAccessibleName(shuffle);
        m_shuffle->setToolTip(shuffle);
        const bool modes = m_player.source != "radio" && m_player.source != "airplay" && m_player.source != "bluetooth";
        m_repeat->setEnabled(modes); m_shuffle->setEnabled(modes);
        if (!m_seek->isSliderDown()) { m_seek->setRange(0, int(qMin(864000., m_player.duration))); m_seek->setValue(int(m_player.elapsed)); }
        m_seek->setEnabled(m_player.duration > 0);
        if (!m_volume->isSliderDown()) m_volume->setValue(qMax(0, m_player.volume));
        m_volumeButton->setEnabled(m_player.volume >= 0);
        m_volumeButton->setIcon(playerIcon(m_player.volume > 0 ? PlayerIcon::Volume : PlayerIcon::Mute));
        m_time->setText(timeLabel(m_player.elapsed) + " / " + (m_player.duration > 0 ? timeLabel(m_player.duration) : "--:--"));
        m_flow->setPlayback(m_player);
        focusInitialPlaybackAlbum();
        if (m_trackDrawer) m_trackDrawer->setPlayback(m_player);
        if (m_message->text().contains("retrying automatically")) setMessage("Connected to PiTunes");
    });
}
void MainWindow::syncAlbumStrip() {
    const QSignalBlocker blocker(m_albumStrip);
    const int count = qMax(m_library->rowCount(), m_library->total());
    m_albumStrip->setRange(0, qMax(0, count - 1));
    m_albumStrip->setEnabled(count > 1);
    if (!m_albumStrip->isSliderDown()) {
        m_albumStrip->setValue(m_pendingAlbumRow >= 0 ? m_pendingAlbumRow : m_flow->currentIndex());
    }
}
void MainWindow::selectAlbumOnStrip(int row) {
    m_initialAlbumFocused = true;
    if (row >= m_library->rowCount()) {
        if (row < m_library->total()) {
            m_pendingAlbumRow = row;
            m_library->loadNext();
        } else m_pendingAlbumRow = -1;
        return;
    }
    m_pendingAlbumRow = -1;
    m_flow->select(row);
}
void MainWindow::focusInitialPlaybackAlbum() {
    if (m_initialAlbumFocused || m_player.track.albumId.isEmpty() ||
        (m_player.state != "pause" && m_player.state != "play")) return;
    for (int row = 0; row < m_library->rowCount(); ++row) {
        if (m_library->album(row)->id != m_player.track.albumId) continue;
        m_initialAlbumFocused = true;
        m_flow->select(row, false);
        syncAlbumStrip();
        return;
    }
}
void MainWindow::showAlbum(int row, bool play) {
    m_initialAlbumFocused = true;
    const auto *selected = m_library->album(row); if (!selected) return;
    const Album album = *selected;
    if (m_demo) {
        showTracks(album.title, {{"demo/track.flac", "Demonstration track", album.artist, album.title, album.id, album.year, 210, "1"}}, album.id, album.year);
        return;
    }
    const auto generation = ++m_drawerGeneration;
    const auto decodedId = QUrl::fromPercentEncoding(album.id.toUtf8());
    m_api->get("/api/library/album/" + QString::fromUtf8(QUrl::toPercentEncoding(decodedId)) + "/tracks",
        [this, album, play, generation](QJsonObject o, QString error) {
            if (generation != m_drawerGeneration) return;
            if (!error.isEmpty()) { setMessage(error); return; }
            QVector<Track> tracks; for (const auto &v : o["tracks"].toArray()) tracks.append(Track::fromJson(v.toObject()));
            if (play && !tracks.isEmpty()) {
                QJsonArray queue; for (const auto &track : tracks) queue.append(track.file);
                command("/api/player/play", {{"file", tracks.first().file}, {"albumId", album.id}, {"queue", queue}, {"continuous", true}});
            } else showTracks(album.title, tracks, album.id, album.year);
        });
}
void MainWindow::closeTrackDrawer() {
    if (!m_trackDrawer) return;
    m_trackDrawer->close();
    m_trackDrawer = nullptr;
    m_flow->setCaptionClearance(0);
    m_flow->setFocus();
}
void MainWindow::showTracks(const QString &title, const QVector<Track> &tracks, const QString &albumId, const QString &year) {
    closeTrackDrawer();
    auto *drawer = new TrackDrawer(m_flow, title, year, tracks, !albumId.isEmpty());
    m_trackDrawer = drawer;
    drawer->setPlayback(m_player);
    drawer->setReducedMotion(m_flow->reducedMotion());
    connect(drawer, &TrackDrawer::dismissRequested, this, &MainWindow::closeTrackDrawer);
    connect(drawer, &TrackDrawer::playRequested, this, [this, tracks, albumId](int row) {
        if (row < 0 || row >= tracks.size()) return;
        QJsonArray queue;
        for (const auto &track : tracks) queue.append(track.file);
        command("/api/player/play", {{"file", tracks[row].file}, {"albumId", albumId},
                                     {"queue", queue}, {"continuous", !albumId.isEmpty()}});
    });
    connect(drawer, &TrackDrawer::trackFavouriteRequested, this, [this, tracks](int row, bool starred) {
        if (row < 0 || row >= tracks.size()) return;
        command("/api/library/favourites", {{"trackId", tracks[row].file}, {"starred", starred}});
    });
    connect(drawer, &TrackDrawer::albumFavouriteRequested, this, [this, albumId] {
        command("/api/library/favourites", {{"albumId", albumId}, {"starred", true}});
    });
    connect(drawer, &TrackDrawer::savePlaylistRequested, this, [this, drawer, tracks] {
        bool accepted;
        const auto name = QInputDialog::getText(drawer, "New playlist", "Name", QLineEdit::Normal, {}, &accepted).trimmed();
        if (!accepted || name.isEmpty()) return;
        QJsonArray ids;
        for (const auto &track : tracks) ids.append(track.file);
        command("/api/library/playlists", {{"name", name}, {"trackIds", ids}});
    });
    drawer->open();
}
void MainWindow::showCollection(QString title, QString path, QString key, QString filterPrefix) {
    if (m_demo) { setMessage("Connect a backend to browse " + title); return; }
    const auto generation = ++m_drawerGeneration;
    m_api->get(path, [this, title, key, filterPrefix, generation](QJsonObject o, QString error) {
        if (generation != m_drawerGeneration) return;
        if (!error.isEmpty()) { setMessage(error); return; }
        const auto rows = o[key].toArray();
        if (key == "tracks") {
            QVector<Track> tracks;
            for (const auto &v : rows) tracks.append(Track::fromJson(v.toObject()));
            if (o["playlist"].isObject()) {
                QHash<QString, Track> byFile; for (const auto &track : tracks) byFile.insert(track.file, track);
                tracks.clear();
                for (const auto &id : o["playlist"].toObject()["trackIds"].toArray())
                    if (byFile.contains(textValue(id))) tracks.append(byFile.value(textValue(id)));
            }
            showTracks(title, tracks); return;
        }
        auto *dialog = new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setWindowTitle(title); dialog->resize(500, 500);
        auto *layout = new QVBoxLayout(dialog); auto *list = new QListView(dialog); auto *model = new QStandardItemModel(list);
        list->setModel(model); list->setUniformItemSizes(true); layout->addWidget(list);
        for (const auto &v : rows) {
            const auto row = v.toObject(); const auto name = row.contains("year") ? textValue(row["year"]) : row.isEmpty() ? textValue(v) : textValue(row["name"]);
            auto *item = new QStandardItem(name); item->setEditable(false); item->setSizeHint(QSize(200, 40));
            item->setData(filterPrefix == "playlist:" ? textValue(row["id"]) : name, Qt::UserRole); model->appendRow(item);
        }
        connect(list, &QListView::activated, dialog, [this, model, filterPrefix, dialog](const QModelIndex &i) {
            const auto value = model->item(i.row())->data(Qt::UserRole).toString(); dialog->accept();
            if (filterPrefix == "playlist:") showCollection(model->item(i.row())->text(), "/api/library/playlists/" + QString::fromUtf8(QUrl::toPercentEncoding(value)) + "/tracks", "tracks", {});
            else m_library->reload(filterPrefix + value);
        });
        auto *close = new QDialogButtonBox(QDialogButtonBox::Close, dialog); layout->addWidget(close);
        connect(close, &QDialogButtonBox::rejected, dialog, &QDialog::reject); dialog->open();
    }, 30000);
}
void MainWindow::showRadio() {
    bool ok; const auto search = QInputDialog::getText(this, "Internet radio", "Search stations (empty for saved stations)", QLineEdit::Normal, {}, &ok);
    if (!ok || m_demo) return;
    const auto path = search.trimmed().isEmpty() ? QString("/api/library/radio") : "/api/library/radio/search?limit=40&q=" + QString::fromUtf8(QUrl::toPercentEncoding(search));
    m_api->get(path, [this](QJsonObject o, QString error) {
        if (!error.isEmpty()) { setMessage(error); return; }
        const auto stations = o["stations"].toArray(); QStringList names;
        for (const auto &v : stations) names.append(textValue(v.toObject()["name"]));
        bool ok; const auto name = QInputDialog::getItem(this, "Radio stations", "Station", names, 0, false, &ok);
        const int i = names.indexOf(name); if (!ok || i < 0) return;
        const auto station = stations[i].toObject();
        command("/api/player/radio/play", {{"url", textValue(station.contains("streamUrl") ? station["streamUrl"] : station["url"])}, {"name", name}});
    }, 45000);
}
}
