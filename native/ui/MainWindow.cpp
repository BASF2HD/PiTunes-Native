#include "MainWindow.h"
#include "SettingsDialog.h"
#include "PerformanceMonitor.h"
#include "TouchKeyboard.h"
#include "TrackDrawer.h"
#include "PlayerIcons.h"
#include "FrostedSurface.h"
#include "MediaInfoPanel.h"
#include "RadioDialog.h"
#include <QActionGroup>
#include <QApplication>
#include <QComboBox>
#include <QStyleOption>
#include <algorithm>
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
#include <QPainter>
#include <QStandardItemModel>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QSettings>
#include <QShortcut>
#include <QStyleOptionSlider>

namespace PiTunes {
namespace {
constexpr int transportHeight = 35;
constexpr int browseHeight = 40;
constexpr int footerHeight = transportHeight + browseHeight;
constexpr int headerControlSize = 24;

QString albumTracksPath(const QString &id) {
    const auto decoded = QUrl::fromPercentEncoding(id.toUtf8());
    return "/api/library/album/" + QString::fromUtf8(QUrl::toPercentEncoding(decoded)) + "/tracks";
}

QJsonObject albumBrowseContext(const LibraryModel &library) {
    QJsonObject context{{"mode", "album"}, {"albumBrowseSort", library.sortMode()}};
    const auto filter = library.filter();
    for (const auto &entry : {qMakePair(QString("artist"), QString("selectedArtist")),
                              qMakePair(QString("composer"), QString("selectedComposer")),
                              qMakePair(QString("year"), QString("selectedYear")),
                              qMakePair(QString("genre"), QString("selectedGenre"))}) {
        if (!filter.startsWith(entry.first + ':')) continue;
        context["mode"] = entry.first;
        context[entry.second] = filter.mid(entry.first.size() + 1);
        return context;
    }
    if (filter == "favourite") context["albumBrowseScope"] = filter;
    else if (filter == "toprated") context["mode"] = "rating";
    return context;
}

class BrowseButton : public QToolButton {
public:
    using QToolButton::QToolButton;
protected:
    void paintEvent(QPaintEvent *event) override {
        QToolButton::paintEvent(event);
        if (!property("activeBrowse").toBool()) return;
        QPainter painter(this);
        const int inset = width() / 10;
        painter.fillRect(inset, height() - 2, width() - 2 * inset, 2, QColor("#e0e3ee"));
    }
};

class PositionSlider : public QSlider {
public:
    explicit PositionSlider(QWidget *parent) : QSlider(Qt::Horizontal, parent) {}
    std::function<void()> grooveClicked;

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
                if (grooveClicked) grooveClicked();
                return;
            }
        }
        QSlider::mousePressEvent(event);
    }
};

