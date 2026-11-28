#include "StandaloneBackend.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QPointer>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUrlQuery>
#include <QUuid>
#include <QtConcurrent>
#include <atomic>
#include <fileref.h>
#include <tag.h>
#include <miniaudio.h>

namespace PiTunes {
namespace {
constexpr int libraryLimit = 50000;
const QStringList audioExtensions{"wav", "flac", "mp3"};
const QStringList artworkNames{"cover.jpg", "folder.jpg", "front.jpg", "cover.png", "folder.png"};

class Database {
public:
    explicit Database(const QString &file)
        : name(QUuid::createUuid().toString()), connection(QSqlDatabase::addDatabase("QSQLITE", name)) {
        connection.setDatabaseName(file);
        connection.setConnectOptions("QSQLITE_BUSY_TIMEOUT=1000");
        connection.open();
    }
    ~Database() {
        connection = {};
        QSqlDatabase::removeDatabase(name);
    }
    QString name;
    QSqlDatabase connection;
};

bool schema(QSqlDatabase &db) {
    QSqlQuery q(db);
    return q.exec("PRAGMA journal_mode=WAL") &&
        q.exec("CREATE TABLE IF NOT EXISTS albums (id INTEGER PRIMARY KEY, folder TEXT UNIQUE, title TEXT, artist TEXT, year INTEGER, art TEXT)") &&
        q.exec("CREATE TABLE IF NOT EXISTS tracks (file TEXT PRIMARY KEY, album_id INTEGER, title TEXT, artist TEXT, track INTEGER, seconds INTEGER, codec TEXT)") &&
        q.exec("CREATE INDEX IF NOT EXISTS tracks_album ON tracks(album_id,track,title)");
}

QString tagValue(const TagLib::String &str) {
    return QString::fromUtf8(str.toCString(true)).trimmed();
}

QString scanLibrary(const QString &index, const QString &root) {
    if (!QDir(root).exists()) return "The music folder is unavailable.";
    Database db(index);
    if (!db.connection.isOpen() || !schema(db.connection) || !db.connection.transaction()) return "Could not open the local music index.";
    QSqlQuery remove(db.connection);
    if (!remove.exec("DELETE FROM tracks")) return remove.lastError().text();
    QSqlQuery album(db.connection), albumId(db.connection), track(db.connection);
    album.prepare("INSERT INTO albums(folder,title,artist,year,art) VALUES(?,?,?,?,?) "
                  "ON CONFLICT(folder) DO UPDATE SET title=excluded.title,artist=excluded.artist,year=excluded.year,art=excluded.art");
    albumId.prepare("SELECT id FROM albums WHERE folder=?");
    track.prepare("INSERT OR IGNORE INTO tracks VALUES(?,?,?,?,?,?,?)");
    QHash<QString, int> indexedFolders;
    QDirIterator entries(root, QDir::Files | QDir::NoSymLinks | QDir::Readable, QDirIterator::Subdirectories);
    int scanned = 0;
    while (entries.hasNext() && scanned < libraryLimit) {
        const auto file = entries.next();
        const auto info = entries.fileInfo();
        const auto suffix = info.suffix().toLower();
        if (!audioExtensions.contains(suffix)) continue;
        ++scanned;
#ifdef Q_OS_WIN
        TagLib::FileRef media(reinterpret_cast<const wchar_t *>(file.utf16()));
#else
        TagLib::FileRef media(QFile::encodeName(file).constData());
#endif
        const auto *tags = media.tag();
        const auto *details = media.audioProperties();
        const auto artist = tags ? tagValue(tags->artist()) : QString();
        const auto title = tags ? tagValue(tags->title()) : QString();
        const auto folder = info.absolutePath();
        int id = indexedFolders.value(folder);
        if (!id) {
            QString art;
            for (const auto &candidate : artworkNames) {
                const auto path = QDir(folder).filePath(candidate);
                if (QFileInfo(path).isFile()) { art = path; break; }
            }
            const auto albumName = tags ? tagValue(tags->album()) : QString();
            album.bindValue(0, folder);
            album.bindValue(1, albumName.isEmpty() ? QDir(folder).dirName() : albumName);
            album.bindValue(2, artist.isEmpty() ? QString("Unknown artist") : artist);
            album.bindValue(3, tags ? int(tags->year()) : 0);
            album.bindValue(4, art.isEmpty() ? QString("") : art);
            if (!album.exec()) return album.lastError().text();
            albumId.bindValue(0, folder);
            if (!albumId.exec() || !albumId.next()) return "Could not identify the indexed album.";
            id = albumId.value(0).toInt();
            albumId.finish();
            indexedFolders.insert(folder, id);
        }
        track.bindValue(0, file);
        track.bindValue(1, id);
        track.bindValue(2, title.isEmpty() ? info.completeBaseName() : title);
        track.bindValue(3, artist.isEmpty() ? QString("Unknown artist") : artist);
        track.bindValue(4, tags ? int(tags->track()) : 0);
        track.bindValue(5, details ? details->length() : 0);
        track.bindValue(6, suffix);
        if (!track.exec()) return track.lastError().text();
    }
    if (!QDir(root).exists()) return "The music folder was disconnected while scanning.";
    if (!db.connection.commit()) return db.connection.lastError().text();
    return {};
}

QJsonObject error(const QString &message) { return {{"error", message}}; }
QJsonObject okay(const QString &message = {}) { return {{"ok", true}, {"message", message}}; }
QString artPath(int album) { return QString("/api/art?album_id=%1").arg(album); }

QJsonObject song(const QSqlQuery &q) {
    return {{"file", q.value(0).toString()}, {"albumId", q.value(1).toInt()}, {"title", q.value(2).toString()},
            {"artist", q.value(3).toString()}, {"trackNumber", q.value(4).toInt()}, {"duration", q.value(5).toInt()},
            {"codec", q.value(6).toString()}, {"album", q.value(7).toString()}, {"year", q.value(8).toInt()},
            {"artUrl", artPath(q.value(1).toInt())}};
}

const char joinedTracks[] = "SELECT t.*,a.title,a.year FROM tracks t JOIN albums a ON a.id=t.album_id";
}

struct StandaloneBackend::Data {
    ma_context context{};
    ma_decoder decoder{};
    ma_device device{};
    bool haveContext = false, haveDecoder = false, haveDevice = false;
    bool playing = false, paused = false, silent = false;
    int position = -1, volume = 60;
    bool shuffle = false;
    double seconds = 0;
    QString repeatMode = "off";
    QString output;
    QString problem;
    QString scanError;
    QStringList queue;
    QJsonObject track;
    std::atomic<ma_uint64> frames{0};
    std::atomic_bool eof{false};

