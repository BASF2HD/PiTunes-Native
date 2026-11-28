#include "models/Domain.h"
#ifdef PITUNES_STANDALONE
#include "standalone/StandaloneBackend.h"
#include <QDataStream>
#include <QDir>
#endif
#include "models/LibraryModel.h"
#include "coverflow/CoverFlowWidget.h"
#include "ui/MainWindow.h"
#include "ui/MediaInfoPanel.h"
#include "ui/RadioDialog.h"
#include "ui/PerformanceMonitor.h"
#include "ui/SettingsDialog.h"
#include "ui/TrackDrawer.h"
#include <QBuffer>
#include <QComboBox>
#include <QDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QListView>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QStyleOptionSlider>
#include <QTabBar>
#include <QTabWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QToolButton>
#include <QUrlQuery>
#include <QWheelEvent>
#include <algorithm>

using namespace PiTunes;

class HttpFixture : public QTcpServer {
public:
    struct Response { QByteArray body; int delay = 0; int status = 200; };
    std::function<Response(QByteArray)> respond;
    QList<QByteArray> requests;
    HttpFixture() {
        listen(QHostAddress::LocalHost, 0);
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (hasPendingConnections()) {
                auto *socket = nextPendingConnection(); auto data = std::make_shared<QByteArray>();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket, data] {
                    data->append(socket->readAll());
                    if (!data->contains("\r\n\r\n") || socket->property("responded").toBool()) return;
                    // Tests issue only GET or empty-body POST, except where the body is inspected after arrival.
                    const auto headers = data->left(data->indexOf("\r\n\r\n"));
                    int length = 0;
                    for (const auto &line : headers.split('\n'))
                        if (line.toLower().startsWith("content-length:")) length = line.mid(15).trimmed().toInt();
                    if (data->size() < data->indexOf("\r\n\r\n") + 4 + length) return;
                    socket->setProperty("responded", true); requests.append(*data);
                    const auto response = respond ? respond(*data) : Response{"{}"};
                    QPointer<QTcpSocket> guard(socket);
                    QTimer::singleShot(response.delay, this, [guard, response] {
                        if (!guard) return;
                        guard->write("HTTP/1.1 " + QByteArray::number(response.status) + " OK\r\nContent-Length: "
                            + QByteArray::number(response.body.size()) + "\r\nConnection: close\r\n\r\n" + response.body);
                        guard->disconnectFromHost();
                    });
                });
            }
        });
    }
    QUrl url() const { return QUrl("http://127.0.0.1:" + QString::number(serverPort())); }
};

class NativeTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QApplication::setStyle("Fusion"); }
#ifdef PITUNES_STANDALONE
    void standaloneIndexesAndDecodesWithoutAnAppliance() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto root = temp.filePath("music");
        QVERIFY(QDir().mkpath(QDir(root).filePath("Test Album")));
        const auto path = QDir(root).filePath("Test Album/Test Track.wav");
        QFile wav(path);
        QVERIFY(wav.open(QIODevice::WriteOnly));
        QDataStream out(&wav);
        out.setByteOrder(QDataStream::LittleEndian);
        const quint32 frames = 48000 * 2, bytes = frames * 4;
        out.writeRawData("RIFF", 4); out << quint32(bytes + 36); out.writeRawData("WAVEfmt ", 8);
        out << quint32(16) << quint16(1) << quint16(2) << quint32(48000) << quint32(192000) << quint16(4) << quint16(16);
        out.writeRawData("data", 4); out << bytes;
        QCOMPARE(wav.write(QByteArray(bytes, '\0')), qint64(bytes));
        wav.close();

        StandaloneBackend backend(root, nullptr, temp.filePath("index.sqlite"), true);
        QSignalSpy indexed(&backend, &StandaloneBackend::libraryChanged);
        ApiClient api(QUrl("http://127.0.0.1:8080"));
        api.setLocalDispatch(backend.transport());
        QTRY_COMPARE(indexed.count(), 1);
        QJsonObject page;
        api.get("/api/library/albums", [&](QJsonObject result, QString error) {
            QVERIFY2(error.isEmpty(), qPrintable(error));
            page = result;
        });
        QTRY_COMPARE(page["total"].toInt(), 1);
        const auto album = page["albums"].toArray().first().toObject();
        QCOMPARE(album["title"].toString(), QString("Test Album"));
        QJsonObject tracks;
        api.get(QString("/api/library/album/%1/tracks").arg(album["id"].toInt()),
                [&](QJsonObject result, QString error) {
            QVERIFY2(error.isEmpty(), qPrintable(error));
            tracks = result;
        });
        QTRY_COMPARE(tracks["tracks"].toArray().size(), 1);
        QJsonObject playback;
        api.post("/api/player/play", {{"file", tracks["tracks"].toArray().first().toObject()["file"]}},
                 [&](QJsonObject result, QString error) {
            QVERIFY2(error.isEmpty(), qPrintable(error));
            playback = result;
        });
        QTRY_COMPARE(playback["status"].toObject()["state"].toString(), QString("play"));
        QCOMPARE(playback["status"].toObject()["duration"].toDouble(), 2.);
    }