class AlbumStrip : public PositionSlider {
public:
    using PositionSlider::PositionSlider;

protected:
    void resizeEvent(QResizeEvent *event) override {
        QSlider::resizeEvent(event);
        const int width = qBound(36, this->width() * 3 / 20, 180);
        if (width == m_thumbWidth) return;
        m_thumbWidth = width;
        setStyleSheet(QString(
            "QSlider { background: transparent; }"
            "QSlider::groove:horizontal { height: 12px; background: transparent; "
            "border-left: 1px solid rgba(210,218,235,52); border-right: 1px solid rgba(210,218,235,52); }"
            "QSlider::handle:horizontal { width: %1px; margin: 0; border: 1px solid rgba(225,232,245,145); "
            "border-radius: 6px; background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "stop:0 #5b5f65,stop:0.23 #222429,stop:0.56 #07080a,stop:1 #181a1f); }").arg(width));
    }

private:
    int m_thumbWidth = 0;
};
}

MainWindow::MainWindow(QUrl api, bool demo, bool reduced, bool lightweight) : m_demo(demo) {
    m_forceReducedMotion = reduced;
    setWindowTitle("PiTunes Native"); setWindowIcon(QIcon(":/brand/icon.png")); resize(1100, 720);
    m_api = new ApiClient(std::move(api), this);
    m_library = new LibraryModel(m_api, this);
    QSettings preferences;
    const int defaultMode = preferences.value("native/reducedMotion", false).toBool()
        ? int(AnimationMode::Off) : int(lightweight ? AnimationMode::Lightweight : AnimationMode::Full);
    const auto mode = reduced ? AnimationMode::Off
        : AnimationMode(qBound(0, preferences.value("native/animationMode", defaultMode).toInt(), 2));
    m_artwork = new ArtworkCache(m_api, mode == AnimationMode::Full ? 12 : 6, this);
    m_flow = new CoverFlowWidget(m_library, m_artwork, this);
    m_flow->setAnimationMode(mode);
    m_monitor = new PerformanceMonitor(m_flow, this);
    m_flow->installEventFilter(this);
    const CaptionFontSizes defaults;
    m_flow->setCaptionFontSizes({
        preferences.value("native/captionTitleSize", defaults.title).toInt(),
        preferences.value("native/captionArtistSize", defaults.artist).toInt(),
        preferences.value("native/captionAlbumSize", defaults.album).toInt()
    });
    auto *root = new QWidget(this); auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(20, 9, 20, 10); layout->setSpacing(2); setCentralWidget(root);
    root->setStyleSheet(
        "QToolButton { background: transparent; border: 1px solid transparent; color: #f4f5fa; padding: 0; }"
        "QToolButton:hover, QToolButton:focus { color: white; background: #1a1d25; border: 1px solid #414651; border-radius: 5px; }"
        "QToolButton::menu-indicator { width: 0; }"
        "QToolButton#roundControl { border: 1px solid #383b45; border-radius: 16px; }"
        "QToolButton#headerCircle { border: 1px solid #383b45; border-radius: 12px; }"
        "QToolButton#headerIcon:hover, QToolButton#headerIcon:focus { border-radius: 12px; }"
        "QToolButton#captionHeart, QToolButton#captionHeart:hover, QToolButton#captionHeart:focus {"
        "border: 1px solid #383b45; border-radius: 14px; }"
        "QToolButton#trackStep:hover, QToolButton#trackStep:focus { border-radius: 16px;"
          "border: 1px solid rgba(225,232,245,48); background: rgba(225,232,245,34); }"
        "QToolButton#mainPlay { border: 1px solid #686b78; border-radius: 17px;"
          "background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #373942,stop:1 #20222a); }"
        "QToolButton#mainPlay:hover, QToolButton#mainPlay:focus { border-color: #b5bac8; background: #343846; }"
        "QToolButton#browseTab { border-radius: 0; border: 1px solid transparent; font-size: 10px; font-weight: 600; }"
        "QToolButton#browseTab:hover, QToolButton#browseTab:focus { background: transparent; border-color: transparent; }");
    const auto iconButton = [root](PlayerIcon icon, const QString &label, int side = 32) {
        auto *button = new QToolButton(root);
        button->setFixedSize(side, side);
        button->setIconSize(QSize(20, 20));
        button->setIcon(playerIcon(icon));
        button->setAccessibleName(label);
        button->setToolTip(label);
        return button;
    };
    const auto headerButton = [iconButton](PlayerIcon icon, const QString &label, bool outlined = false) {
        auto *button = iconButton(icon, label, headerControlSize);
        button->setIconSize(QSize(15, 15));
        button->setObjectName(outlined ? "headerCircle" : "headerIcon");
        return button;
    };
    auto *header = new QHBoxLayout;
    auto *leftHeader = new QHBoxLayout;
    auto *rightHeader = new QHBoxLayout;
    header->setSpacing(6);
    leftHeader->setSpacing(6);
    rightHeader->setSpacing(6);
    layout->addLayout(header);
    leftHeader->addStretch(1);
    header->addLayout(leftHeader, 1);
    m_repeat = headerButton(PlayerIcon::Repeat, "Repeat off");
    m_repeat->setIcon(playerIcon(PlayerIcon::Repeat, false));
    leftHeader->addWidget(m_repeat);
    connect(m_repeat, &QToolButton::clicked, this, [this] {
        const auto mode = m_player.repeatMode == "all" ? "song" : m_player.repeatMode == "song" ? "off" : "all";
        command("/api/player/options", {{"repeatMode", mode}});
    });
    m_time = new QLabel("0:00 / --:--", root);
    m_time->setMinimumWidth(72);
    m_time->setStyleSheet("font-size: 12px;");
    m_time->setAccessibleName("Playback time");
    leftHeader->addWidget(m_time);
    auto *seek = new PositionSlider(root);
    m_seek = seek;
    m_seek->setAccessibleName("Playback position");
    m_seek->setFixedHeight(headerControlSize);
    m_seek->setFixedWidth(120);
    m_seek->setEnabled(false);
    m_seek->setStyleSheet(
        "QSlider::groove:horizontal { height: 4px; background: #32343b; border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: #a9adba; border-radius: 2px; }"
        "QSlider::handle:horizontal { width: 12px; margin: -4px 0; border-radius: 6px; background: white; }");
    header->addWidget(m_seek);
    header->addLayout(rightHeader, 1);
    seek->grooveClicked = [this] { command("/api/player/seek", {{"seconds", m_seek->value()}}); };
    auto *searchToggle = headerButton(PlayerIcon::Search, "Open search", true);
    m_searchToggle = searchToggle;
    searchToggle->setCheckable(true);
    rightHeader->addWidget(searchToggle);
    m_volumeButton = headerButton(PlayerIcon::Volume, "Volume", true);
    auto *volumeMenu = new FrostedMenu(m_flow, m_volumeButton);
    auto *volumeAction = new QWidgetAction(volumeMenu);
    auto *volumePanel = new QWidget(volumeMenu);
    volumePanel->setObjectName("volumePanel");
    volumePanel->setStyleSheet("QWidget#volumePanel { background: transparent; }");
    auto *volumeLayout = new QVBoxLayout(volumePanel);
    volumeLayout->setContentsMargins(4, 4, 4, 5);
    volumeLayout->setSpacing(3);
    m_volume = new QSlider(Qt::Vertical, volumePanel);
    m_volume->setRange(0, 100);
    m_volume->setFixedSize(36, 112);
    m_volume->setAccessibleName("Volume level");
    m_volume->setStyleSheet(
        "QSlider { background: transparent; }"
        "QSlider::groove:vertical { width: 4px; background: #393c48; border-radius: 2px; }"
        "QSlider::handle:vertical { height: 14px; margin: 0 -5px; border-radius: 7px; background: #ecedf5; }");
    volumeLayout->addWidget(m_volume, 0, Qt::AlignHCenter);
    m_mute = iconButton(PlayerIcon::Volume, "Mute");
    m_mute->setObjectName("roundControl");
    volumeLayout->addWidget(m_mute, 0, Qt::AlignHCenter);
    connect(m_mute, &QToolButton::clicked, this, [this] {
        command("/api/player/volume", {{"volume", m_player.volume > 0 ? 0 : m_volumeBeforeMute}});
    });
    volumeAction->setDefaultWidget(volumePanel);
    volumeMenu->addAction(volumeAction);
    m_volumeButton->setMenu(volumeMenu);
    m_volumeButton->setPopupMode(QToolButton::InstantPopup);
    rightHeader->addWidget(m_volumeButton);
    m_shuffle = headerButton(PlayerIcon::Shuffle, "Shuffle off");
    m_shuffle->setIcon(playerIcon(PlayerIcon::Shuffle, false));
    m_shuffle->setCheckable(true);
    rightHeader->addWidget(m_shuffle);
    rightHeader->addStretch(1);
    connect(m_shuffle, &QToolButton::clicked, this, [this] {
        command("/api/player/options", {{"shuffle", !m_player.shuffle}});
    });

    layout->addWidget(m_flow, 1);

    m_searchPanel = new FrostedPanel(m_flow, QColor(14, 16, 26, 182), 24, 6);
    m_searchPanel->setObjectName("librarySearchPanel");
    m_searchPanel->setAccessibleName("Library search");
    m_searchPanel->setStyleSheet(
        "QFrame#librarySearchPanel { background: transparent; border: 0; }");
    auto *searchLayout = new QVBoxLayout(m_searchPanel);
    searchLayout->setContentsMargins(8, 8, 8, 8);
    searchLayout->setSpacing(5);
    auto *searchBar = new QHBoxLayout;
    searchLayout->addLayout(searchBar);
    auto *search = new QLineEdit(m_searchPanel);
    search->setPlaceholderText("Search albums and songs");
    search->setAccessibleName("Search library");
    search->setClearButtonEnabled(true);
    searchBar->addWidget(search, 1);
    auto *keyboardButton = new QPushButton("⌨", m_searchPanel);
    keyboardButton->setAccessibleName("Open search keyboard");
    searchBar->addWidget(keyboardButton);
    auto *closeSearch = iconButton(PlayerIcon::Close, "Close search");
    searchBar->addWidget(closeSearch);
    auto *keyboard = new TouchKeyboard(m_searchPanel);
    searchLayout->addWidget(keyboard);
    keyboard->watch(search, false);
    m_searchPanel->hide();

    m_captionActions = iconButton(PlayerIcon::More, "Album or song information actions");
    m_captionActions->setParent(m_flow);
    m_captionActions->setObjectName("roundControl");
    m_captionActions->hide();
    connect(m_captionActions, &QToolButton::clicked, this, &MainWindow::showCaptionActions);
    m_captionHeart = iconButton(PlayerIcon::Favourite, "Favourite", 30);
    m_captionHeart->setParent(m_flow);
    m_captionHeart->setObjectName("captionHeart");
    m_captionHeart->hide();
    connect(m_captionHeart, &QToolButton::clicked, this, [this] {
        const auto [id, album] = captionFavouriteSubject();
        toggleFavourite(id, album);
    });
    connect(m_flow, &CoverFlowWidget::captionChanged, this, &MainWindow::syncCaptionHeart);

    m_transportPanel = new QFrame(m_flow);
    m_transportPanel->setObjectName("transportPanel");
    m_transportPanel->setStyleSheet("QFrame#transportPanel { background: transparent; }");
    auto *footer = new QVBoxLayout(m_transportPanel);
    footer->setContentsMargins(0, 0, 0, 0);
    footer->setSpacing(0);
    const auto centredRow = [footer, this](int maxWidth, int spacing) {
        auto *outer = new QHBoxLayout;
        outer->setSpacing(0);
        auto *row = new QWidget(m_transportPanel);
        row->setObjectName("footerRow");
        row->setStyleSheet("QWidget#footerRow { background: transparent; }");
        row->setMaximumWidth(maxWidth);
        auto *inner = new QHBoxLayout(row);
        inner->setContentsMargins(0, 0, 0, 0);
        inner->setSpacing(spacing);
        outer->addStretch(1);
        outer->addWidget(row, 12);
        outer->addStretch(1);
        footer->addLayout(outer);
        return inner;
    };
    auto *transport = centredRow(920, 7);
    m_flow->setTransportInset(footerHeight);
    auto *previous = iconButton(PlayerIcon::Previous, "Previous song", 32);
    previous->setObjectName("trackStep");
    connect(previous, &QToolButton::clicked, this, [this] { command("/api/player/previous"); });
    transport->addWidget(previous);
    m_play = iconButton(PlayerIcon::Play, "Play or pause", 34);
    m_play->setObjectName("mainPlay");
    m_play->setIconSize(QSize(22, 22));
    m_play->setProperty("playing", false);
    connect(m_play, &QToolButton::clicked, this, [this] {
        if (m_player.track.file.isEmpty()) showAlbum(m_flow->currentIndex(), true);
        else command(m_player.state == "play" ? "/api/player/pause" : "/api/player/play");
    });
    transport->addWidget(m_play);
    auto *next = iconButton(PlayerIcon::Next, "Next song", 32);
    next->setObjectName("trackStep");
    connect(next, &QToolButton::clicked, this, [this] { command("/api/player/next"); });
    transport->addWidget(next);

    auto *albumRail = new QFrame(m_transportPanel);
    albumRail->setObjectName("albumRail");
    albumRail->setStyleSheet(
        "QFrame#albumRail { border: 1px solid rgba(225, 232, 245, 68); border-radius: 8px; background:"
        "qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 rgba(255,255,255,44),"
        "stop:0.48 rgba(255,255,255,13),stop:1 rgba(255,255,255,28)); }"
        "QToolButton { border: 0; } QToolButton:hover { background: rgba(230,235,248,60); }");
    albumRail->setFixedHeight(16);
    auto *rail = new QHBoxLayout(albumRail);
    rail->setContentsMargins(5, 1, 5, 1);
    rail->setSpacing(2);
    auto *coverBack = iconButton(PlayerIcon::Left, "Previous album", 25);
    coverBack->setFixedHeight(12);
    coverBack->setIconSize(QSize(12, 12));
    auto *coverForward = iconButton(PlayerIcon::Right, "Next album", 25);
    coverForward->setFixedHeight(12);
    coverForward->setIconSize(QSize(12, 12));
    m_albumStrip = new AlbumStrip(albumRail);
    m_albumStrip->setRange(0, 0);
    m_albumStrip->setEnabled(false);
    m_albumStrip->setAccessibleName("Browse albums");
    rail->addWidget(coverBack);
    rail->addWidget(m_albumStrip, 1);
    rail->addWidget(coverForward);
    transport->addWidget(albumRail, 1);
    connect(m_albumStrip, &QSlider::valueChanged, this, &MainWindow::selectAlbumOnStrip);
    connect(coverBack, &QToolButton::clicked, this, [this] { m_albumStrip->setValue(m_albumStrip->value() - 1); });
    connect(coverForward, &QToolButton::clicked, this, [this] { m_albumStrip->setValue(m_albumStrip->value() + 1); });
    auto *drawer = iconButton(PlayerIcon::Drawer, "Open songs drawer", 32);
    connect(drawer, &QToolButton::clicked, this, [this] {
        if (m_trackDrawer) closeTrackDrawer();
        else showAlbum(m_flow->currentIndex());
    });
    transport->addWidget(drawer);

    auto *navigation = centredRow(520, 6);
    buildBrowseMenus(navigation, m_transportPanel);
    m_fullView = iconButton(PlayerIcon::Fullscreen, "Toggle full view", 27);
    m_fullView->setParent(m_transportPanel);
    connect(m_fullView, &QToolButton::clicked, this, [this] {
        isFullScreen() ? showNormal() : showFullScreen();
    });
    m_message = new QLabel(root);
    m_message->setWordWrap(true);
    m_message->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_message);
    m_message->hide();
    connect(m_flow, &CoverFlowWidget::captionLayoutChanged, this, &MainWindow::placeOverlays);
    m_searchPanel->installEventFilter(this);
    m_snapBack.setSingleShot(true);
    m_snapBack.setInterval(5000);
    m_snapBack.setTimerType(Qt::PreciseTimer);
    connect(&m_snapBack, &QTimer::timeout, this, [this] {
        const int row = returnAlbumRow();
        if (row >= 0) m_flow->select(row);
    });
    connect(m_flow, &CoverFlowWidget::navigationSettled, this, &MainWindow::scheduleReturnToPlayback);
    connect(m_albumStrip, &QSlider::sliderPressed, &m_snapBack, &QTimer::stop);
    connect(m_albumStrip, &QSlider::sliderReleased, this, &MainWindow::scheduleReturnToPlayback);
    watchBrowsePopup(volumeMenu);
    connect(keyboardButton, &QPushButton::clicked, this, [=] { search->setFocus(); keyboard->show(); });
    connect(closeSearch, &QToolButton::clicked, searchToggle, [searchToggle] { searchToggle->setChecked(false); });
    connect(searchToggle, &QToolButton::toggled, this, [=](bool shown) {
        m_searchPanel->setVisible(shown);
        if (shown) {
            closeMediaInfo();
            m_snapBack.stop();
            placeOverlays();
            m_searchPanel->raise();
            search->setFocus();
        } else {
            ++m_drawerGeneration;
            m_searchDelay.stop();
            keyboard->hide();
            const QSignalBlocker blocker(search);
            search->clear();
            if (m_searchResults) {
                m_searchResults = false;
                m_library->search({});
            }
            m_flow->setFocus();
            scheduleReturnToPlayback();
        }
    });
    auto *dismissSearch = new QShortcut(QKeySequence(Qt::Key_Escape), m_searchPanel);
    dismissSearch->setContext(Qt::WidgetWithChildrenShortcut);
    connect(dismissSearch, &QShortcut::activated, closeSearch, &QToolButton::click);
    connect(search, &QLineEdit::textChanged, this, [this] { m_searchDelay.start(220); });
    m_searchDelay.setSingleShot(true);
    connect(&m_searchDelay, &QTimer::timeout, this, [this, search] {
        if (m_demo) return;
        const auto query = search->text().trimmed();
        m_searchResults = !query.isEmpty();
        m_library->search(query);
    });
    connect(search, &QLineEdit::returnPressed, this, [this, search, keyboard] {
        keyboard->hide();
        m_searchDelay.stop();
        const auto query = search->text().trimmed();
        m_searchResults = !query.isEmpty();
        if (!m_demo) m_library->search(query);
    });
    connect(m_flow, &CoverFlowWidget::activated, this, [this](int row) { showAlbum(row); });
    connect(m_flow, &CoverFlowWidget::selectionChanged, this, [this](int row) {
        ++m_drawerGeneration;
        m_initialAlbumFocused = true;
        closeTrackDrawer();
        m_pendingAlbumRow = -1;
        syncAlbumStrip();
        if (!m_demo && row >= m_library->rowCount() - 24) m_library->loadNext();
        scheduleReturnToPlayback();
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
        m_captionActions->hide();
        m_captionHeart->hide();
        syncAlbumStrip();
    });
    connect(m_library, &LibraryModel::pageLoaded, this, [this] {
        if (m_pendingAlbumRow >= 0) selectAlbumOnStrip(qMin(m_pendingAlbumRow, qMax(0, m_library->total() - 1)));
        else focusInitialPlaybackAlbum();
        syncAlbumStrip();
        m_captionActions->setVisible(m_library->rowCount() > 0);
        syncCaptionHeart();
        if (!m_favouritesReady) refreshFavourites();
        placeOverlays();
        setMessage({});
        if (!m_snapBack.isActive()) scheduleReturnToPlayback();
    });
    connect(m_seek, &QSlider::sliderReleased, this, [this] { command("/api/player/seek", {{"seconds", m_seek->value()}}); });
    connect(m_volume, &QSlider::sliderReleased, this, [this] { command("/api/player/volume", {{"volume", m_volume->value()}}); });
    auto *fullscreen = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(fullscreen, &QShortcut::activated, this, [this] { isFullScreen() ? showNormal() : showFullScreen(); });
    auto *monitorShortcut = new QShortcut(QKeySequence(Qt::Key_F12), this);
    monitorShortcut->setContext(Qt::ApplicationShortcut);
    connect(monitorShortcut, &QShortcut::activated, this, [this] {
        m_monitor->setMode(m_monitor->mode() == MonitorMode::Off ? MonitorMode::Compact : MonitorMode::Off);
    });
    m_poll.setInterval(1500); connect(&m_poll, &QTimer::timeout, this, &MainWindow::refreshPlayer);
    if (m_demo) m_library->setDemo();
    else { m_library->reload(); refreshPlayer(); m_poll.start(); }
    m_flow->setFocus();
}
void MainWindow::setMessage(const QString &message) {
    m_message->setText(message);
    m_message->setVisible(!message.isEmpty());
}
void MainWindow::buildBrowseMenus(QHBoxLayout *navigation, QWidget *root) {
    const auto heading = [](QMenu *menu, const QString &title, bool main = false) {
        auto *header = new QWidgetAction(menu);
        auto *label = new QLabel(title, menu);
        label->setTextFormat(Qt::PlainText);
        label->setStyleSheet(main
            ? "background: transparent; color: #f2f4fb; font-size: 14px; font-weight: 600; padding: 7px 13px;"
            : "background: transparent; color: #a5adbf; font-size: 11px; font-weight: 600; padding: 5px 13px 2px;");
        header->setDefaultWidget(label);
        menu->addAction(header);
    };
    const auto tab = [this, root, navigation, heading](PlayerIcon icon, const QString &name) {
        auto *button = new BrowseButton(root);
        button->setObjectName("browseTab");
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIcon(playerIcon(icon));
        button->setIconSize(QSize(20, 20));
        button->setText(name.toUpper());
        button->setAccessibleName(name);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setFixedHeight(browseHeight);
        navigation->addWidget(button, 1);
        auto *menu = new FrostedMenu(m_flow, button);
        menu->setTitle(name);
        menu->setAccessibleName(name + " menu");
        heading(menu, name, true);
        button->setProperty("activeBrowse", name == m_activeBrowse);
        m_browseMenus.insert(button, menu);
        menu->installEventFilter(this);
        connect(menu, &QMenu::aboutToHide, this, [this] { m_pressedBrowseTab.clear(); });
        connect(button, &QToolButton::clicked, this, [this, button] { openBrowseMenu(button); });
        watchBrowsePopup(menu);
        return menu;
    };
    const auto options = [this](QMenu *menu, const QVector<QPair<QString, QString>> &items,
                                const QString &selected, std::function<void(QString)> apply) {
        auto *group = new QActionGroup(menu);
        for (const auto &item : items) {
            auto *action = menu->addAction(item.first);
            group->addAction(action);
            action->setCheckable(true);
            action->setChecked(item.second == selected);
            connect(action, &QAction::triggered, this, [apply, value = item.second] { apply(value); });
        }
    };
    const QVector<QPair<QString, QString>> sorts = {
        {"Year (newest first)", "year-desc"}, {"Year (oldest first)", "year-asc"}, {"Title (A–Z)", "title"}
    };
    const auto action = [this](QMenu *menu, PlayerIcon icon, const QString &label, std::function<void()> apply) {
        connect(menu->addAction(playerIcon(icon), label), &QAction::triggered, this, std::move(apply));
    };

    auto *albums = tab(PlayerIcon::Albums, "Albums");
    options(albums, {{"All albums", ""}, {"Favourite", "favourite"}}, m_albumScope, [this](const QString &scope) {
        m_albumScope = scope;
        browseAlbums(scope, m_albumSort);
    });
    heading(albums, "Sort by");
    auto albumSorts = sorts;
    albumSorts.append(QPair<QString, QString>{"Artist (A–Z)", "artist"});
    options(albums, albumSorts, m_albumSort, [this](const QString &sort) {
        m_albumSort = sort;
        browseAlbums(m_albumScope, sort);
    });
    auto *count = albums->addAction(QString());
    count->setEnabled(false);
    connect(albums, &QMenu::aboutToShow, this, [this, count] {
        const bool countMatches = !m_searchResults && m_library->filter() == m_albumScope;
        count->setVisible(countMatches);
        if (countMatches) {
            const int albums = m_library->total();
            count->setText(albums == 1 ? "1 album" : QString("%1 albums").arg(albums));
        }
    });

    auto *songs = tab(PlayerIcon::Songs, "Songs");
    options(songs, {{"All songs", "all"}, {"Group by album", "album"}, {"Favourite", "favourite"}},
            m_songScope, [this](const QString &scope) {
        m_songScope = scope;
        browseSongs();
    });
    heading(songs, "Sort by");
    auto songSorts = sorts;
    songSorts.prepend({"Library order", ""});
    options(songs, songSorts, m_songSort, [this](const QString &sort) {
        m_songSort = sort;
        browseSongs();
    });

    auto *artists = tab(PlayerIcon::Artists, "Artists");
    action(artists, PlayerIcon::Artists, "Browse artists", [this] {
        showCollection("Artists", "/api/library/artists", "artists", "artist:", m_artistSort);
    });
    action(artists, PlayerIcon::Songs, "Browse composers", [this] {
        showCollection("Composers", "/api/library/composers", "composers", "composer:", m_artistSort);
    });
    heading(artists, "Sort albums by");
    options(artists, sorts, m_artistSort, [this](const QString &sort) {
        m_artistSort = sort;
        if (m_activeBrowse == "Artists") browseArtistShelf();
    });

    auto *playlists = tab(PlayerIcon::Playlist, "Playlists");
    action(playlists, PlayerIcon::Playlist, "Browse playlists", [this] {
        showCollection("Playlists", "/api/library/playlists", "playlists", "playlist:", m_playlistSort);
    });
    heading(playlists, "Sort songs by");
    auto playlistSorts = sorts;
    playlistSorts.prepend({"Playlist order", ""});
    options(playlists, playlistSorts, m_playlistSort, [this](const QString &sort) {
        m_playlistSort = sort;
        browsePlaylistShelf();
    });

    auto *more = tab(PlayerIcon::More, "More");
    action(more, PlayerIcon::Volume, "Radio", [this] { browseRadio(); });
    action(more, PlayerIcon::Search, "Find internet radio…", [this] { showRadio(); });
    action(more, PlayerIcon::Albums, "Years", [this] { showCollection("Years", "/api/library/years", "years", "year:", m_albumSort); });
    action(more, PlayerIcon::Songs, "Genres", [this] { showCollection("Genres", "/api/library/genres", "genres", "genre:", m_albumSort); });
    action(more, PlayerIcon::Albums, "Highest rated albums", [this] {
        selectBrowseTab("More");
        browseAlbums("toprated", "rating");
    });

    auto *settings = tab(PlayerIcon::Settings, "Settings");
    action(settings, PlayerIcon::Settings, "Application settings", [this] {
        auto *dialog = new SettingsDialog(m_demo ? nullptr : m_api, m_flow->captionFontSizes(),
                                          this, m_flow->animationMode(), m_monitor, m_forceReducedMotion);
        connect(dialog, &SettingsDialog::animationModeChanged, this, &MainWindow::applyAnimationMode);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, &SettingsDialog::captionFontSizesChanged, this, [this](CaptionFontSizes sizes) {
            m_flow->setCaptionFontSizes(sizes);
            QSettings preferences;
            preferences.setValue("native/captionTitleSize", sizes.title);
            preferences.setValue("native/captionArtistSize", sizes.artist);
            preferences.setValue("native/captionAlbumSize", sizes.album);
        });
        connect(dialog, &QDialog::finished, this, [this] { QTimer::singleShot(0, this, &MainWindow::scheduleReturnToPlayback); });
        dialog->open();
    });
}

