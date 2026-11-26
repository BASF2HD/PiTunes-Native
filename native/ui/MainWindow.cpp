#include "MainWindow.h"
#include "SettingsDialog.h"
#include "TouchKeyboard.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QListView>
#include <QMenu>
#include <QPointer>
#include <QStandardItemModel>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QSettings>
#include <QShortcut>

namespace PiTunes {
MainWindow::MainWindow(QUrl api, bool demo, bool reduced) : m_demo(demo) {
    setWindowTitle("PiTunes Native"); setWindowIcon(QIcon(":/brand/icon.png")); resize(1100, 720);
    m_api = new ApiClient(std::move(api), this);
    m_library = new LibraryModel(m_api, this);
    reduced = reduced || QSettings().value("native/reducedMotion", false).toBool();
    m_artwork = new ArtworkCache(m_api, reduced ? 6 : 12, this);
    auto *root = new QWidget(this); auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(20, 12, 20, 10); layout->setSpacing(4); setCentralWidget(root);
    auto *header = new QHBoxLayout; layout->addLayout(header);
    auto *logo = new QLabel; logo->setPixmap(QPixmap(":/brand/logo.png").scaled(145, 42, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    header->addWidget(logo); m_elapsed = new QLabel("0:00"); m_elapsed->setMinimumWidth(42); header->addWidget(m_elapsed);
    m_seek = new QSlider(Qt::Horizontal); m_seek->setAccessibleName("Playback position"); header->addWidget(m_seek, 1);
    m_duration = new QLabel("0:00"); m_duration->setMinimumWidth(42); header->addWidget(m_duration);
    auto *search = new QLineEdit; search->setPlaceholderText("Search library"); search->setMaximumWidth(220);
    search->setClearButtonEnabled(true); header->addWidget(search);
    auto *keyboardButton = new QPushButton("⌨"); keyboardButton->setAccessibleName("Open search keyboard"); header->addWidget(keyboardButton);
    m_flow = new CoverFlowWidget(m_library, m_artwork, this); m_flow->setReducedMotion(reduced); layout->addWidget(m_flow, 1);
    m_now = new QLabel; m_now->setAlignment(Qt::AlignCenter); m_now->setWordWrap(true); layout->addWidget(m_now);
    auto *transport = new QHBoxLayout; layout->addLayout(transport);
    auto add = [this](QHBoxLayout *row, const QString &text, std::function<void()> action) {
        auto *button = new QPushButton(text); button->setMinimumHeight(40); row->addWidget(button);
        connect(button, &QPushButton::clicked, this, std::move(action)); return button;
    };
    transport->addStretch();
    m_repeat = add(transport, "Repeat Off", [this] {
        command("/api/player/options", {{"repeatMode", m_player.repeatMode == "off" ? "all" : m_player.repeatMode == "all" ? "song" : "off"}});
    });
    add(transport, "⏮", [this] { command("/api/player/previous"); });
    m_play = add(transport, "▶", [this] {
        if (m_player.track.file.isEmpty()) showAlbum(m_flow->currentIndex(), true);
        else command(m_player.state == "play" ? "/api/player/pause" : "/api/player/play");
    });
    m_play->setAccessibleName("Play or pause");
    add(transport, "⏭", [this] { command("/api/player/next"); });
    add(transport, "Stop", [this] { command("/api/player/stop"); });
    m_shuffle = add(transport, "Shuffle", [this] { command("/api/player/options", {{"shuffle", !m_player.shuffle}}); });
    transport->addStretch();
    auto *vol = new QLabel("Volume"); transport->addWidget(vol);
    m_volume = new QSlider(Qt::Horizontal); m_volume->setRange(0, 100); m_volume->setMaximumWidth(100);
    m_volume->setAccessibleName("Volume"); transport->addWidget(m_volume);
    auto *navigation = new QHBoxLayout; navigation->addStretch(); layout->addLayout(navigation);
    add(navigation, "Albums", [this] { if (!m_demo) m_library->reload(); });
    add(navigation, "Songs", [this] { showCollection("Songs", "/api/library/tracks?offset=0&limit=200", "tracks", {}); });
    add(navigation, "Artists", [this] { showCollection("Artists", "/api/library/artists", "artists", "artist:"); });
    add(navigation, "Playlists", [this] { showCollection("Playlists", "/api/library/playlists", "playlists", "playlist:"); });
    auto *more = new QPushButton("More"); more->setMinimumHeight(40); navigation->addWidget(more);
    auto *menu = new QMenu(more); more->setMenu(menu);
    const auto menuAction = [this, menu](const QString &name, std::function<void()> action) {
        connect(menu->addAction(name), &QAction::triggered, this, std::move(action));
    };
    menuAction("Favourite albums", [this] { if (!m_demo) m_library->reload("favourite"); });
    menuAction("Favourite songs", [this] { showCollection("Favourite songs", "/api/library/starred/tracks", "tracks", {}); });
    menuAction("Composers", [this] { showCollection("Composers", "/api/library/composers", "composers", "composer:"); });
    menuAction("Genres", [this] { showCollection("Genres", "/api/library/genres", "genres", "genre:"); });
    menuAction("Years", [this] { showCollection("Years", "/api/library/years", "years", "year:"); });
    menuAction("Highest rated albums", [this] { if (!m_demo) m_library->reload("toprated", "rating"); });
    menuAction("Radio", [this] { showRadio(); });
    menuAction("Sort by title", [this] { if (!m_demo) m_library->reload({}, "title"); });
    menuAction("Sort by artist", [this] { if (!m_demo) m_library->reload({}, "artist"); });
    menuAction("Sort by year", [this] { if (!m_demo) m_library->reload({}, "year-desc"); });
    auto *reducedAction = menu->addAction("Reduced animations"); reducedAction->setCheckable(true); reducedAction->setChecked(reduced);
    connect(reducedAction, &QAction::toggled, this, [this](bool enabled) {
        m_flow->setReducedMotion(enabled); QSettings().setValue("native/reducedMotion", enabled);
    });
    add(navigation, "Settings", [this] {
        if (m_demo) { setMessage("Demo mode · connect an API for appliance settings"); return; }
        auto *dialog = new SettingsDialog(m_api, this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->open();
    });
    navigation->addStretch();
    m_message = new QLabel; m_message->setWordWrap(true); m_message->setAlignment(Qt::AlignCenter); layout->addWidget(m_message);
    auto *keyboard = new TouchKeyboard(this); layout->addWidget(keyboard); keyboard->watch(search, false);
    connect(keyboardButton, &QPushButton::clicked, this, [=] { search->setFocus(); keyboard->show(); });
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
        if (!m_demo && row >= m_library->rowCount() - 24) m_library->loadNext();
    });
    connect(m_library, &LibraryModel::error, this, &MainWindow::setMessage);
    connect(m_library, &QAbstractItemModel::modelReset, this, [this] { ++m_drawerGeneration; });
    connect(m_library, &LibraryModel::pageLoaded, this, [this] {
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
        m_play->setText(m_player.state == "play" ? "Ⅱ" : "▶");
        m_repeat->setText("Repeat " + m_player.repeatMode); m_shuffle->setChecked(m_player.shuffle);
        m_shuffle->setCheckable(true);
        const bool modes = m_player.source != "radio" && m_player.source != "airplay" && m_player.source != "bluetooth";
        m_repeat->setEnabled(modes); m_shuffle->setEnabled(modes);
        if (!m_seek->isSliderDown()) { m_seek->setRange(0, int(qMin(864000., m_player.duration))); m_seek->setValue(int(m_player.elapsed)); }
        m_seek->setEnabled(m_player.duration > 0);
        if (!m_volume->isSliderDown()) m_volume->setValue(qMax(0, m_player.volume));
        m_volume->setEnabled(m_player.volume >= 0);
        m_elapsed->setText(timeLabel(m_player.elapsed)); m_duration->setText(timeLabel(m_player.duration));
        const auto &track = m_player.track;
        const auto info = track.title + (track.artist.isEmpty() ? "" : " · " + track.artist) + (track.album.isEmpty() ? "" : "\n" + track.album + " · " + track.year);
        if (m_now->text() != info) m_now->setText(info);
        m_flow->setPlayingAlbum(track.albumId);
        if (m_message->text().contains("retrying automatically")) setMessage("Connected to PiTunes");
    });
}
void MainWindow::showAlbum(int row, bool play) {
    const auto *selected = m_library->album(row); if (!selected) return;
    const Album album = *selected;
    if (m_demo) { showTracks(album.title, {{"demo/track.flac", "Demonstration track", album.artist, album.title, album.id, album.year, 210}}, album.id); return; }
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
            } else showTracks(album.title + " · " + album.artist, tracks, album.id);
        });
}
void MainWindow::showTracks(QString title, QVector<Track> tracks, QString albumId) {
    auto *dialog = new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setWindowTitle(title); dialog->resize(680, 500);
    auto *layout = new QVBoxLayout(dialog); auto *list = new QListView(dialog); auto *model = new QStandardItemModel(list);
    list->setModel(model); list->setUniformItemSizes(true); layout->addWidget(list);
    for (const auto &track : tracks) {
        auto *item = new QStandardItem(track.title + "   ·   " + track.artist + "   " + timeLabel(track.duration));
        item->setEditable(false); item->setSizeHint(QSize(200, 40)); model->appendRow(item);
    }
    auto *actions = new QHBoxLayout; layout->addLayout(actions);
    auto button = [this, actions, dialog](const QString &text, std::function<void()> action) {
        auto *b = new QPushButton(text, dialog); b->setMinimumHeight(38); actions->addWidget(b); connect(b, &QPushButton::clicked, this, std::move(action));
    };
    const auto play = [this, tracks, albumId](int row) {
        if (row < 0 || row >= tracks.size()) return;
        QJsonArray queue; for (const auto &t : tracks) queue.append(t.file);
        command("/api/player/play", {{"file", tracks[row].file}, {"albumId", albumId}, {"queue", queue}, {"continuous", !albumId.isEmpty()}});
    };
    connect(list, &QListView::activated, dialog, [play](const QModelIndex &i) { play(i.row()); });
    button("Play", [list, play] { play(list->currentIndex().isValid() ? list->currentIndex().row() : 0); });
    button("Favourite song", [this, list, tracks] { const int i = list->currentIndex().row(); if (i >= 0 && i < tracks.size()) command("/api/library/favourites", {{"trackId", tracks[i].file}, {"starred", true}}); });
    button("Unfavourite song", [this, list, tracks] { const int i = list->currentIndex().row(); if (i >= 0 && i < tracks.size()) command("/api/library/favourites", {{"trackId", tracks[i].file}, {"starred", false}}); });
    if (!albumId.isEmpty()) button("Favourite album", [this, albumId] { command("/api/library/favourites", {{"albumId", albumId}, {"starred", true}}); });
    auto *playlistActions = new QHBoxLayout; layout->addLayout(playlistActions);
    auto *create = new QPushButton("Save as playlist…", dialog); playlistActions->addWidget(create);
    connect(create, &QPushButton::clicked, dialog, [this, dialog, tracks] {
        bool ok; const auto name = QInputDialog::getText(dialog, "New playlist", "Name", QLineEdit::Normal, {}, &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        QJsonArray ids; for (const auto &t : tracks) ids.append(t.file);
        command("/api/library/playlists", {{"name", name.trimmed()}, {"trackIds", ids}});
    });
    auto *close = new QDialogButtonBox(QDialogButtonBox::Close, dialog); layout->addWidget(close);
    connect(close, &QDialogButtonBox::rejected, dialog, &QDialog::reject); dialog->open();
}
void MainWindow::showCollection(QString title, QString path, QString key, QString filterPrefix) {
    if (m_demo) { setMessage("Connect a backend to browse " + title); return; }
    m_api->get(path, [this, title, key, filterPrefix](QJsonObject o, QString error) {
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