#endif
    void callbackCanDestroyItsApiClient() {
        HttpFixture server; auto *api = new ApiClient(server.url()); QPointer<ApiClient> guard(api);
        bool done = false;
        api->get("/api/test", [&](QJsonObject, QString error) { QVERIFY(error.isEmpty()); delete api; done = true; });
        QTRY_VERIFY(done); QVERIFY(guard.isNull());
    }
    void navigatingCancelsAPendingAlbumDrawer() {
        HttpFixture server;
        server.respond = [](QByteArray request) {
            if (request.startsWith("GET /api/library/albums")) return HttpFixture::Response{
                "{\"albums\":[{\"id\":\"1\",\"title\":\"First\"},{\"id\":\"2\",\"title\":\"Second\"}],\"total\":2}"};
            if (request.startsWith("GET /api/library/album/")) return HttpFixture::Response{
                "{\"tracks\":[{\"file\":\"first/song.flac\",\"title\":\"Old result\"}]}", 150};
            return HttpFixture::Response{"{}"};
        };
        MainWindow window(server.url()); window.show();
        auto *library = window.findChild<LibraryModel *>(); QVERIFY(library); QTRY_COMPARE(library->rowCount(), 2);
        QTest::keyClick(window.coverFlow(), Qt::Key_Return);
        const auto requestedAlbum = [&server] {
            for (const auto &request : server.requests) if (request.startsWith("GET /api/library/album/")) return true;
            return false;
        };
        QTRY_VERIFY(requestedAlbum());
        window.coverFlow()->select(1, false); QTest::qWait(220);
        QVERIFY(!window.findChild<TrackDrawer *>());
    }
    void clickingPlaybackSeekSendsTheClickedTimeAndMuteRestoresVolume() {
        HttpFixture server;
        int volume = 61;
        QList<int> volumes, seeks;
        server.respond = [&](const QByteArray &request) {
            const auto body = QJsonDocument::fromJson(request.mid(request.indexOf("\r\n\r\n") + 4)).object();
            if (request.startsWith("POST /api/player/seek ")) seeks.append(body["seconds"].toInt());
            if (request.startsWith("POST /api/player/volume ")) {
                volume = body["volume"].toInt();
                volumes.append(volume);
            }
            return HttpFixture::Response{QJsonDocument(QJsonObject{{"status", QJsonObject{
                {"state", "pause"}, {"duration", 400}, {"elapsed", 8}, {"volume", volume}}}}).toJson()};
        };
        MainWindow window(server.url());
        window.resize(1280, 680);
        window.show();
        QSlider *seek = nullptr;
        for (auto *slider : window.findChildren<QSlider *>())
            if (slider->accessibleName() == "Playback position") seek = slider;
        QVERIFY(seek);
        QTRY_COMPARE(seek->maximum(), 400);
        QCOMPARE(seek->width(), window.coverFlow()->centreCoverRect().width());
        const auto coverCentre = window.coverFlow()->mapToGlobal(window.coverFlow()->centreCoverRect().center()).x();
        QVERIFY(qAbs(seek->mapToGlobal(seek->rect().center()).x() - coverCentre) <= 1);
        QCOMPARE(seek->height(), 24);
        QTest::mouseClick(seek, Qt::LeftButton, Qt::NoModifier, seek->rect().center());
        QTRY_COMPARE(seeks.size(), 1);
        QVERIFY(qAbs(seeks[0] - 200) <= 2);
        QToolButton *mute = nullptr;
        for (auto *button : window.findChildren<QToolButton *>())
            if (button->accessibleName() == "Mute") mute = button;
        QVERIFY(mute);
        mute->click();
        QTRY_COMPARE(mute->accessibleName(), QString("Unmute"));
        mute->click();
        QTRY_COMPARE(mute->accessibleName(), QString("Mute"));
        QCOMPARE(volumes, QList<int>({0, 61}));
    }
    void captionFollowsTheCentredAlbumWhileBrowsing() {
        HttpFixture server;
        server.respond = [](const QByteArray &request) {
            if (request.startsWith("GET /api/library/albums")) {
                return HttpFixture::Response{R"({"albums":[{"id":1,"title":"Playing album"},{"id":2,"title":"Browsing album","albumArtist":"Album artist","year":1992}],"total":2})"};
            }
            return HttpFixture::Response{R"json({"status":{"state":"play"},"song":{"file":"record/song.flac",
                "Title":"The song","Artist":"The singer","Album":"Playing album (1983)","year":"1983","albumId":1}})json"};
        };
        MainWindow window(server.url());
        window.show();
        auto *flow = window.coverFlow();
        QTRY_COMPARE(flow->visibleCoverCount(), 2);
        const QString caption = "The song\nThe singer\nPlaying album | 1983";
        QTRY_COMPARE(flow->accessibleDescription(), caption);
        flow->select(1);
        QTRY_COMPARE(flow->accessibleDescription(), QString("Browsing album\nAlbum artist | 1992"));
        flow->select(0, false);
        QCOMPARE(flow->accessibleDescription(), caption);
    }
    void searchOverlayKeepsTheStageAndTransportInPlace() {
        HttpFixture server;
        MainWindow window(server.url(), true);
        window.resize(800, 480);
        window.show();
        auto *flow = window.coverFlow();
        auto *transport = window.findChild<QFrame *>("transportPanel");
        auto *panel = window.findChild<QFrame *>("librarySearchPanel");
        QVERIFY(transport); QVERIFY(panel);
        QTest::qWait(30);
        const auto originalStage = flow->geometry();
        const auto originalCover = flow->centreCoverRect();
        const auto originalTransport = transport->geometry();
        QToolButton *search = nullptr;
        for (auto *button : window.findChildren<QToolButton *>()) {
            if (button->accessibleName() == "Open search") search = button;
        }
        QVERIFY(search);
        QTest::mouseClick(search, Qt::LeftButton);
        QTRY_VERIFY(panel->isVisible());
        QCOMPARE(panel->parentWidget(), flow);
        for (auto *button : panel->findChildren<QPushButton *>()) {
            if (button->accessibleName() == "Open search keyboard") QTest::mouseClick(button, Qt::LeftButton);
        }
        QCoreApplication::processEvents();
        QCOMPARE(flow->geometry(), originalStage);
        QCOMPARE(flow->centreCoverRect(), originalCover);
        QCOMPARE(transport->geometry(), originalTransport);
        QTest::mouseClick(search, Qt::LeftButton);
        QVERIFY(!panel->isVisible());
        QCOMPARE(flow->geometry(), originalStage);
    }
    void browseButtonsHaveContextMenus() {
        HttpFixture server;
        MainWindow window(server.url(), true);
        window.resize(1280, 680);
        window.show();
        const QMap<QString, QString> items{
            {"Albums", "Favourite"}, {"Songs", "Group by album"}, {"Artists", "Browse artists"},
            {"Playlists", "Playlist order"}, {"More", "Genres"}, {"Settings", "Application settings"}
        };
        int found = 0;
        QVector<int> centres;
        for (auto *button : window.findChildren<QToolButton *>()) {
            const auto expected = items.value(button->accessibleName());
            if (expected.isEmpty()) continue;
            ++found;
            centres.append(button->mapTo(&window, button->rect().center()).x());
            QTest::mouseClick(button, Qt::LeftButton);
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            QVERIFY(menu);
            QCOMPARE(menu->accessibleName(), button->accessibleName() + " menu");
            bool present = false;
            for (const auto *action : menu->actions()) present |= action->text() == expected;
            QVERIFY(present);
            menu->hide();
        }
        QCOMPARE(found, items.size());
        const auto horizontal = std::minmax_element(centres.cbegin(), centres.cend());
        QVERIFY(*horizontal.second - *horizontal.first < 520);
        QVERIFY(qAbs((*horizontal.second + *horizontal.first) / 2 - window.width() / 2) < 5);
    }
    void browsePopupHandsOffTheClickToTheNextMenu() {
        HttpFixture server;
        MainWindow window(server.url(), true);
        window.resize(1000, 600);
        window.show();
        QHash<QString, QToolButton *> buttons;
        for (auto *button : window.findChildren<QToolButton *>("browseTab")) buttons.insert(button->accessibleName(), button);
        QVERIFY(buttons.contains("Albums"));
        QTest::mouseClick(buttons["Albums"], Qt::LeftButton);
        for (const QString name : {"Songs", "Artists", "Playlists", "More", "Settings", "Albums"}) {
            QVERIFY(buttons.contains(name));
            auto *popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            QVERIFY(popup);
            const auto *button = buttons.value(name);
            const auto global = button->mapToGlobal(button->rect().center());
            const auto local = popup->mapFromGlobal(global);
            QMouseEvent press(QEvent::MouseButtonPress, local, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QMouseEvent release(QEvent::MouseButtonRelease, local, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(popup, &press);
            QCoreApplication::sendEvent(popup, &release);
            auto *next = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            QVERIFY(next && next != popup);
            QCOMPARE(next->accessibleName(), name + " menu");
        }
        QApplication::activePopupWidget()->hide();
    }
    void settingsTabsSwitchByClick() {
        HttpFixture server;
        server.respond = [](const QByteArray &request) {
            if (request.startsWith("GET /api/audio/devices ")) return HttpFixture::Response{
                R"({"routes":[{"id":"hdmi","label":"HDMI"},{"id":"usb-dac","label":"USB DAC"}]})"};
            return HttpFixture::Response{"{}"};
        };
        ApiClient api(server.url());
        SettingsDialog dialog(&api, {});
        dialog.open();
        auto *tabs = dialog.findChild<QTabWidget *>();
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 6);
        for (const int row : {1, 2, 3, 4, 5, 0}) {
            QTest::mouseClick(tabs->tabBar(), Qt::LeftButton, Qt::NoModifier, tabs->tabBar()->tabRect(row).center());
            QCOMPARE(tabs->currentIndex(), row);
            QVERIFY(tabs->widget(row)->isVisible());
        }
        tabs->setCurrentIndex(2);
        QTRY_VERIFY(dialog.findChild<QComboBox *>("applianceAudioRoute"));
        auto *routes = dialog.findChild<QComboBox *>("applianceAudioRoute");
        QTRY_COMPARE(routes->count(), 2);
        routes->showPopup();
        auto *popup = routes->view()->window();
        QVERIFY(popup->isVisible());
        const auto global = tabs->tabBar()->mapToGlobal(tabs->tabBar()->tabRect(3).center());
        const auto local = popup->mapFromGlobal(global);
        QMouseEvent press(QEvent::MouseButtonPress, local, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, local, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(popup, &press);
        QCoreApplication::sendEvent(popup, &release);
        QCOMPARE(tabs->currentIndex(), 3);
        QVERIFY(!popup->isVisible());
    }
    void menuSwitchesCoverShelfWithoutChangingPlayback() {
        HttpFixture server;
        QJsonObject played;
        server.respond = [&](const QByteArray &request) {
            if (request.startsWith("GET /api/library/albums")) return HttpFixture::Response{
                "{\"albums\":[{\"id\":1,\"title\":\"Album shelf\"}],\"total\":1}"};
            if (request.startsWith("GET /api/library/tracks")) return HttpFixture::Response{
                "{\"tracks\":[{\"file\":\"album/song.flac\",\"title\":\"Track shelf\",\"albumId\":1,\"artUrl\":\"/api/art?album_id=1\"}],\"total\":1}"};
            if (request.startsWith("GET /api/search")) return HttpFixture::Response{
                "{\"tracks\":[{\"file\":\"other.flac\",\"title\":\"Matching song\",\"albumId\":2}],\"albums\":[]}"};
            if (request.startsWith("GET /api/library/radio ")) return HttpFixture::Response{
                "{\"stations\":[{\"id\":\"station-id\",\"name\":\"Saved Radio\",\"streamUrl\":\"https://station.invalid/live\",\"artUrl\":\"https://station.invalid/logo.png\"}]}"};
            if (request.startsWith("POST /api/player/radio/play "))
                played = QJsonDocument::fromJson(request.mid(request.indexOf("\r\n\r\n") + 4)).object();
            return HttpFixture::Response{"{}"};
        };
        MainWindow window(server.url());
        window.show();
        auto *model = window.findChild<LibraryModel *>();
        QVERIFY(model);
        QTRY_COMPARE(model->rowCount(), 1);
        for (auto *button : window.findChildren<QToolButton *>("browseTab")) {
            if (button->accessibleName() == "Songs") QTest::mouseClick(button, Qt::LeftButton);
        }
        QTRY_VERIFY(model->entry(0) && model->entry(0)->kind == CoverEntry::TrackCover);
        QCOMPARE(model->entry(0)->face.title, QString("Track shelf"));
        QApplication::activePopupWidget()->hide();
        model->search("Matching");
        QTRY_VERIFY(model->entry(0) && model->entry(0)->face.title == "Matching song");
        model->search({});
        QTRY_VERIFY(model->entry(0) && model->entry(0)->face.title == "Track shelf");
        for (auto *button : window.findChildren<QToolButton *>("browseTab")) {
            if (button->accessibleName() == "More") QTest::mouseClick(button, Qt::LeftButton);
        }
        auto *popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        QVERIFY(popup);
        for (auto *action : popup->actions()) {
            if (action->text() != "Radio") continue;
            popup->hide();
            action->trigger();
        }
        QTRY_VERIFY(model->entry(0) && model->entry(0)->kind == CoverEntry::StationCover);
        QVERIFY(model->entry(0)->face.artUrl.startsWith("/api/library/radio/icon?"));
        QVERIFY(!window.findChild<RadioDialog *>());
        for (const auto &request : server.requests) QVERIFY(!request.startsWith("POST /api/player/"));
        auto *flow = window.coverFlow();
        flow->grab();
        QTest::mouseDClick(flow, Qt::LeftButton, Qt::NoModifier, flow->centreCoverRect().center());
        QTRY_COMPARE(played["stationId"].toString(), QString("station-id"));
    }
    void radioSearchShowsArtworkAndSavesWithoutPlaying() {
        HttpFixture server;
        QImage art(64, 64, QImage::Format_RGB32);
        art.fill(Qt::red);
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        QVERIFY(art.save(&buffer, "PNG"));
        const QJsonObject station{{"name", "City Radio"}, {"url", "https://radio.invalid/live"}, {"externalUuid", "city-1"},
                                  {"artUrl", "https://radio.invalid/logo?size=64&variant=dark"}, {"genre", "Jazz"}};
        QJsonObject stored;
        QJsonObject played;
        server.respond = [&](const QByteArray &request) {
            if (request.startsWith("GET /api/library/radio/icon?")) return HttpFixture::Response{png};
            QJsonObject response;
            const auto body = QJsonDocument::fromJson(request.mid(request.indexOf("\r\n\r\n") + 4)).object();
            if (request.startsWith("GET /api/library/radio/search?")) response["stations"] = QJsonArray{station};
            if (request.startsWith("GET /api/library/radio ")) response["stations"] = stored.isEmpty() ? QJsonArray{} : QJsonArray{stored};
            if (request.startsWith("POST /api/library/radio/stations ")) {
                stored = body;
                stored["id"] = "saved-id";
                response["station"] = stored;
            }
            if (request.startsWith("POST /api/player/radio/play ")) played = body;
            return HttpFixture::Response{QJsonDocument(response).toJson()};
        };
        MainWindow window(server.url());
        window.show();
        for (auto *button : window.findChildren<QToolButton *>("browseTab"))
            if (button->accessibleName() == "More") QTest::mouseClick(button, Qt::LeftButton);
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        QVERIFY(menu);
        for (auto *action : menu->actions()) {
            if (action->text() != "Find internet radio…") continue;
            menu->hide();
            action->trigger();
        }
        auto *dialog = window.findChild<RadioDialog *>();
        QVERIFY(dialog);
        auto *input = dialog->findChild<QLineEdit *>();
        auto *list = dialog->findChild<QListView *>();
        QVERIFY(input && list);
        input->setText("City Radio");
        QTest::keyClick(input, Qt::Key_Return);
        QTRY_COMPARE(list->model()->rowCount(), 1);
        auto *cache = dialog->findChild<ArtworkCache *>();
        QVERIFY(cache);
        const auto key = "/api/library/radio/icon?wait=1&url=" +
            QString::fromUtf8(QUrl::toPercentEncoding(station["artUrl"].toString()));
        QTRY_VERIFY(cache->find(key));
        QCOMPARE(cache->find(key)->cover.pixelColor(32, 32), QColor(Qt::red));
        QMap<QString, QPushButton *> buttons;
        for (auto *button : dialog->findChildren<QPushButton *>()) buttons[button->text()] = button;
        QVERIFY(buttons.contains("Save station") && buttons.contains("Play station") && buttons.contains("Saved stations"));
        auto *save = buttons.value("Save station");
        QTest::mouseClick(save, Qt::LeftButton);
        QTRY_COMPARE(save->text(), QString("Saved"));
        QVERIFY(!save->isEnabled());
        QVERIFY(played.isEmpty());
        QCOMPARE(stored["artUrl"], station["artUrl"]);
        QCOMPARE(stored["externalUuid"], station["externalUuid"]);
        QTest::mouseClick(buttons.value("Saved stations"), Qt::LeftButton);
        QTRY_COMPARE(list->model()->rowCount(), 1);
        QTest::mouseClick(buttons.value("Play station"), Qt::LeftButton);
        QTRY_COMPARE(played["stationId"].toString(), QString("saved-id"));
    }
    void fontTabResizesAndPersistsEachCaptionLine() {
        HttpFixture server;
        MainWindow window(server.url(), true);
        window.resize(1280, 680);
        window.show();
        QToolButton *settings = nullptr;
        for (auto *button : window.findChildren<QToolButton *>())
            if (button->accessibleName() == "Settings") settings = button;
        QVERIFY(settings);
        QTest::mouseClick(settings, Qt::LeftButton);
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        QVERIFY(menu);
        for (auto *action : menu->actions()) {
            if (action->text() != "Application settings") continue;
            menu->hide();
            action->trigger();
        }
        auto *dialog = window.findChild<SettingsDialog *>();
        QVERIFY(dialog);
        auto *tabs = dialog->findChild<QTabWidget *>();
        QVERIFY(tabs);
        QCOMPARE(tabs->tabText(tabs->currentIndex()), QString("Font"));
        auto *title = dialog->findChild<QSpinBox *>("captionTitleSize");
        auto *artist = dialog->findChild<QSpinBox *>("captionArtistSize");
        auto *album = dialog->findChild<QSpinBox *>("captionAlbumSize");
        QVERIFY(title); QVERIFY(artist); QVERIFY(album);
        const auto original = window.coverFlow()->captionFontSizes();
        const auto clickStep = [](QSpinBox *control, bool up) {
            QStyleOptionSpinBox option;
            option.initFrom(control);
            option.buttonSymbols = QAbstractSpinBox::UpDownArrows;
            const auto button = up ? QStyle::SC_SpinBoxUp : QStyle::SC_SpinBoxDown;
            const auto position = control->style()->subControlRect(QStyle::CC_SpinBox, &option, button, control).center();
            QTest::mouseClick(control, Qt::LeftButton, Qt::NoModifier, position);
        };
        title->setValue(45);
        clickStep(title, true);
        artist->setValue(44);
        clickStep(artist, false);
        album->setValue(42);
        const auto updated = window.coverFlow()->captionFontSizes();
        QCOMPARE(updated.title, 46);
        QCOMPARE(updated.artist, 43);
        QCOMPARE(updated.album, 42);
        MainWindow reopened(server.url(), true);
        auto persisted = reopened.coverFlow()->captionFontSizes();
        QCOMPARE(persisted.title, 46);
        QCOMPARE(persisted.artist, 43);
        QCOMPARE(persisted.album, 42);
        title->setValue(original.title);
        artist->setValue(original.artist);
        album->setValue(original.album);
        auto *titleBar = dialog->findChild<QWidget *>("settingsTitleBar");
        auto *close = dialog->findChild<QToolButton *>("settingsClose");
        QVERIFY(titleBar); QVERIFY(close);
        const QPoint start = dialog->pos();
        const QPoint local(45, 14);
        const QPoint global = titleBar->mapToGlobal(local);
        QMouseEvent press(QEvent::MouseButtonPress, local, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent move(QEvent::MouseMove, local + QPoint(42, 23), global + QPoint(42, 23),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(titleBar, &press);
        QCoreApplication::sendEvent(titleBar, &move);
        QCOMPARE(dialog->pos(), start + QPoint(42, 23));
        QTest::mouseClick(close, Qt::LeftButton);
        QTRY_VERIFY(!window.findChild<SettingsDialog *>());
        QCOMPARE(server.requests.size(), 0);
    }
    void returnsToPlayingAlbumAfterBrowsingAndClosingDrawer() {
        HttpFixture server;
        server.respond = [](const QByteArray &request) {
            if (request.startsWith("GET /api/library/albums")) {
                return HttpFixture::Response{R"({"albums":[{"id":1,"title":"Playing album"},{"id":2,"title":"Other album"}],"total":2})"};
            }
            if (request.startsWith("GET /api/player/state")) {
                return HttpFixture::Response{R"({"status":{"state":"play"},"song":{"file":"record/song.flac","albumId":1}})"};
            }
            return HttpFixture::Response{"{}"};
        };
        MainWindow window(server.url());
        window.show();
        auto *flow = window.coverFlow();
        QTRY_COMPARE(flow->visibleCoverCount(), 2);
        QTRY_VERIFY(flow->accessibleDescription().startsWith("Unknown song\n"));
        flow->select(1, false);
        QTest::keyClick(flow, Qt::Key_Return);
        QTRY_VERIFY(window.findChild<TrackDrawer *>());
        QTest::qWait(5100);
        QCOMPARE(flow->currentIndex(), 1);
        QTest::keyClick(window.findChild<TrackDrawer *>()->findChild<QListView *>(), Qt::Key_Escape);
        QTRY_VERIFY(!window.findChild<TrackDrawer *>());
        QTest::qWait(4000);
        QCOMPARE(flow->currentIndex(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(flow->currentIndex(), 0, 2500);
        for (const auto &request : server.requests) QVERIFY(!request.startsWith("POST "));
    }
    void albumHeartFillsAndRestoresOnSaveError() {
        HttpFixture server;
        int posts = 0;
        server.respond = [&posts](const QByteArray &request) {
            if (request.startsWith("GET /api/library/albums")) {
                return HttpFixture::Response{R"({"albums":[{"id":1,"title":"Album"}],"total":1})"};
            }
            if (request.startsWith("GET /api/library/favourites")) {
                return HttpFixture::Response{R"({"albums":[],"tracks":[]})"};
            }
            if (request.startsWith("POST /api/library/favourites")) {
                ++posts;
                return posts == 1 ? HttpFixture::Response{R"({"ok":true,"starred":true})", 80}
                                  : HttpFixture::Response{R"({"ok":false,"message":"Fixture error"})", 80};
            }
            return HttpFixture::Response{"{}"};
        };
        MainWindow window(server.url());
        window.show();
        auto *flow = window.coverFlow();
        QTRY_COMPARE(flow->visibleCoverCount(), 1);
        QTest::keyClick(flow, Qt::Key_Return);
        QTRY_VERIFY(window.findChild<TrackDrawer *>());
        QPushButton *heart = nullptr;
        for (auto *button : window.findChild<TrackDrawer *>()->findChildren<QPushButton *>())
            if (button->accessibleName() == "Favourite album") heart = button;
        QVERIFY(heart);
        QTRY_VERIFY(heart->isEnabled());
        auto *captionHeart = window.findChild<QToolButton *>("captionHeart");
        QVERIFY(captionHeart);
        QVERIFY(captionHeart->isVisible());
        QCOMPARE(captionHeart->accessibleName(), QString("Favourite album"));
        const auto outline = heart->icon().cacheKey();
        QTest::mouseClick(captionHeart, Qt::LeftButton);
        QVERIFY(heart->isChecked());
        QVERIFY(heart->icon().cacheKey() != outline);
        QCOMPARE(captionHeart->accessibleName(), QString("Remove favourite album"));
        QTRY_COMPARE(posts, 1);
        QTest::qWait(125);
        QTest::mouseClick(heart, Qt::LeftButton);
        QVERIFY(!heart->isChecked());
        QTRY_COMPARE(posts, 2);
        QTRY_VERIFY(heart->isChecked());
        QCOMPARE(captionHeart->accessibleName(), QString("Remove favourite album"));
        int mutations = 0;
        for (const auto &request : server.requests) {
            if (!request.startsWith("POST /api/library/favourites")) continue;
            const auto body = QJsonDocument::fromJson(request.mid(request.indexOf("\r\n\r\n") + 4)).object();
            QCOMPARE(body["albumId"].toString(), QString("1"));
            QCOMPARE(body["starred"].toBool(), mutations++ == 0);
        }
        QCOMPARE(mutations, 2);
        QTest::keyClick(window.findChild<TrackDrawer *>()->findChild<QListView *>(), Qt::Key_Escape);
        QTRY_VERIFY(!window.findChild<TrackDrawer *>());
        QToolButton *actions = nullptr;
        for (auto *button : window.findChildren<QToolButton *>())
            if (button->accessibleName() == "Album or song information actions") actions = button;
        QVERIFY(actions); QVERIFY(actions->isVisible());
        QTest::mouseClick(actions, Qt::LeftButton);
        auto *infoMenu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        QVERIFY(infoMenu);
        QCOMPARE(infoMenu->accessibleName(), QString("Album actions"));
        const auto menuActions = infoMenu->actions();
        QVERIFY(std::any_of(menuActions.begin(), menuActions.end(), [](const QAction *action) {
            return action->text() == "More info";
        }));
        infoMenu->hide();
    }
    void moreInfoHydratesSongAndAlbumDetails() {
        HttpFixture server;
        server.respond = [](const QByteArray &request) {
            if (request.startsWith("GET /api/library/albums")) {
                return HttpFixture::Response{R"({"albums":[{"id":1,"title":"Playing"},{"id":2,"title":"Other","albumArtist":"Album Artist","year":1984,"genre":"Soundtrack"}],"total":2})"};
            }
            if (request.startsWith("GET /api/player/state")) {
                return HttpFixture::Response{R"({"status":{"state":"play","bitrate":920},"song":{"file":"a/song.flac","Title":"Playing song","albumId":1}})"};
            }
            if (request.startsWith("GET /api/library/album/1/tracks")) {
                return HttpFixture::Response{R"({"tracks":[{"file":"a/other.flac","composer":"Wrong"},
                    {"file":"a/song.flac","title":"Playing song","singer":"Singer","composer":"Composer","trackNumber":4,"duration":186}]})", 90};
            }
            if (request.startsWith("GET /api/library/album/2/tracks")) {
                return HttpFixture::Response{R"({"tracks":[{"file":"b/one.flac","artist":"First","duration":218},
                    {"file":"b/two.flac","artist":"Second","duration":102}]})"};
            }
            return HttpFixture::Response{"{}"};
        };
        MainWindow window(server.url());
        window.show();
        auto *flow = window.coverFlow();
        QTRY_VERIFY(flow->accessibleDescription().startsWith("Playing song\n"));
        QToolButton *actions = nullptr;
        for (auto *button : window.findChildren<QToolButton *>())
            if (button->accessibleName() == "Album or song information actions") actions = button;
        QVERIFY(actions);
        const auto openDetails = [&] {
            QTest::mouseClick(actions, Qt::LeftButton);
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            QVERIFY(menu);
            for (auto *action : menu->actions()) {
                if (action->text() != "More info") continue;
                menu->hide();
                action->trigger();
                return;
            }
            QFAIL("Missing More info action");
        };
        const auto field = [&](const QString &name) {
            auto *panel = window.findChild<MediaInfoPanel *>();
            if (panel) {
                for (auto *label : panel->findChildren<QLabel *>("detailValue"))
                    if (label->accessibleName() == name) return label->text();
            }
            return QString();
        };
        openDetails();
        QTRY_COMPARE(field("Composer"), QString("Composer"));
        QCOMPARE(field("Singer"), QString("Singer"));
        QCOMPARE(field("Nr"), QString("4"));
        QCOMPARE(field("Codec"), QString("FLAC"));
        QCOMPARE(field("Bitrate"), QString("920 kbps"));
        QCOMPARE(field("Path"), QString("a/song.flac"));
        QTest::keyClick(window.findChild<MediaInfoPanel *>(), Qt::Key_Escape);
        QTRY_VERIFY(!window.findChild<MediaInfoPanel *>());
        flow->select(1, false);
        openDetails();
        QTRY_COMPARE(field("Songs"), QString("2"));
        QCOMPARE(field("Artist"), QString("First, Second"));
        QCOMPARE(field("Duration"), QString("5:20"));
        QCOMPARE(field("Genre"), QString("Soundtrack"));
    }
    void albumStripBrowsesAcrossPagesWithoutChangingPlayback() {
        HttpFixture server;
        server.respond = [](const QByteArray &request) {
            if (request.startsWith("GET /api/library/albums?offset=0")) {
                return HttpFixture::Response{R"({"albums":[{"id":1,"title":"First"},{"id":2,"title":"Second"}],"total":4})"};
            }
            if (request.startsWith("GET /api/library/albums?offset=2")) {
                return HttpFixture::Response{R"({"albums":[{"id":3,"title":"Third"},{"id":4,"title":"Last"}],"total":4})"};
            }
            return HttpFixture::Response{"{}"};
        };
        MainWindow window(server.url());
        window.show();
        QSlider *strip = nullptr;
        for (auto *slider : window.findChildren<QSlider *>())
            if (slider->accessibleName() == "Browse albums") strip = slider;
        QVERIFY(strip);
        QTRY_COMPARE(strip->maximum(), 3);
        QStyleOptionSlider style;
        style.initFrom(strip);
        style.minimum = strip->minimum();
        style.maximum = strip->maximum();
        style.orientation = Qt::Horizontal;
        style.state |= QStyle::State_Horizontal;
        style.subControls = QStyle::SC_SliderHandle | QStyle::SC_SliderGroove;
        const auto handle = strip->style()->subControlRect(QStyle::CC_Slider, &style, QStyle::SC_SliderHandle, strip);
        const auto groove = strip->style()->subControlRect(QStyle::CC_Slider, &style, QStyle::SC_SliderGroove, strip);
        QVERIFY(handle.height() >= groove.height() - 1);
        QVERIFY(handle.width() > 36);
        QTest::mouseClick(strip, Qt::LeftButton, Qt::NoModifier, QPoint(strip->width() - 2, strip->height() / 2));
        QTRY_COMPARE(window.coverFlow()->currentIndex(), 3);
        QTRY_COMPARE(window.coverFlow()->accessibleDescription(), QString("Last"));
        QToolButton *previousAlbum = nullptr;
        for (auto *button : window.findChildren<QToolButton *>())
            if (button->accessibleName() == "Previous album") previousAlbum = button;
        QVERIFY(previousAlbum);
        QTest::mouseClick(previousAlbum, Qt::LeftButton);
        QCOMPARE(window.coverFlow()->currentIndex(), 2);
        QCOMPARE(strip->value(), 2);
        for (const auto &request : server.requests) QVERIFY(!request.startsWith("POST "));
    }
    void drawerAnimatesOnlyThePlayingMarker() {
        HttpFixture server;
        ApiClient api(server.url());
        LibraryModel library(&api);
        library.setDemo();
        ArtworkCache cache(&api);
        CoverFlowWidget flow(&library, &cache);
        flow.resize(800, 420);
        flow.show();
        Track track;
        track.file = "album/song.flac";
        track.title = "Song";
        TrackDrawer drawer(&flow, "Album", "1983", {track}, true);
        drawer.open();
        auto *list = drawer.findChild<QListView *>();
        const QRect row = list->visualRect(list->model()->index(0, 0));
        QVERIFY(row.isValid());
        const auto marker = [&] {
            return list->viewport()->grab(QRect(row.left(), row.top(), 33, row.height())).toImage();
        };
        PlayerState state;
        state.track = track;
        state.state = "play";
        drawer.setPlayback(state);
        const auto start = marker();
        QVERIFY(!start.isNull());
        QTRY_VERIFY(marker() != start);
        state.state = "pause";
        drawer.setPlayback(state);
        const auto paused = marker();
        QVERIFY(paused != start);
        QTest::qWait(120);
        QCOMPARE(marker(), paused);
        drawer.setReducedMotion(true);
        state.state = "play";
        drawer.setPlayback(state);
        const auto reduced = marker();
        QVERIFY(reduced != paused);
        QTest::qWait(120);
        QCOMPARE(marker(), reduced);
    }
    void trackDrawerPlaysTheSelectedFileWithTheAlbumQueue() {
        HttpFixture server;
        server.respond = [](const QByteArray &request) {
            if (request.startsWith("GET /api/library/albums")) {
                return HttpFixture::Response{R"({"albums":[{"id":7,"title":"Album","year":1986}],"total":1})"};
            }
            if (request.startsWith("GET /api/library/album/7/tracks")) {
                return HttpFixture::Response{R"({"tracks":[
                    {"file":"album/first.flac","title":"First","artist":"Singer One","trackNumber":1},
                    {"file":"album/second.flac","title":"Second","artist":"Singer Two","trackNumber":2,"duration":150}]})"};
            }
            if (request.startsWith("GET /api/library/favourites")) {
                return HttpFixture::Response{R"({"albums":[],"tracks":["album/first.flac"]})"};
            }
            return HttpFixture::Response{"{}"};
        };
        MainWindow window(server.url());
        window.show();
        auto *flow = window.coverFlow();
        QTRY_COMPARE(flow->visibleCoverCount(), 1);
        QTest::keyClick(flow, Qt::Key_Return);
        QTRY_VERIFY(window.findChild<TrackDrawer *>());
        auto *drawer = window.findChild<TrackDrawer *>();
        QCOMPARE(drawer->parentWidget(), flow);
        QCOMPARE(drawer->accessibleName(), QString("Album | 1986"));
        auto *list = drawer->findChild<QListView *>();
        const auto second = list->model()->index(1, 0);
        QVERIFY(second.data(Qt::AccessibleTextRole).toString().contains("Singer Two"));
        const auto playRequest = [&server] {
            for (const auto &request : server.requests) {
                if (request.startsWith("POST /api/player/play ")) return request;
            }
            return QByteArray();
        };
        const QRect row = list->visualRect(second);
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(row.right() - 8, row.center().y()));
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        QVERIFY(menu);
        for (const auto *action : menu->actions()) {
            QVERIFY(action->text() != "Play song");
            QVERIFY(!action->text().contains("favourite", Qt::CaseInsensitive));
        }
        QVERIFY(playRequest().isEmpty());
        menu->hide();
        QTRY_VERIFY(list->model()->index(0, 0).data(Qt::AccessibleTextRole).toString().endsWith(", Favourite"));
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(row.right() - 107, row.center().y()));
        QTRY_VERIFY(second.data(Qt::AccessibleTextRole).toString().endsWith(", Favourite"));
        QVERIFY(playRequest().isEmpty());
        const auto favouriteRequest = [&server] {
            for (const auto &request : server.requests) {
                if (request.startsWith("POST /api/library/favourites ")) return request;
            }
            return QByteArray();
        };
        QTRY_VERIFY(!favouriteRequest().isEmpty());
        const auto favourite = favouriteRequest();
        const auto saved = QJsonDocument::fromJson(favourite.mid(favourite.indexOf("\r\n\r\n") + 4)).object();
        QCOMPARE(saved["trackId"].toString(), QString("album/second.flac"));
        QVERIFY(saved["starred"].toBool());
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, row.center());
        QTRY_VERIFY(!playRequest().isEmpty());
        const auto request = playRequest();
        const auto body = QJsonDocument::fromJson(request.mid(request.indexOf("\r\n\r\n") + 4)).object();
        QCOMPARE(body["file"].toString(), QString("album/second.flac"));
        QCOMPARE(body["queue"].toArray(), QJsonArray({"album/first.flac", "album/second.flac"}));
        QVERIFY(body["continuous"].toBool());
        QTest::keyClick(list, Qt::Key_Escape);
        QTRY_VERIFY(!window.findChild<TrackDrawer *>());
    }
    void demoNeverContactsAnAppliance() {
        HttpFixture server; MainWindow window(server.url(), true); window.show();
        for (auto *button : window.findChildren<QToolButton *>())
            if (button->accessibleName() == "Settings") QTest::mouseClick(button, Qt::LeftButton);
        QTest::qWait(100); QCOMPARE(server.requests.size(), 0);
    }
    void nativeWindowSerializesTransportIntents() {
        HttpFixture server;
        server.respond = [](QByteArray req) {
            if (req.startsWith("GET /api/library/albums")) return HttpFixture::Response{"{\"albums\":[{\"id\":\"7\",\"title\":\"Album\"}],\"total\":1}"};
            if (req.startsWith("GET /api/player/state")) return HttpFixture::Response{
                "{\"inputSource\":\"local\",\"status\":{\"state\":\"play\",\"volume\":65,\"duration\":210,\"random\":true},\"song\":{\"file\":\"album/first.flac\",\"Title\":\"First\",\"albumId\":\"7\"}}"};
            return HttpFixture::Response{"{}", 80};
        };
        MainWindow window(server.url()); window.show();
        QToolButton *play = nullptr, *next = nullptr, *shuffle = nullptr;
        for (auto *b : window.findChildren<QToolButton *>()) {
            if (b->accessibleName() == "Play or pause") play = b;
            if (b->accessibleName() == "Next song") next = b;
            if (b->accessibleName().startsWith("Shuffle")) shuffle = b;
        }
        QVERIFY(play); QVERIFY(next); QVERIFY(shuffle);
        QTRY_VERIFY(play->property("playing").toBool());
        QVERIFY(shuffle->isChecked());
        QTest::mouseClick(next, Qt::LeftButton); QTest::mouseClick(next, Qt::LeftButton);
        const auto nextCount = [&server] { int n = 0; for (const auto &r : server.requests) if (r.startsWith("POST /api/player/next")) ++n; return n; };
        QTRY_COMPARE(nextCount(), 2);
        QTest::mouseClick(play, Qt::LeftButton);
        const auto paused = [&server] { for (const auto &r : server.requests) if (r.startsWith("POST /api/player/pause")) return true; return false; };
        QTRY_VERIFY(paused());
    }
    void mixedPlayerSchema() {
        const auto p = PlayerState::fromJson({{"inputSource", "local"}, {"status", QJsonObject{
            {"state", "play"}, {"volume", "-1"}, {"elapsed", "83.5"}, {"duration", 210},
            {"repeat", "1"}, {"single", true}, {"random", "false"}}}, {"song", QJsonObject{
            {"file", "தமிழ்/பாடல்.flac"}, {"Title", "பாடல்"}, {"Artist", "Singer"}, {"Album", "Release"}, {"albumId", 72}}}});
        QCOMPARE(p.elapsed, 83.5); QCOMPARE(p.volume, -1); QCOMPARE(p.repeatMode, QString("song"));
        QVERIFY(!p.shuffle); QCOMPARE(p.track.albumId, QString("72")); QCOMPARE(p.track.artist, QString("Singer"));
        QCOMPARE(timeLabel(p.elapsed), QString("1:23"));
    }
    void authoritativeAlbumIds() {
        const auto a = Album::fromJson({{"id", "12"}, {"title", "Greatest Hits"}, {"albumArtist", "Various Artists"}, {"year", 1998}});
        const auto b = Album::fromJson({{"id", "13"}, {"title", "Greatest Hits"}, {"year", 2024}});
        QVERIFY(a.id != b.id); QCOMPARE(a.artist, QString("Various Artists")); QCOMPARE(a.year, QString("1998"));
    }
    void payloadFailuresAndInvalidJson() {
        HttpFixture server;
        server.respond = [](QByteArray request) { return HttpFixture::Response{request.contains("invalid") ? "oops" : "{\"ok\":false,\"message\":\"Output unavailable\"}"}; };
        ApiClient api(server.url()); QString error; bool done = false;
        api.get("/api/test", [&](QJsonObject, QString e) { error = e; done = true; });
        QTRY_VERIFY(done); QCOMPARE(error, QString("Output unavailable")); done = false;
        api.get("/api/invalid", [&](QJsonObject, QString e) { error = e; done = true; });
        QTRY_VERIFY(done); QVERIFY(error.contains("invalid JSON"));
    }
    void timeoutsDoNotRetryMutations() {
        HttpFixture server; server.respond = [](QByteArray) { return HttpFixture::Response{"{}", 200}; };
        ApiClient api(server.url()); bool done = false; QString error;
        api.post("/api/player/next", {}, [&](QJsonObject, QString e) { error = e; done = true; }, 40);
        QTRY_VERIFY(done); QVERIFY(error.contains("timed out")); QTest::qWait(220); QCOMPARE(server.requests.size(), 1);
    }
    void originAndSizeBounds() {
        HttpFixture server; ApiClient api(server.url()); bool done = false; QString error;
        api.get("https://example.invalid/api/test", [&](QJsonObject, QString e) { error = e; done = true; });
        QTRY_VERIFY(done); QVERIFY(error.contains("outside")); QCOMPARE(server.requests.size(), 0);
        server.respond = [](QByteArray) { return HttpFixture::Response{QByteArray(20000, 'a')}; }; done = false;
        api.bytes("/api/art", [&](QByteArray bytes, QString e) { QVERIFY(bytes.size() <= 1025); error = e; done = true; }, 1024);
        QTRY_VERIFY(done); QVERIFY(error.contains("size limit"));
    }
    void staleLibraryRepliesAndPagination() {
        HttpFixture server;
        server.respond = [](QByteArray req) {
            if (req.contains("q=new")) return HttpFixture::Response{"{\"albums\":[{\"id\":\"2\",\"title\":\"New\"}],\"total\":1}"};
            if (req.contains("offset=200")) return HttpFixture::Response{"{\"albums\":[],\"total\":5001}"};
            return HttpFixture::Response{"{\"albums\":[{\"id\":\"1\",\"title\":\"Stale\"}],\"total\":5001}", 160};
        };
        ApiClient api(server.url()); LibraryModel model(&api);
        model.reload(); model.reload({}, "title", "new");
        QTRY_COMPARE(model.rowCount(), 1); QCOMPARE(model.album(0)->id, QString("2"));
        QTest::qWait(200); QCOMPARE(model.album(0)->id, QString("2"));
        QCOMPARE(model.total(), 1);
        model.reload(); QTRY_VERIFY(!model.loading()); QCOMPARE(model.total(), 5001);
        QCOMPARE(model.rowCount(), 1); // Display total is independent of loaded row count.
        auto *temporary = new LibraryModel(&api); temporary->reload(); delete temporary;
        QTest::qWait(200); // Pending callbacks must not dereference a deleted model.
    }
    void libraryRecoversWhenBackendStartsLater() {
        HttpFixture server; int requests = 0;
        server.respond = [&requests](QByteArray) {
            if (++requests == 1) return HttpFixture::Response{"{}", 0, 503};
            return HttpFixture::Response{"{\"albums\":[{\"id\":\"9\",\"title\":\"Ready later\"}],\"total\":1}"};
        };
        ApiClient api(server.url()); LibraryModel library(&api); library.reload();
        QTRY_COMPARE(requests, 1); QTRY_COMPARE_WITH_TIMEOUT(library.rowCount(), 1, 5000);
        QCOMPARE(library.album(0)->id, QString("9")); QCOMPARE(requests, 2);
    }
    void paginatesBeyondFiveThousand() {
        HttpFixture server;
        server.respond = [](QByteArray req) {
            const QUrl url("http://localhost" + QString::fromUtf8(req.split(' ')[1]));
            const int offset = QUrlQuery(url).queryItemValue("offset").toInt();
            QJsonArray rows;
            for (int i = offset; i < qMin(5001, offset + 200); ++i) rows.append(QJsonObject{{"id", QString::number(i)}, {"title", "Album"}});
            return HttpFixture::Response{QJsonDocument(QJsonObject{{"albums", rows}, {"total", 5001}}).toJson()};
        };
        ApiClient api(server.url()); LibraryModel model(&api); model.reload();
        while (model.rowCount() < 5001) { QTRY_VERIFY(!model.loading()); model.loadNext(); }
        QTRY_VERIFY(!model.loading()); QCOMPARE(model.rowCount(), 5001); QCOMPARE(model.album(5000)->id, QString("5000"));
        const int requests = server.requests.size(); model.loadNext(); QTest::qWait(50); QCOMPARE(server.requests.size(), requests);
    }
    void imageDecodeAndBudget() {
        QImage image(420, 420, QImage::Format_RGB32); image.fill(Qt::blue);
        QByteArray png; QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly); QVERIFY(image.save(&buffer, "PNG"));
        const auto art = ArtworkCache::decode(png, 128);
        QCOMPARE(art.cover.size(), QSize(128, 128)); QCOMPARE(art.reflection.height(), 64);
        QVERIFY(ArtworkCache::decode("not an image").cover.isNull());
        HttpFixture server; ApiClient api(server.url()); ArtworkCache cache(&api, 4);
        for (int i = 0; i < 5000; ++i) cache.insert(QString::number(i), art);
        QVERIFY(cache.bytesUsed() <= cache.budgetBytes()); QVERIFY(!cache.find("0")); QVERIFY(cache.find("4999"));
    }
    void backendSvgRgbaPaintRetainsItsColorAndAlpha() {
        const QByteArray svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"128\" height=\"128\">"
            "<rect width=\"128\" height=\"128\" fill=\"rgba(255,255,255,.86)\"/></svg>";
        const auto artwork = ArtworkCache::decode(svg, 128);
        QVERIFY(!artwork.cover.isNull());
        const auto color = artwork.cover.pixelColor(64, 64);
        QVERIFY(color.red() > 240 && color.green() > 240 && color.blue() > 240);
        QVERIFY(color.alpha() >= 215 && color.alpha() <= 225);
    }
    void artworkWorkIsBoundedAndDestructionSafe() {
        HttpFixture server; server.respond = [](QByteArray) { return HttpFixture::Response{"bad image", 60}; };
        ApiClient api(server.url()); auto *cache = new ArtworkCache(&api, 4);
        QStringList keys; for (int i = 0; i < 5000; ++i) keys.append("/api/art?id=" + QString::number(i));
        cache->prefetch(keys); QVERIFY(cache->activeJobs() <= 2);
        QTest::qWait(250); QVERIFY(server.requests.size() <= cache->prefetchCapacity()); delete cache;
        QTest::qWait(200);
    }
    void lowMemoryPrefetchSettlesWithoutEvictionLoop() {
        QImage image(420, 420, QImage::Format_RGB32); image.fill(Qt::darkBlue);
        QByteArray png; QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly); QVERIFY(image.save(&buffer, "PNG"));
        HttpFixture server; server.respond = [png](QByteArray) { return HttpFixture::Response{png}; };
        ApiClient api(server.url()); ArtworkCache cache(&api, 4);
        QStringList keys; for (int i = 0; i < 20; ++i) keys.append("/api/art?id=" + QString::number(i));
        cache.prefetch(keys); QTRY_COMPARE(server.requests.size(), cache.prefetchCapacity()); QTRY_COMPARE(cache.activeJobs(), 0);
        const auto requests = server.requests.size(); QTest::qWait(120);
        QCOMPARE(server.requests.size(), requests);
        QVERIFY(cache.bytesUsed() <= cache.budgetBytes());
    }
    void coverNavigationAndIdlePainting() {
        HttpFixture server; ApiClient api(server.url()); LibraryModel library(&api); library.setDemo();
        ArtworkCache cache(&api); CoverFlowWidget widget(&library, &cache);
        widget.resize(800, 400); widget.show(); widget.setFocus();
        QTest::qWait(50); QVERIFY(!widget.animating());
        QTest::keyClick(&widget, Qt::Key_Right); QVERIFY(widget.animating());
        QTRY_VERIFY(!widget.animating()); QCOMPARE(widget.currentIndex(), 1);
        widget.select(9); widget.select(12); QTRY_VERIFY(!widget.animating()); QCOMPARE(widget.currentIndex(), 12);
        QTest::qWait(50); const auto paints = widget.paintCount(); QTest::qWait(150);
        QCOMPARE(widget.paintCount(), paints);
        widget.setReducedMotion(true); widget.select(4); QVERIFY(!widget.animating()); QCOMPARE(widget.currentIndex(), 4);
        QSignalSpy opened(&widget, &CoverFlowWidget::activated); QTest::keyClick(&widget, Qt::Key_Return); QCOMPARE(opened.count(), 1);
        const auto frame = widget.grab().toImage(); QVERIFY(!frame.isNull());
        // The 260 px centre cover spans x=270 to x=530. The neighbouring
        // covers should be clickable immediately beyond either edge.
        QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(538, 150));
        QCOMPARE(widget.currentIndex(), 5);
        widget.select(4);
        widget.repaint();
        QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(262, 150));
        QCOMPARE(widget.currentIndex(), 3);
    }
    void trackpadBurstKeepsPaintingBeforeTheGestureEnds() {
        HttpFixture server;
        ApiClient api(server.url());
        LibraryModel library(&api);
        library.setDemo();
        ArtworkCache cache(&api);
        CoverFlowWidget widget(&library, &cache);
        widget.resize(800, 400);
        widget.show();
        auto scroll = [&widget](QPoint pixels, QPoint angle, Qt::ScrollPhase phase) {
            QWheelEvent event(QPointF(400, 150), QPointF(400, 150), pixels, angle,
                              Qt::NoButton, Qt::NoModifier, phase, false);
            QCoreApplication::sendEvent(&widget, &event);
        };
        QTest::qWait(40);
        const auto paints = widget.paintCount();
        scroll({0, -13}, {}, Qt::ScrollBegin);
        QCOMPARE(widget.currentIndex(), 0);
        // Span several 33 ms Linux paint ticks while packets are still arriving.
        for (int packet = 1; packet < 25; ++packet) {
            QTest::qWait(5);
            scroll({0, -13}, {}, Qt::ScrollUpdate);
        }
        QCOMPARE(widget.currentIndex(), 5);
        QVERIFY(widget.animating());
        QVERIFY(widget.paintCount() > paints);
        QVERIFY(widget.visualPosition() > 0. && widget.visualPosition() < 6.);
        scroll({0, -13}, {}, Qt::ScrollMomentum);
        scroll({}, {}, Qt::ScrollEnd);
        QTRY_VERIFY(!widget.animating());
        QCOMPARE(widget.visualPosition(), 6.);
        scroll({}, {0, 120}, Qt::NoScrollPhase);
        QCOMPARE(widget.currentIndex(), 5);
        QTRY_VERIFY(!widget.animating());

        scroll({0, -200}, {}, Qt::ScrollBegin);
        for (int packet = 1; packet < 5; ++packet) {
            QTest::qWait(9);
            scroll({0, -200}, {}, Qt::ScrollUpdate);
        }
        scroll({}, {}, Qt::ScrollEnd);
        QCOMPARE(widget.currentIndex(), 11);
        QVERIFY(widget.animating());
        QTRY_VERIFY(!widget.animating());
        QCOMPARE(widget.visualPosition(), 11.);
    }
    void coverStackFitsViewport() {
        HttpFixture server;
        ApiClient api(server.url());
        LibraryModel library(&api);
        library.setDemo();
        ArtworkCache cache(&api);
        CoverFlowWidget widget(&library, &cache);
        widget.resize(480, 400);
        widget.select(7, false);
        const int narrowCount = widget.visibleCoverCount();
        widget.resize(1440, 400);
        QVERIFY(widget.visibleCoverCount() > narrowCount);
        widget.resize(480, 400);
        QCOMPARE(widget.visibleCoverCount(), narrowCount);
        QCOMPARE(widget.currentIndex(), 7);

    }
    void firstNeighboursTurnAndReflectOnTheSameFloor() {
        const auto centre = projectCover(coverPose(0.), 100., {});
        const auto right = projectCover(coverPose(1.), 100., {});
        const auto left = projectCover(coverPose(-1.), 100., {});
        QVERIFY(right.face[0].x() < centre.face[1].x());
        QVERIFY(right.face[1].x() - centre.face[1].x() > 60.);
        QVERIFY(right.face[2].y() > 97. && right.face[2].y() < 100.);
        QVERIFY(right.face[3].y() < 88.);
        QCOMPARE(right.face[2].y(), left.face[3].y());
        QCOMPARE(right.face[3].y(), left.face[2].y());
        for (const auto &card : {left, centre, right}) {
            for (const int edge : {0, 1}) {
                const auto foot = card.face[3 - edge];
                const auto reflection = card.reflection[edge];
                QCOMPARE(reflection.x(), foot.x());
                QVERIFY(reflection.y() > foot.y() && reflection.y() - foot.y() < 1.);
                const double height = card.face[3 - edge].y() - card.face[edge].y();
                const double reflectedHeight = card.reflection[3 - edge].y() - reflection.y();
                QVERIFY(qAbs(reflectedHeight / height - .5) < .001);
            }
        }
    }
    void lightweightModePaintsAndMonitorStopsWhenOff() {
        HttpFixture server;
        MainWindow window(server.url(), true);
        window.show();
        auto *flow = window.coverFlow();
        QCOMPARE(flow->animationMode(), AnimationMode::Full);
        flow->setAnimationMode(AnimationMode::Lightweight);
        auto *monitor = window.findChild<PerformanceMonitor *>();
        QVERIFY(monitor);
        QCOMPARE(monitor->mode(), MonitorMode::Off);
        monitor->setMode(MonitorMode::Detailed);
        QVERIFY(monitor->text().contains("0 FPS (idle)"));
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("test.csv");
        monitor->startRecording(path);
        flow->select(2);
        QVERIFY(flow->animating());
        QTest::qWait(180);
        const auto active = flow->takeFrameSample();
        QVERIFY(active.count > 0);
        QVERIFY(active.activeNs > 0 && active.peakNs > 0);
        QTRY_VERIFY(!flow->animating());
        QTest::qWait(1100);
        monitor->setMode(MonitorMode::Off);
        QVERIFY(!monitor->isVisible() && !monitor->recording());
        QFile report(path);
        QVERIFY(report.open(QIODevice::ReadOnly));
        const auto csv = report.readAll();
        QVERIFY(csv.contains("animation_fps,frame_count,active_animation_ms"));
        QVERIFY(csv.count('\n') >= 2);
        flow->select(1);
        QTest::qWait(120);
        QCOMPARE(flow->takeFrameSample().count, 0);
    }
    void motionTurnsFirstAndGlidesToTheRestingGeometry() {
        const auto side = coverPose(1.);
        const auto center = coverPose(0.);
        const auto incoming = animateCover(side, center, .28);
        QCOMPARE(incoming.yaw, 0.);
        QVERIFY(incoming.x > 0.);
        QVERIFY(incoming.depth > 0.);
        double lastX = side.x;
        for (int ms = 0; ms <= 420; ++ms) {
            const auto pose = animateCover(side, center, ms / 1000.);
            QVERIFY(pose.x <= lastX && pose.x >= center.x);
            QVERIFY(qAbs(pose.depth / side.depth - pose.x / side.x) < .00001);
            lastX = pose.x;
        }
        const auto settled = animateCover(side, center, coverMoveDuration);
        QCOMPARE(projectCover(settled, 100., {}).face, projectCover(center, 100., {}).face);
        QCOMPARE(projectCover(settled, 100., {}).reflection, projectCover(center, 100., {}).reflection);

        const auto departure = animateCover(center, coverPose(-1.), .12);
        const auto retarget = animateCover(departure, coverPose(-2.), 0.);
        QCOMPARE(retarget.x, departure.x);
        QCOMPARE(retarget.yaw, departure.yaw);
        QCOMPARE(retarget.depth, departure.depth);
        QCOMPARE(projectCover(retarget, 100., {}).face, projectCover(departure, 100., {}).face);
    }
    void nearerReflectionHidesItsNeighbour() {
        HttpFixture server;
        server.respond = [](const QByteArray &) {
            return HttpFixture::Response{R"({"albums":[
                {"id":"1","title":"Far left","artUrl":"/api/art?id=1"},
                {"id":"2","title":"Near left","artUrl":"/api/art?id=2"},
                {"id":"3","title":"Centre","artUrl":"/api/art?id=3"},
                {"id":"4","title":"Near right","artUrl":"/api/art?id=4"},
                {"id":"5","title":"Far right","artUrl":"/api/art?id=5"}],"total":5})"};
        };
        ApiClient api(server.url());
        LibraryModel library(&api);
        library.reload();
        QTRY_COMPARE(library.rowCount(), 5);
        ArtworkCache cache(&api);
        for (int row = 0; row < library.rowCount(); ++row) {
            QColor color = row == 0 || row == 4 ? Qt::red : Qt::green;
            QImage cover(16, 16, QImage::Format_ARGB32_Premultiplied);
            cover.fill(color);
            QImage reflection(16, 8, QImage::Format_ARGB32_Premultiplied);
            color.setAlpha(96);
            reflection.fill(color);
            cache.insert(library.album(row)->artUrl, {cover, reflection});
        }
        CoverFlowWidget widget(&library, &cache);
        widget.resize(800, 400);
        widget.select(2, false);
        const auto frame = widget.grab().toImage();
        const auto reflectedPixel = [&frame](int x) {
            const auto scale = frame.devicePixelRatio();
            return frame.pixelColor(qRound(x * scale), qRound(300 * scale));
        };
        // Red is still visible farther out, but cannot tint the translucent green.
        QCOMPARE(reflectedPixel(710), QColor(96, 0, 0));
        QCOMPARE(reflectedPixel(545), QColor(0, 96, 0));
        QCOMPARE(reflectedPixel(630), reflectedPixel(545));
        QCOMPARE(reflectedPixel(170), reflectedPixel(255));
    }
    void glassSamplesArtworkAndPlaybackAddsNoCoverOutline() {
        HttpFixture server;
        server.respond = [](const QByteArray &) {
            return HttpFixture::Response{R"({"albums":[{"id":1,"title":"Cover","artUrl":"/api/art?id=1"}],"total":1})"};
        };
        ApiClient api(server.url());
        LibraryModel library(&api);
        library.reload();
        QTRY_COMPARE(library.rowCount(), 1);
        ArtworkCache cache(&api);
        QImage cover(16, 16, QImage::Format_ARGB32_Premultiplied);
        for (int x = 0; x < cover.width(); ++x) {
            for (int y = 0; y < cover.height(); ++y)
                cover.setPixelColor(x, y, x < 8 ? Qt::red : Qt::green);
        }
        cache.insert(library.album(0)->artUrl, {cover, {}});
        CoverFlowWidget widget(&library, &cache);
        widget.resize(800, 400);
        widget.show();
        const QRect area = widget.centreCoverRect().adjusted(-3, -3, 3, 3).intersected(widget.rect());
        const auto before = widget.grab(area).toImage();
        PlayerState playback;
        playback.state = "play";
        playback.track.file = "a/song.flac";
        playback.track.albumId = "1";
        widget.setPlayback(playback);
        QCOMPARE(widget.grab(area).toImage(), before);

        FrostedPanel panel(&widget, QColor(10, 10, 20, 155));
        panel.setGeometry(350, 30, 100, 90);
        panel.show();
        const auto center = [&] {
            const auto image = panel.grab().toImage();
            return image.pixelColor(image.width() / 2, image.height() / 2);
        };
        // A matte fill would have no saturated red or green. The source has
        // a hard split, so seeing both colours at the split also tests blur.
        QTRY_VERIFY(center().red() >= 25 && center().green() >= 25);

        FrostedMenu menu(&widget, &widget);
        menu.addAction("Menu");
        menu.popup(widget.mapToGlobal(QPoint(420, 100)));
        QVERIFY(!menu.mask().isEmpty());
        QVERIFY(!menu.mask().contains(QPoint(0, 0)));
        QVERIFY(menu.mask().contains(menu.rect().center()));
        const auto backdrop = [&] {
            const auto image = menu.grab().toImage();
            return image.pixelColor(image.width() / 2, qRound(7 * image.devicePixelRatio()));
        };
        QTRY_VERIFY(backdrop().green() > backdrop().red() + 30);
        menu.hide();
    }
};
QTEST_MAIN(NativeTests)
#include "NativeTests.moc"