void MainWindow::applyAnimationMode(AnimationMode mode) {
    if (m_forceReducedMotion) return;
    m_artwork->setBudgetMiB(mode == AnimationMode::Full ? 12 : 6);
    m_flow->setAnimationMode(mode);
    if (m_trackDrawer) m_trackDrawer->setReducedMotion(mode == AnimationMode::Off);
    QSettings().setValue("native/animationMode", int(mode));
}

void MainWindow::openBrowseMenu(QToolButton *button) {
    auto *menu = m_browseMenus.value(button);
    if (!menu) return;
    const auto name = button->accessibleName();
    if (name != m_activeBrowse) {
        if (name == "Albums") { selectBrowseTab(name); browseAlbums(m_albumScope, m_albumSort); }
        else if (name == "Songs") { selectBrowseTab(name); browseSongs(); }
        else if (name == "Artists") { selectBrowseTab(name); browseArtistShelf(); }
        else if (name == "Playlists") { selectBrowseTab(name); browsePlaylistShelf(); }
    }
    auto *activeMenu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
    if (activeMenu && m_browseMenus.key(activeMenu)) activeMenu->hide();
    if (activeMenu == menu) return;
    const auto size = menu->sizeHint();
    menu->popup(button->mapToGlobal(QPoint((button->width() - size.width()) / 2, -size.height() - 5)));
}