    bool openContext() {
        if (!haveContext && silent) {
            const ma_backend backend = ma_backend_null;
            haveContext = ma_context_init(&backend, 1, nullptr, &context) == MA_SUCCESS;
        } else if (!haveContext) {
#ifdef Q_OS_WIN
            const ma_backend backends[]{ma_backend_wasapi};
            haveContext = ma_context_init(backends, 1, nullptr, &context) == MA_SUCCESS;
#else
            haveContext = ma_context_init(nullptr, 0, nullptr, &context) == MA_SUCCESS;
#endif
        }
        return haveContext;
    }

    static void render(ma_device *device, void *output, const void *, ma_uint32 count) {
        auto *data = static_cast<Data *>(device->pUserData);
        ma_uint64 read = 0;
        const auto result = ma_decoder_read_pcm_frames(&data->decoder, output, count, &read);
        data->frames.fetch_add(read, std::memory_order_relaxed);
        if (result != MA_SUCCESS || read < count) data->eof.store(true, std::memory_order_release);
    }
};

StandaloneBackend::StandaloneBackend(QString initialFolder, QObject *parent, QString indexPath, bool silentOutput)
    : QObject(parent), m_data(std::make_unique<Data>()) {
    m_data->silent = silentOutput;
    QSettings preferences;
    if (!silentOutput) m_data->output = preferences.value("windows/audioOutput").toString();
    m_folder = initialFolder.isEmpty()
        ? preferences.value("windows/musicDirectory", QStandardPaths::writableLocation(QStandardPaths::MusicLocation)).toString()
        : QDir(initialFolder).absolutePath();
    if (indexPath.isEmpty()) {
        const auto storage = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        QDir().mkpath(storage);
        m_index = QDir(storage).filePath("library.sqlite3");
    } else m_index = std::move(indexPath);
    {
        Database db(m_index);
        if (db.connection.isOpen()) schema(db.connection);
    }
    m_finishPlayback.setInterval(100);
    connect(&m_finishPlayback, &QTimer::timeout, this, [this] {
        if (!m_data->playing || !m_data->eof.load(std::memory_order_acquire)) return;
        int following = m_data->repeatMode == "song" ? m_data->position : m_data->position + 1;
        if (following >= m_data->queue.size() && m_data->repeatMode == "all") following = 0;
        if (following < m_data->queue.size()) play(following);
        else stop();
    });
    QTimer::singleShot(0, this, &StandaloneBackend::scan);
}

StandaloneBackend::~StandaloneBackend() {
    stop();
    if (m_data->haveContext) ma_context_uninit(&m_data->context);
}

ApiClient::LocalDispatch StandaloneBackend::transport() {
    return [this](const QUrl &url, bool mutation, QByteArray body, ApiClient::BytesCallback callback) {
        dispatch(url, mutation, body, std::move(callback));
    };
}

void StandaloneBackend::scan() {
    if (m_scanning || m_folder.isEmpty()) return;
    m_scanning = true;
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher] {
        m_scanning = false;
        m_data->scanError = watcher->result();
        watcher->deleteLater();
        emit libraryChanged();
    });
    const QString index = m_index, folder = m_folder;
    watcher->setFuture(QtConcurrent::run([index, folder] { return scanLibrary(index, folder); }));
}

