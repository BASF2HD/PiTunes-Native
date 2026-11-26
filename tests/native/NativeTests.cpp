#include "models/Domain.h"
#include "models/LibraryModel.h"
#include "coverflow/CoverFlowWidget.h"
#include "ui/MainWindow.h"
#include <QBuffer>
#include <QDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QUrlQuery>

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
        QCOMPARE(window.findChildren<QDialog *>().size(), 0);
    }
    void demoNeverContactsAnAppliance() {
        HttpFixture server; MainWindow window(server.url(), true); window.show();
        for (auto *button : window.findChildren<QPushButton *>())
            if (button->text() == "Settings") QTest::mouseClick(button, Qt::LeftButton);
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
        QPushButton *play = nullptr, *next = nullptr;
        for (auto *b : window.findChildren<QPushButton *>()) {
            if (b->accessibleName() == "Play or pause") play = b;
            if (b->text() == "⏭") next = b;
        }
        QVERIFY(play); QVERIFY(next); QTRY_COMPARE(play->text(), QString("Ⅱ"));
        bool shuffleChecked = false;
        for (auto *b : window.findChildren<QPushButton *>()) if (b->text() == "Shuffle") shuffleChecked = b->isChecked();
        QVERIFY(shuffleChecked);
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
    void artworkWorkIsBoundedAndDestructionSafe() {
        HttpFixture server; server.respond = [](QByteArray) { return HttpFixture::Response{"bad image", 60}; };
        ApiClient api(server.url()); auto *cache = new ArtworkCache(&api, 4);
        QStringList keys; for (int i = 0; i < 5000; ++i) keys.append("/api/art?id=" + QString::number(i));
        cache->prefetch(keys); QVERIFY(cache->activeJobs() <= 2);
        QTest::qWait(250); QVERIFY(server.requests.size() <= 11); delete cache;
        QTest::qWait(200);
    }
    void lowMemoryPrefetchSettlesWithoutEvictionLoop() {
        QImage image(420, 420, QImage::Format_RGB32); image.fill(Qt::darkBlue);
        QByteArray png; QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly); QVERIFY(image.save(&buffer, "PNG"));
        HttpFixture server; server.respond = [png](QByteArray) { return HttpFixture::Response{png}; };
        ApiClient api(server.url()); ArtworkCache cache(&api, 4);
        QStringList keys; for (int i = 0; i < 11; ++i) keys.append("/api/art?id=" + QString::number(i));
        cache.prefetch(keys); QTRY_VERIFY(server.requests.size() >= 10); QTRY_COMPARE(cache.activeJobs(), 0);
        const auto requests = server.requests.size(); QTest::qWait(120);
        QCOMPARE(server.requests.size(), requests); QVERIFY(requests <= 11);
        QVERIFY(cache.bytesUsed() <= cache.budgetBytes());
    }
    void coverNavigationAndIdlePainting() {
        HttpFixture server; ApiClient api(server.url()); LibraryModel library(&api); library.setDemo();
        ArtworkCache cache(&api); CoverFlowWidget widget(&library, &cache);
        widget.resize(800, 400); widget.show(); widget.setFocus();
        QTest::qWait(50); QVERIFY(!widget.animating()); QVERIFY(widget.visibleCoverCount() <= 9);
        QTest::keyClick(&widget, Qt::Key_Right); QVERIFY(widget.animating());
        QTRY_VERIFY(!widget.animating()); QCOMPARE(widget.currentIndex(), 1);
        widget.select(9); widget.select(12); QTRY_VERIFY(!widget.animating()); QCOMPARE(widget.currentIndex(), 12);
        QTest::qWait(50); const auto paints = widget.paintCount(); QTest::qWait(150);
        QCOMPARE(widget.paintCount(), paints);
        widget.setReducedMotion(true); widget.select(4); QVERIFY(!widget.animating()); QCOMPARE(widget.currentIndex(), 4);
        QSignalSpy opened(&widget, &CoverFlowWidget::activated); QTest::keyClick(&widget, Qt::Key_Return); QCOMPARE(opened.count(), 1);
        const auto frame = widget.grab().toImage(); QVERIFY(!frame.isNull());
    }
};
QTEST_MAIN(NativeTests)
#include "NativeTests.moc"