void MainWindow::selectBrowseTab(const QString &name) {
    if (m_searchToggle->isChecked()) {
        m_searchResults = false;
        m_searchToggle->setChecked(false);
    }
    m_activeBrowse = name;
    for (auto *button : m_browseMenus.keys()) {
        button->setProperty("activeBrowse", button->accessibleName() == name);
        button->update();
    }
}
void MainWindow::browseAlbums(const QString &filter, const QString &sort) {
    closeTrackDrawer();
    if (!m_demo) m_library->reload(filter, sort);
}
void MainWindow::browseSongs() {
    closeTrackDrawer();
    if (m_demo) return;
    if (m_songScope == "album") {
        browseAlbums({}, m_songSort.isEmpty() ? m_albumSort : m_songSort);
    } else {
        m_library->reloadTracks(m_songScope == "favourite" ? "/api/library/starred/tracks" : "/api/library/tracks", m_songSort);
    }
}
void MainWindow::browseArtistShelf() {
    if (m_demo) return;
    if (!m_artistFilter.isEmpty()) {
        browseAlbums(m_artistFilter, m_artistSort);
        return;
    }
    closeTrackDrawer();
    const auto generation = ++m_drawerGeneration;
    m_api->get("/api/library/artists", [this, generation](QJsonObject result, QString error) {
        if (generation != m_drawerGeneration || m_activeBrowse != "Artists") return;
        if (!error.isEmpty()) { setMessage(error); return; }
        const auto artists = result["artists"].toArray();
        if (artists.isEmpty()) { browseAlbums({}, "artist"); return; }
        const auto first = artists.first();
        const auto name = first.isObject() ? textValue(first.toObject()["name"]) : textValue(first);
        if (!name.isEmpty()) m_artistFilter = "artist:" + name;
        browseAlbums(m_artistFilter, m_artistSort);
    });

}
void MainWindow::browsePlaylistShelf() {
    if (m_demo) return;
    closeTrackDrawer();
    if (!m_currentPlaylistId.isEmpty()) {
        m_library->reloadTracks("/api/library/playlists/" +
            QString::fromUtf8(QUrl::toPercentEncoding(m_currentPlaylistId)) + "/tracks", m_playlistSort);
        return;
    }
    const auto generation = ++m_drawerGeneration;
    m_api->get("/api/library/playlists", [this, generation](QJsonObject o, QString error) {
        if (m_activeBrowse != "Playlists" || generation != m_drawerGeneration) return;
        if (!error.isEmpty()) { setMessage(error); return; }
        const auto rows = o["playlists"].toArray();
        if (rows.isEmpty()) { m_library->reloadTracks("/api/library/playlists", {}); return; }
        const auto playlist = rows.first().toObject();
        m_currentPlaylistId = textValue(playlist["id"]);
        if (!m_currentPlaylistId.isEmpty()) browsePlaylistShelf();
    });
}
void MainWindow::browseRadio() {
    if (m_demo) return;
    selectBrowseTab("More");
    closeTrackDrawer();
    m_library->reloadRadio();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_flow && m_trackDrawer && event->type() == QEvent::Wheel) {
        event->accept();
        return true;
    }
    if (watched == m_searchPanel && event->type() == QEvent::LayoutRequest && m_searchPanel->isVisible()) {
        placeOverlays();
    }
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease) {
        auto *menu = qobject_cast<QMenu *>(watched);
        if (menu && m_browseMenus.key(menu)) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton && !menu->rect().contains(menu->mapFromGlobal(mouse->globalPos()))) {
                for (auto *button : m_browseMenus.keys()) {
                    if (!button->rect().contains(button->mapFromGlobal(mouse->globalPos()))) continue;
                    if (event->type() == QEvent::MouseButtonPress) m_pressedBrowseTab = button;
                    else if (m_pressedBrowseTab == button) {
                        menu->setAttribute(Qt::WA_NoMouseReplay);
                        openBrowseMenu(button);
                    }
                    return true;
                }
            }
            m_pressedBrowseTab.clear();
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::placeOverlays() {
    // The side groups have equal stretch and enough room for their controls.
    const int controlRoom = qMax(3 * headerControlSize + 12, headerControlSize + m_time->sizeHint().width() + 6) + 8;
    const int seekWidth = qMax(1, qMin(m_flow->centreCoverRect().width(), m_flow->width() - 2 * controlRoom));
    if (m_seek->width() != seekWidth) m_seek->setFixedWidth(seekWidth);
    m_transportPanel->setGeometry(0, m_flow->height() - footerHeight, m_flow->width(), footerHeight);
    m_fullView->move(m_transportPanel->width() - m_fullView->width() - 3,
                     m_transportPanel->height() - m_fullView->height() - 3);
    const auto caption = m_flow->captionRect();
    m_captionActions->move(caption.right() + 4, caption.top());
    m_captionHeart->move(caption.right() + 5, caption.top() + m_captionActions->height() + 2);
    if (m_searchPanel->isVisible()) {
        m_searchPanel->setFixedWidth(qMin(575, m_flow->width() - 16));
        m_searchPanel->adjustSize();
        m_searchPanel->move((m_flow->width() - m_searchPanel->width()) / 2, 10);
        m_searchPanel->raise();
    }
    if (m_infoPanel) {
        const auto cover = m_flow->centreCoverRect();
        const int width = qMin(m_flow->width() - 12, qMax(356, cover.width()));
        const int height = qMin(m_flow->height() - footerHeight - cover.top() - 6, qMax(180, cover.height()));
        m_infoPanel->setGeometry((m_flow->width() - width) / 2, cover.top(), width, height);
        m_infoPanel->raise();
    }
}