void StandaloneBackend::dispatch(const QUrl &url, bool mutation, const QByteArray &body, ApiClient::BytesCallback callback) {
    if (!mutation && url.path() == "/api/art") {
        QString path;
        {
            Database db(m_index);
            QSqlQuery q(db.connection);
            q.prepare("SELECT art FROM albums WHERE id=?");
            q.addBindValue(QUrlQuery(url).queryItemValue("album_id").toInt());
            if (q.exec() && q.next()) path = q.value(0).toString();
        }
        auto *watcher = new QFutureWatcher<QByteArray>(this);
        connect(watcher, &QFutureWatcher<QByteArray>::finished, this, [watcher, callback = std::move(callback)] {
            const auto bytes = watcher->result();
            watcher->deleteLater();
            callback(bytes, bytes.isEmpty() ? QString("No folder artwork.") : QString());
        });
        watcher->setFuture(QtConcurrent::run([path] {
            QFile file(path);
            return file.size() > 0 && file.size() <= 4 * 1024 * 1024 && file.open(QIODevice::ReadOnly)
                ? file.read(4 * 1024 * 1024 + 1) : QByteArray();
        }));
        return;
    }
    const auto result = mutation ? mutate(url.path(), QJsonDocument::fromJson(body).object()) : query(url);
    callback(QJsonDocument(result).toJson(QJsonDocument::Compact), {});
}