void MainWindow::watchBrowsePopup(QMenu *menu) {
    connect(menu, &QMenu::aboutToShow, &m_snapBack, &QTimer::stop);
    connect(menu, &QMenu::aboutToHide, this, [this] { QTimer::singleShot(0, this, &MainWindow::scheduleReturnToPlayback); });
}

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
        const auto previousState = m_player.state;
        const auto previousAlbum = m_player.track.albumId;
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
        const bool muted = m_player.volume == 0;
        if (m_player.volume > 0) m_volumeBeforeMute = m_player.volume;
        const auto volumeIcon = playerIcon(m_player.volume > 0 ? PlayerIcon::Volume : PlayerIcon::Mute);
        m_volumeButton->setIcon(volumeIcon);
        m_mute->setIcon(volumeIcon);
        m_mute->setAccessibleName(muted ? "Unmute" : "Mute");
        m_mute->setToolTip(m_mute->accessibleName());
        m_time->setText(timeLabel(m_player.elapsed) + " / " + (m_player.duration > 0 ? timeLabel(m_player.duration) : "--:--"));
        m_flow->setPlayback(m_player);
        focusInitialPlaybackAlbum();
        if (m_trackDrawer) m_trackDrawer->setPlayback(m_player);
        if (previousState != m_player.state || previousAlbum != m_player.track.albumId) scheduleReturnToPlayback();
        if (m_message->text().contains("retrying automatically")) setMessage({});
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
    m_flow->select(row, !m_albumStrip->isSliderDown());
}
int MainWindow::playingAlbumRow() const {
    for (int row = 0; row < m_library->rowCount(); ++row) {
        if (m_library->entry(row)->matchesPlayback(m_player)) return row;
    }
    return -1;
}
void MainWindow::focusInitialPlaybackAlbum() {
    if (m_initialAlbumFocused || (m_player.state != "pause" && m_player.state != "play")) return;
    const int row = playingAlbumRow();
    if (row >= 0) {
        m_initialAlbumFocused = true;
        m_flow->select(row, false);
        syncAlbumStrip();
    }
}
int MainWindow::returnAlbumRow() const {
    if (m_player.state != "play" || m_player.track.file.isEmpty() || m_flow->animating() ||
        m_trackDrawer || m_infoPanel || m_searchPanel->isVisible() || m_albumStrip->isSliderDown() ||
        m_pendingAlbumRow >= 0 || (m_pendingInspection && m_pendingInspection == m_drawerGeneration) ||
        QApplication::activePopupWidget() || QApplication::activeModalWidget()) return -1;
    const int row = playingAlbumRow();
    return row == m_flow->currentIndex() ? -1 : row;
}
void MainWindow::scheduleReturnToPlayback() {
    if (returnAlbumRow() < 0) m_snapBack.stop();
    else m_snapBack.start();
}
void MainWindow::showAlbum(int row, bool play) {
    m_initialAlbumFocused = true;
    const auto *selected = m_library->entry(row);
    if (!selected) return;
    m_snapBack.stop();
    if (selected->kind == CoverEntry::StationCover) {
        command("/api/player/radio/play", {{"stationId", selected->face.id},
                                            {"url", radioStreamUrl(selected->station)}, {"name", selected->face.title}});
        return;
    }
    if (selected->kind == CoverEntry::TrackCover) {
        const auto track = selected->track;
        if (play) {
            QJsonArray queue;
            for (int i = 0; i < m_library->rowCount(); ++i) {
                const auto *entry = m_library->entry(i);
                if (entry->kind == CoverEntry::TrackCover) queue.append(entry->track.file);
            }
            command("/api/player/play", {{"file", track.file}, {"albumId", track.albumId},
                                         {"queue", queue}, {"continuous", false}});
        } else if (track.albumId.isEmpty()) {
            showTracks(track.title, {track});
        } else {
            showAlbumTracks({track.albumId, track.album, track.albumArtist, track.year, selected->face.artUrl, track.genre}, false);
        }
        return;
    }
    showAlbumTracks(selected->face, play);
}

void MainWindow::showAlbumTracks(Album album, bool play) {
    if (m_demo) {
        showTracks(album.title, {{"demo/track.flac", "Demonstration track", album.artist, album.title, album.id, album.year, 210, "1"}}, album.id, album.year);
        return;
    }
    const auto generation = ++m_drawerGeneration;
    if (!play) m_pendingInspection = generation;
    m_api->get(albumTracksPath(album.id),
        [this, album, play, generation](QJsonObject o, QString error) {
            if (generation != m_drawerGeneration) return;
            m_pendingInspection = 0;
            if (!error.isEmpty()) { setMessage(error); scheduleReturnToPlayback(); return; }
            QVector<Track> tracks;
            for (const auto &v : o["tracks"].toArray()) {
                auto track = Track::fromJson(v.toObject());
                track.albumId = album.id;
                if (track.albumArtist.isEmpty()) track.albumArtist = album.artist;
                if (track.genre.isEmpty()) track.genre = album.genre;
                tracks.append(std::move(track));
            }
            if (play && !tracks.isEmpty()) {
                QJsonArray queue; for (const auto &track : tracks) queue.append(track.file);
                command("/api/player/play", {{"file", tracks.first().file}, {"albumId", album.id}, {"queue", queue}, {"continuous", true}, {"browseContext", albumBrowseContext(*m_library)}});
                scheduleReturnToPlayback();
            } else showTracks(album.title, tracks, album.id, album.year);
        });
}
void MainWindow::closeTrackDrawer() {
    closeMediaInfo();
    if (!m_trackDrawer) return;
    m_trackDrawer->close();
    m_trackDrawer = nullptr;
    m_drawerAlbumId.clear();
    m_flow->setCaptionClearance(0);
    m_flow->setFocus();
    placeOverlays();
    scheduleReturnToPlayback();
}
void MainWindow::showTracks(const QString &title, const QVector<Track> &tracks, const QString &albumId, const QString &year) {
    closeTrackDrawer();
    auto *drawer = new TrackDrawer(m_flow, title, year, tracks, !albumId.isEmpty());
    m_trackDrawer = drawer;
    m_drawerAlbumId = albumId;
    m_snapBack.stop();
    drawer->setPlayback(m_player);
    drawer->setReducedMotion(m_flow->reducedMotion());
    connect(drawer, &TrackDrawer::dismissRequested, this, &MainWindow::closeTrackDrawer);
    connect(drawer, &TrackDrawer::playRequested, this, [this, tracks, albumId](int row) {
        if (row < 0 || row >= tracks.size()) return;
        QJsonArray queue;
        for (const auto &track : tracks) queue.append(track.file);
        command("/api/player/play", {{"file", tracks[row].file}, {"albumId", albumId},
                                     {"queue", queue}, {"continuous", !albumId.isEmpty()}, {"browseContext", albumBrowseContext(*m_library)}});
    });
    connect(drawer, &TrackDrawer::trackActionsRequested, this, [this, tracks](int row, const QPoint &position) {
        if (row < 0 || row >= tracks.size()) return;
        showSubjectActions(position, {}, tracks[row], tracks, row);
    });
    connect(drawer, &TrackDrawer::albumFavouriteRequested, this, [this, albumId] {
        toggleFavourite(albumId, true);
    });
    connect(drawer, &TrackDrawer::trackFavouriteRequested, this, [this, tracks](int row) {
        if (row >= 0 && row < tracks.size()) toggleFavourite(tracks[row].file, false);
    });
    drawer->open();
    placeOverlays();
    refreshFavourites();
    updateFavouriteHearts();
}