QJsonObject StandaloneBackend::query(const QUrl &url) {
    const auto path = url.path();
    if (path == "/api/player/state") return status();
    if (path == "/api/settings")
        return {{"settings", QJsonObject{{"music_directory", m_folder}, {"storage_source", "local"}}}};
    if (path == "/api/library/favourites") return {{"tracks", QJsonArray{}}, {"albums", QJsonArray{}}};
    if (path == "/api/library/playlists") return {{"playlists", QJsonArray{}}};
    if (path == "/api/audio/devices") {
        QJsonArray devices{QJsonObject{{"alsa", "default"}, {"label", "Windows default output"}}};
#ifdef Q_OS_WIN
        ma_device_info *info = nullptr;
        ma_uint32 count = 0;
        if (!m_data->silent && m_data->openContext() &&
            ma_context_get_devices(&m_data->context, &info, &count, nullptr, nullptr) == MA_SUCCESS) {
            for (ma_uint32 i = 0; i < count; ++i) {
                devices.append(QJsonObject{{"alsa", QString::fromWCharArray(info[i].id.wasapi)},
                                           {"label", QString::fromUtf8(info[i].name)}});
            }
        }
#endif
        return {{"platform", "windows"}, {"devices", devices},
                {"current", QJsonObject{{"device", m_data->output.isEmpty() ? QString("default") : m_data->output}}},
                {"routes", QJsonArray{QJsonObject{{"id", "wasapi"}, {"label", "WASAPI"}}}}};
    }
    const bool albums = path == "/api/library/albums" || path == "/api/search";
    const bool search = path == "/api/search";
    const bool filterTracks = path.startsWith("/api/library/album/") && path.endsWith("/tracks");
    if (!albums && !filterTracks && path != "/api/library/tracks") return error("This feature is unavailable in the standalone prototype.");

    Database db(m_index);
    const QUrlQuery args(url);
    const int offset = qBound(0, args.queryItemValue("offset").toInt(), libraryLimit);
    const int limit = qBound(1, args.queryItemValue("limit").toInt() ? args.queryItemValue("limit").toInt() : 200, 400);
    const auto sort = args.queryItemValue("sort");
    const auto text = "%" + args.queryItemValue("q", QUrl::FullyDecoded).left(140) + "%";
    QJsonArray albumResults, trackResults;
    int total = 0;
    if (albums) {
        const auto filter = args.queryItemValue("filter", QUrl::FullyDecoded);
        const bool artistFilter = filter.startsWith("artist:");
        QString where = " WHERE EXISTS(SELECT 1 FROM tracks t WHERE t.album_id=a.id) AND (a.title LIKE ? OR a.artist LIKE ?)";
        if (artistFilter) where += " AND a.artist=?";
        else if (!filter.isEmpty() && filter != "all") return {{"albums", QJsonArray{}}, {"total", 0}};
        QSqlQuery count(db.connection), page(db.connection);
        count.prepare("SELECT COUNT(*) FROM albums a" + where);
        const QString order = sort == "year-desc" ? "a.year DESC," : sort == "year-asc" ? "a.year ASC," : "";
        page.prepare("SELECT id,title,artist,year FROM albums a" + where + " ORDER BY " + order + "a.title COLLATE NOCASE,id LIMIT ? OFFSET ?");
        for (auto *q : {&count, &page}) {
            q->addBindValue(text); q->addBindValue(text);
            if (artistFilter) q->addBindValue(filter.mid(7));
        }
        page.addBindValue(limit); page.addBindValue(offset);
        if (!count.exec() || !count.next() || !page.exec()) return error("Could not read the standalone music index.");
        total = count.value(0).toInt();
        while (page.next())
            albumResults.append(QJsonObject{{"id", page.value(0).toInt()}, {"title", page.value(1).toString()},
                                            {"artist", page.value(2).toString()}, {"year", page.value(3).toInt()},
                                            {"artUrl", artPath(page.value(0).toInt())}});
    }
    if (!albums || search) {
        const QString where = filterTracks ? " WHERE t.album_id=?" : " WHERE (t.title LIKE ? OR t.artist LIKE ?)";
        QSqlQuery count(db.connection), page(db.connection);
        count.prepare(QString("SELECT COUNT(*) FROM tracks t") + where);
        page.prepare(QString(joinedTracks) + where + " ORDER BY " +
                     (filterTracks ? QString("t.track,t.title") : QString("t.title COLLATE NOCASE,t.file")) + " LIMIT ? OFFSET ?");
        for (auto *q : {&count, &page}) {
            if (filterTracks) q->addBindValue(path.section('/', 4, 4).toInt());
            else { q->addBindValue(text); q->addBindValue(text); }
        }
        page.addBindValue(filterTracks ? 2000 : limit); page.addBindValue(offset);
        if (!count.exec() || !count.next() || !page.exec()) return error("Could not read the standalone tracks.");
        total += count.value(0).toInt();
        while (page.next()) trackResults.append(song(page));
    }
    return {{"albums", albumResults}, {"tracks", trackResults}, {"total", total}, {"offset", offset}, {"limit", limit},
            {"scanning", m_scanning}, {"scanError", m_data->scanError}};
}