namespace {
QString favouriteKey(const QString &id, bool album) { return (album ? "album:" : "track:") + id; }

QJsonArray trackIds(const QVector<Track> &tracks) {
    QJsonArray ids;
    for (const auto &track : tracks) if (!track.file.isEmpty()) ids.append(track.file);
    return ids;
}
}

void MainWindow::refreshFavourites() {
    if (m_demo || m_favouritesLoading || !m_pendingFavourites.isEmpty()) return;
    m_favouritesLoading = true;
    const auto revision = m_favouritesRevision;
    m_api->get("/api/library/favourites", [this, revision](QJsonObject o, QString error) {
        m_favouritesLoading = false;
        if (!error.isEmpty() || revision != m_favouritesRevision) return;
        m_albumFavourites.clear();
        m_trackFavourites.clear();
        for (const auto &value : o["albums"].toArray()) m_albumFavourites.insert(textValue(value));
        for (const auto &value : o["tracks"].toArray()) m_trackFavourites.insert(textValue(value));
        m_favouritesReady = true;
        updateFavouriteHearts();
    });
}

void MainWindow::updateFavouriteHearts() {
    if (m_trackDrawer) {
        const bool ready = m_favouritesReady || m_demo;
        m_trackDrawer->setAlbumFavourite(m_albumFavourites.contains(m_drawerAlbumId), ready);
        m_trackDrawer->setTrackFavourites(m_trackFavourites, ready);
    }
    syncCaptionHeart();
}

QPair<QString, bool> MainWindow::captionFavouriteSubject() const {
    const auto *entry = m_library->entry(m_flow->captionAlbumIndex());
    if (!entry || entry->kind == CoverEntry::StationCover) return {};
    if (entry->kind == CoverEntry::TrackCover) return {entry->track.file, false};
    if (m_flow->captionShowsPlayback() && !m_player.track.file.isEmpty()) return {m_player.track.file, false};
    return {entry->face.id, true};
}

void MainWindow::syncCaptionHeart() {
    const auto [id, album] = captionFavouriteSubject();
    const bool starred = (album ? m_albumFavourites : m_trackFavourites).contains(id);
    const QString label = (starred ? "Remove favourite " : "Favourite ") + QString(album ? "album" : "song");
    m_captionHeart->setIcon(playerIcon(starred ? PlayerIcon::FavouriteFilled : PlayerIcon::Favourite));
    m_captionHeart->setAccessibleName(label);
    m_captionHeart->setToolTip(label);
    m_captionHeart->setEnabled(m_favouritesReady || m_demo);
    m_captionHeart->setVisible(!id.isEmpty() && m_captionActions->isVisibleTo(m_flow));
}

void MainWindow::toggleFavourite(const QString &id, bool album) {
    const auto key = favouriteKey(id, album);
    if (id.isEmpty() || (!m_favouritesReady && !m_demo) || m_pendingFavourites.contains(key)) {
        updateFavouriteHearts();
        return;
    }
    auto &favourites = album ? m_albumFavourites : m_trackFavourites;
    const bool starred = !favourites.contains(id);
    if (starred) favourites.insert(id);
    else favourites.remove(id);
    updateFavouriteHearts();
    if (m_demo) return;
    ++m_favouritesRevision;
    m_pendingFavourites.insert(key);
    m_api->post("/api/library/favourites", {{album ? "albumId" : "trackId", id}, {"starred", starred}},
        [this, key, id, album, starred](QJsonObject, QString error) {
            m_pendingFavourites.remove(key);
            if (error.isEmpty()) return;
            auto &favourites = album ? m_albumFavourites : m_trackFavourites;
            if (starred) favourites.remove(id);
            else favourites.insert(id);
            updateFavouriteHearts();
            setMessage(error);
        });
}

void MainWindow::showCaptionActions() {
    const auto *entry = m_library->entry(m_flow->captionAlbumIndex());
    if (!entry) return;
    const auto anchor = m_captionActions->mapToGlobal(QPoint(m_captionActions->width(), 0));
    if (entry->kind == CoverEntry::StationCover) {
        auto *menu = new FrostedMenu(m_flow, this);
        const auto row = m_flow->captionAlbumIndex();
        menu->addAction(playerIcon(PlayerIcon::Play), "Play station", this, [this, row] { showAlbum(row, true); });
        menu->addAction(playerIcon(PlayerIcon::Search), "Find internet radio…", this, &MainWindow::showRadio);
        connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
        watchBrowsePopup(menu);
        menu->popup(anchor - QPoint(menu->sizeHint().width(), menu->sizeHint().height()));
        return;
    }
    if (entry->kind == CoverEntry::TrackCover) { showSubjectActions(anchor, {}, entry->track); return; }
    const auto *album = &entry->face;
    const bool playingHere = m_flow->captionShowsPlayback() && !m_player.track.file.isEmpty();
    if (!playingHere) { showSubjectActions(anchor, *album, {}); return; }
    auto track = m_player.track;
    if (track.albumArtist.isEmpty()) track.albumArtist = album->artist;
    if (track.genre.isEmpty()) track.genre = album->genre;
    if (track.year.isEmpty()) track.year = album->year;
    showSubjectActions(anchor, {}, track);
}

void MainWindow::showSubjectActions(const QPoint &anchor, const Album &album, const Track &track,
                                    const QVector<Track> &list, int row) {
    const bool albumContext = !album.id.isEmpty();
    const auto id = albumContext ? album.id : track.file;
    auto *menu = new FrostedMenu(m_flow, this);
    menu->setAccessibleName(albumContext ? "Album actions" : "Song actions");
    auto *add = menu->addAction(albumContext ? "Add album to playlist…" : "Add to playlist…", this,
        [this, album, albumContext, track] {
            if (m_demo) return;
            if (!albumContext) { choosePlaylist({track.file}); return; }
            const auto generation = ++m_drawerGeneration;
            m_api->get(albumTracksPath(album.id),
                [this, generation](QJsonObject o, QString error) {
                    if (generation != m_drawerGeneration) return;
                    if (!error.isEmpty()) { setMessage(error); return; }
                    QVector<Track> tracks;
                    for (const auto &row : o["tracks"].toArray()) tracks.append(Track::fromJson(row.toObject()));
                    choosePlaylist(trackIds(tracks));
                });
        });
    add->setEnabled(!id.isEmpty() && !m_demo);
    menu->addAction("More info", this, [this, album, track, row] { showMediaInfo(album, track, row); });
    if (!list.isEmpty()) {
        menu->addSeparator();
        menu->addAction("Save list as playlist…", this, [this, list] { createPlaylist(trackIds(list)); });
    }
    watchBrowsePopup(menu);
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    menu->popup(anchor - QPoint(menu->sizeHint().width(), menu->sizeHint().height()));
}

void MainWindow::closeMediaInfo() {
    if (!m_infoPanel) return;
    m_infoPanel->close();
    m_infoPanel = nullptr;
    if (m_trackDrawer) m_trackDrawer->findChild<QListView *>()->setFocus();
    else m_flow->setFocus();
    scheduleReturnToPlayback();
}

void MainWindow::showMediaInfo(const Album &album, const Track &selected, int row) {
    closeMediaInfo();
    m_snapBack.stop();
    const bool isAlbum = !album.id.isEmpty();
    auto track = selected;
    if (track.bitRate <= 0. && track.file == m_player.track.file && m_player.state != "stop")
        track.bitRate = m_player.track.bitRate;
    auto *panel = new MediaInfoPanel(m_flow, isAlbum ? "Album Details" : "Track Details",
                                     isAlbum ? albumWithYear(album.title, album.year) : "More Info");
    m_infoPanel = panel;
    connect(panel, &MediaInfoPanel::dismissRequested, this, &MainWindow::closeMediaInfo);
    if (isAlbum) panel->setAlbum(album);
    else panel->setTrack(track, row);
    placeOverlays();
    panel->show();
    panel->raise();
    panel->setFocus();

    const auto albumId = isAlbum ? album.id : track.albumId;
    if (albumId.isEmpty() || m_demo) return;
    const auto generation = m_drawerGeneration;
    m_api->get(albumTracksPath(albumId),
        [this, guard = QPointer<MediaInfoPanel>(panel), generation, isAlbum, album, track, row](QJsonObject o, QString error) {
            if (!guard || guard != m_infoPanel || generation != m_drawerGeneration || !error.isEmpty()) return;
            QVector<Track> tracks;
            for (const auto &v : o["tracks"].toArray()) tracks.append(Track::fromJson(v.toObject()));
            if (isAlbum) {
                auto enriched = album;
                if (enriched.genre.isEmpty() && !tracks.isEmpty()) enriched.genre = tracks.front().genre;
                guard->setAlbum(enriched, &tracks);
                return;
            }
            for (auto &detail : tracks) {
                if (detail.file != track.file) continue;
                for (auto field : {&Track::title, &Track::artist, &Track::albumArtist, &Track::singer, &Track::composer,
                                   &Track::album, &Track::genre, &Track::year, &Track::codec, &Track::trackNumber}) {
                    if ((detail.*field).isEmpty()) detail.*field = track.*field;
                }
                if (detail.duration <= 0.) detail.duration = track.duration;
                if (detail.bitRate <= 0.) detail.bitRate = track.bitRate;
                guard->setTrack(detail, row);
                break;
            }
        });
}

void MainWindow::choosePlaylist(const QJsonArray &ids) {
    if (ids.isEmpty()) return;
    const auto generation = ++m_drawerGeneration;
    m_api->get("/api/library/playlists", [this, ids, generation](QJsonObject o, QString error) {
        if (generation != m_drawerGeneration) return;
        if (!error.isEmpty()) { setMessage(error); return; }
        const auto playlists = o["playlists"].toArray();
        QStringList labels{"New playlist…"};
        for (const auto &entry : playlists) labels.append(textValue(entry.toObject()["name"]));
        QInputDialog dialog(this);
        dialog.setWindowTitle("Add to playlist");
        dialog.setLabelText("Playlist");
        dialog.setComboBoxItems(labels);
        dialog.setComboBoxEditable(false);
        const bool accepted = dialog.exec() == QDialog::Accepted;
        const auto *combo = dialog.findChild<QComboBox *>();
        const int row = combo ? combo->currentIndex() : -1;
        scheduleReturnToPlayback();
        if (!accepted) return;
        if (row == 0) createPlaylist(ids);
        else if (row > 0) appendToPlaylist(textValue(playlists[row - 1].toObject()["id"]), ids, 0);
    });
}
void MainWindow::createPlaylist(const QJsonArray &ids) {
    bool accepted = false;
    const auto name = QInputDialog::getText(this, "New playlist", "Name", QLineEdit::Normal, {}, &accepted).trimmed();
    scheduleReturnToPlayback();
    if (accepted && !name.isEmpty()) command("/api/library/playlists", {{"name", name}, {"trackIds", ids}});
}
void MainWindow::appendToPlaylist(const QString &id, const QJsonArray &tracks, int row) {
    if (id.isEmpty() || row >= tracks.size()) return;
    m_api->post("/api/library/playlists/tracks", {{"playlistId", id}, {"trackId", tracks[row]}},
        [this, id, tracks, row](QJsonObject, QString error) {
            if (!error.isEmpty()) { setMessage(error); return; }
            appendToPlaylist(id, tracks, row + 1);
        });
}

void MainWindow::showCollection(QString title, QString path, QString key, QString filterPrefix, QString sort) {
    if (m_demo) { setMessage("Connect a backend to browse " + title); return; }
    const auto generation = ++m_drawerGeneration;
    m_pendingInspection = generation;
    m_snapBack.stop();
    m_api->get(path, [this, title, key, filterPrefix, sort, generation](QJsonObject o, QString error) {
        if (generation != m_drawerGeneration) return;
        m_pendingInspection = 0;
        if (!error.isEmpty()) { setMessage(error); scheduleReturnToPlayback(); return; }
        const auto rows = o[key].toArray();
        auto *dialog = new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setWindowTitle(title); dialog->resize(500, 500);
        auto *layout = new QVBoxLayout(dialog); auto *list = new QListView(dialog); auto *model = new QStandardItemModel(list);
        list->setModel(model); list->setUniformItemSizes(true); layout->addWidget(list);
        for (const auto &v : rows) {
            const auto row = v.toObject(); const auto name = row.contains("year") ? textValue(row["year"]) : row.isEmpty() ? textValue(v) : textValue(row["name"]);
            auto *item = new QStandardItem(name); item->setEditable(false); item->setSizeHint(QSize(200, 40));
            item->setData(filterPrefix == "playlist:" ? textValue(row["id"]) : name, Qt::UserRole); model->appendRow(item);
        }
        connect(dialog, &QDialog::finished, this, [this] { QTimer::singleShot(0, this, &MainWindow::scheduleReturnToPlayback); });
        connect(list, &QListView::activated, dialog, [this, model, filterPrefix, sort, dialog](const QModelIndex &i) {
            const auto value = model->item(i.row())->data(Qt::UserRole).toString(); dialog->accept();
            if (filterPrefix == "playlist:") {
                m_currentPlaylistId = value;
                selectBrowseTab("Playlists");
                browsePlaylistShelf();
            } else {
                const auto filter = filterPrefix + value;
                if (filterPrefix == "artist:" || filterPrefix == "composer:") {
                    selectBrowseTab("Artists");
                    m_artistFilter = filter;
                } else selectBrowseTab("More");
                browseAlbums(filter, sort);
            }
        });
        auto *close = new QDialogButtonBox(QDialogButtonBox::Close, dialog); layout->addWidget(close);
        connect(close, &QDialogButtonBox::rejected, dialog, &QDialog::reject); dialog->open();
    }, 30000);
}
void MainWindow::showRadio() {
    if (m_demo) { setMessage("Demo mode · radio needs a PiTunes server"); return; }
    auto *dialog = new RadioDialog(m_api, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &RadioDialog::playRequested, this, [this](const QJsonObject &station) {
        command("/api/player/radio/play", station);
    });
    connect(dialog, &QDialog::finished, this, [this] {
        if (m_library->isRadioShelf()) browseRadio();
        QTimer::singleShot(0, this, &MainWindow::scheduleReturnToPlayback);
    });
    dialog->open();
}
}