QJsonObject StandaloneBackend::mutate(const QString &path, const QJsonObject &body) {
    if (path == "/api/settings" || path == "/api/library/rescan" || path == "/api/library/rebuild-cache") {
        if (m_scanning) return error("A scan is already in progress.");
        if (path == "/api/settings") {
            const auto folder = body["music_directory"].toString().trimmed();
            if (folder.isEmpty() || !QFileInfo(folder).isDir()) return error("Choose an available music folder.");
            m_folder = folder;
            QSettings().setValue("windows/musicDirectory", folder);
        }
        scan();
        return okay("Indexing the music folder.");
    }
    if (path == "/api/audio/output") {
        if (m_data->playing || m_data->paused) return error("Stop playback before switching output.");
        m_data->output = body["alsa"].toString();
        if (m_data->output == "default") m_data->output.clear();
        QSettings().setValue("windows/audioOutput", m_data->output);
        return okay("Audio output saved.");
    }
    m_data->problem.clear();
    if (path == "/api/player/play") {
        const auto file = body["file"].toString();
        if (!file.isEmpty()) {
            QStringList queue;
            for (const auto &value : body["queue"].toArray()) {
                if (queue.size() >= 2000) break;
                if (!value.toString().isEmpty()) queue.append(value.toString());
            }
            int first = queue.indexOf(file);
            if (first < 0) { queue.prepend(file); first = 0; }
            m_data->queue = std::move(queue);
            play(first);
        } else if (m_data->paused && m_data->haveDevice) {
            if (ma_device_start(&m_data->device) != MA_SUCCESS) return error("Could not resume playback.");
            m_data->playing = true;
            m_data->paused = false;
            m_finishPlayback.start();
        } else if (m_data->position >= 0) play(m_data->position);
        else return error("Choose an album or a song first.");
    } else if (path == "/api/player/pause") {
        if (m_data->playing) {
            ma_device_stop(&m_data->device);
            m_data->playing = false;
            m_data->paused = true;
            m_finishPlayback.stop();
        }
    } else if (path == "/api/player/stop") {
        stop();
    } else if (path == "/api/player/previous" || path == "/api/player/next") {
        if (m_data->queue.isEmpty()) return error("The playback queue is empty.");
        int next = m_data->position + (path.endsWith("next") ? 1 : -1);
        if (path.endsWith("next") && m_data->shuffle && m_data->queue.size() > 1) {
            next = (m_data->position + 1 + int(QUuid::createUuid().data4[0] % (m_data->queue.size() - 1))) % m_data->queue.size();
        }
        if (next >= m_data->queue.size() && m_data->repeatMode == "all") next = 0;
        if (next >= m_data->queue.size()) stop();
        else play(qMax(0, next));
    } else if (path == "/api/player/volume") {
        m_data->volume = qBound(0, body["volume"].toInt(), 100);
        if (m_data->haveDevice) ma_device_set_master_volume(&m_data->device, m_data->volume / 100.f);
    } else if (path == "/api/player/seek" && m_data->haveDevice) {
        const auto frame = ma_uint64(qBound(0., body["seconds"].toDouble(), m_data->seconds) * m_data->decoder.outputSampleRate);
        if (m_data->playing) ma_device_stop(&m_data->device);
        const auto rc = ma_decoder_seek_to_pcm_frame(&m_data->decoder, frame);
        if (rc == MA_SUCCESS) m_data->frames.store(frame);
        m_data->eof.store(false);
        if (m_data->playing && ma_device_start(&m_data->device) != MA_SUCCESS) {
            stop();
            return error("Could not resume playback after seeking.");
        }
        if (rc != MA_SUCCESS) return error("Could not seek in this file.");
    } else if (path == "/api/player/options") {
        if (body.contains("repeatMode")) m_data->repeatMode = body["repeatMode"].toString();
        if (body.contains("shuffle")) m_data->shuffle = body["shuffle"].toBool();
    } else return error("This feature is unavailable in the standalone prototype.");
    return m_data->problem.isEmpty() ? status() : error(m_data->problem);
}

QJsonObject StandaloneBackend::status() const {
    const auto &d = *m_data;
    const double elapsed = d.haveDecoder ? double(d.frames.load()) / d.decoder.outputSampleRate : 0;
    return {{"inputSource", "local"},
            {"status", QJsonObject{{"state", d.playing ? "play" : d.paused ? "pause" : "stop"},
                                  {"elapsed", elapsed}, {"duration", d.seconds}, {"playlistlength", d.queue.size()},
                                  {"volume", d.volume}, {"repeat", d.repeatMode != "off"}, {"single", d.repeatMode == "song"},
                                  {"random", d.shuffle}}},
            {"song", d.track}};
}

void StandaloneBackend::stop() {
    auto &d = *m_data;
    m_finishPlayback.stop();
    if (d.haveDevice) { ma_device_uninit(&d.device); d.haveDevice = false; }
    if (d.haveDecoder) { ma_decoder_uninit(&d.decoder); d.haveDecoder = false; }
    d.playing = d.paused = false;
    d.frames.store(0);
    d.eof.store(false);
    d.seconds = 0;
}

void StandaloneBackend::play(int position) {
    auto &d = *m_data;
    stop();
    d.position = position;
    d.track = {};
    const auto file = d.queue.value(position);
    {
        Database db(m_index);
        QSqlQuery tracks(db.connection);
        tracks.prepare(QString(joinedTracks) + " WHERE t.file=?");
        tracks.addBindValue(file);
        if (tracks.exec() && tracks.next()) d.track = song(tracks);
    }
    if (d.track.isEmpty() || !QFileInfo(file).isFile()) {
        d.problem = "This song is unavailable in the selected music folder.";
        return;
    }
    if (!d.openContext()) { d.problem = "Could not initialise the local sound system."; return; }
    auto decoder = ma_decoder_config_init(ma_format_f32, 0, 0);
#ifdef Q_OS_WIN
    const auto opened = ma_decoder_init_file_w(reinterpret_cast<const wchar_t *>(file.utf16()), &decoder, &d.decoder);
#else
    const auto opened = ma_decoder_init_file(QFile::encodeName(file).constData(), &decoder, &d.decoder);
#endif
    if (opened != MA_SUCCESS) { d.problem = "The selected file could not be decoded."; return; }
    d.haveDecoder = true;
    auto config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = d.decoder.outputChannels;
    config.sampleRate = d.decoder.outputSampleRate;
    config.dataCallback = Data::render;
    config.pUserData = &d;
#ifdef Q_OS_WIN
    ma_device_id id{};
    if (!d.output.isEmpty()) {
        if (d.output.size() >= int(sizeof(id.wasapi) / sizeof(id.wasapi[0]))) {
            stop();
            d.problem = "The saved output device ID is invalid.";
            return;
        }
        d.output.toWCharArray(id.wasapi);
        config.playback.pDeviceID = &id;
    }
#endif
    if (ma_device_init(&d.context, &config, &d.device) != MA_SUCCESS) {
        stop();
        d.problem = "Could not open the selected output device.";
        return;
    }
    d.haveDevice = true;
    ma_device_set_master_volume(&d.device, d.volume / 100.f);
    ma_uint64 frames = 0;
    d.seconds = ma_decoder_get_length_in_pcm_frames(&d.decoder, &frames) == MA_SUCCESS
        ? double(frames) / d.decoder.outputSampleRate : d.track["duration"].toDouble();
    if (ma_device_start(&d.device) != MA_SUCCESS) {
        stop();
        d.problem = "Could not start the selected output device.";
        return;
    }
    d.playing = true;
    m_finishPlayback.start();
}
}
