#include "LibraryModel.h"
#include <QCollator>
#include <QHash>
#include <QJsonArray>
#include <QPointer>
#include <QUrlQuery>
#include <algorithm>

namespace PiTunes {
bool CoverEntry::matchesPlayback(const PlayerState &player) const {
    if (player.track.file.isEmpty()) return false;
    if (kind == StationCover) return player.source == "radio" && radioStreamUrl(station) == player.track.file;
    if (player.source == "radio") return false;
    return kind == TrackCover ? track.file == player.track.file : !face.id.isEmpty() && face.id == player.track.albumId;
}

LibraryModel::LibraryModel(ApiClient *api, QObject *parent) : QAbstractListModel(parent), m_api(api) {
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, &LibraryModel::loadNext);
}
int LibraryModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_rows.size(); }
QVariant LibraryModel::data(const QModelIndex &index, int role) const {
    const auto *a = album(index.row());
    if (!index.isValid() || !a) return {};
    if (role == Qt::DisplayRole) return a->title + " — " + a->artist;
    if (role == Qt::UserRole) return a->id;
    return {};
}
const CoverEntry *LibraryModel::entry(int row) const {
    return row >= 0 && row < m_rows.size() ? &m_rows[row] : nullptr;
}
const Album *LibraryModel::album(int row) const {
    const auto *rowEntry = entry(row);
    return rowEntry ? &rowEntry->face : nullptr;
}
void LibraryModel::reload(QString filter, QString sort, QString search) {
    m_kind = CoverEntry::AlbumCover;
    m_path = "/api/library/albums";
    m_filter = std::move(filter);
    m_sort = std::move(sort);
    m_search = std::move(search);
    resetShelf();
}
void LibraryModel::reloadTracks(QString path, QString sort) {
    m_kind = CoverEntry::TrackCover;
    m_path = std::move(path);
    m_sort = std::move(sort);
    m_filter.clear();
    m_search.clear();
    resetShelf();
}
void LibraryModel::reloadRadio() {
    m_kind = CoverEntry::StationCover;
    m_path = "/api/library/radio";
    m_sort.clear();
    m_filter.clear();
    m_search.clear();
    resetShelf();
}
void LibraryModel::search(QString query) {
    if (query == m_search) return;
    m_search = std::move(query);
    resetShelf();
}
void LibraryModel::resetShelf() {
    ++m_generation;
    m_retry.stop(); m_retryDelayMs = 3000;
    m_loading = false;
    beginResetModel(); m_rows.clear(); m_total = 0; endResetModel();
    loadNext();
}
void LibraryModel::loadNext() {
    if (m_loading || (!m_rows.isEmpty() && m_rows.size() >= m_total)) return;
    m_loading = true;
    const auto generation = m_generation;
    const bool searching = !m_search.isEmpty();
    const bool paged = !searching && (m_path == "/api/library/albums" || m_path == "/api/library/tracks");
    QUrlQuery query;
    if (paged) {
        query.addQueryItem("offset", QString::number(m_rows.size()));
        query.addQueryItem("limit", "200");
        query.addQueryItem("filter", m_filter);
        query.addQueryItem("sort", m_sort);
        query.addQueryItem("q", m_search);
    } else if (searching) {
        query.addQueryItem("limit", "120");
        query.addQueryItem("q", m_search);
    }
    const auto path = searching ? QString("/api/search") : m_path;
    const auto url = query.isEmpty() ? path : path + "?" + query.toString(QUrl::FullyEncoded).replace('+', "%2B");
    QPointer<LibraryModel> guard(this);
    m_api->get(url, [this, guard, generation, searching, paged](QJsonObject result, QString message) {
        if (!guard || generation != m_generation) return;
        m_loading = false;
        if (!message.isEmpty()) {
            emit error(message);
            m_retry.start(m_retryDelayMs);
            m_retryDelayMs = qMin(30000, m_retryDelayMs * 2);
            return;
        }
        m_retryDelayMs = 3000;
        QVector<CoverEntry> rows;
        if (searching || m_kind == CoverEntry::TrackCover) {
            const auto tracks = result["tracks"].toArray();
            QHash<QString, QJsonObject> byFile;
            const auto append = [&rows](const QJsonObject &json) {
                auto track = Track::fromJson(json);
                if (track.file.isEmpty()) return;
                Album face{track.file, track.title, track.artist, {}, textValue(json["artUrl"]), track.genre};
                rows.append({CoverEntry::TrackCover, face, track, {}});
            };
            if (result["playlist"].isObject()) {
                for (const auto &value : tracks) {
                    const auto track = value.toObject();
                    byFile.insert(textValue(track["file"]), track);
                }
                for (const auto &file : result["playlist"].toObject()["trackIds"].toArray())
                    if (byFile.contains(textValue(file))) append(byFile.value(textValue(file)));
            } else {
                for (const auto &track : tracks) append(track.toObject());
            }
            if (!searching && !paged && !m_sort.isEmpty()) {
                QCollator collator;
                collator.setCaseSensitivity(Qt::CaseInsensitive);
                std::stable_sort(rows.begin(), rows.end(), [&](const CoverEntry &a, const CoverEntry &b) {
                    if (m_sort != "title" && a.track.year.toInt() != b.track.year.toInt())
                        return m_sort == "year-asc" ? a.track.year.toInt() < b.track.year.toInt()
                                                   : a.track.year.toInt() > b.track.year.toInt();
                    return collator.compare(a.face.title, b.face.title) < 0;
                });
            }
        }
        if (searching || m_kind == CoverEntry::AlbumCover) {
            for (const auto &value : result["albums"].toArray())
                rows.append({CoverEntry::AlbumCover, Album::fromJson(value.toObject()), {}, {}});
        }
        if (!searching && m_kind == CoverEntry::StationCover) {
            for (const auto &value : result["stations"].toArray()) {
                const auto station = value.toObject();
                if (radioStreamUrl(station).isEmpty()) continue;
                Album face{textValue(station["id"]), textValue(station["name"]), textValue(station["genre"]), {}, radioArtworkPath(station)};
                if (face.artist.isEmpty()) face.artist = "Internet radio";
                rows.append({CoverEntry::StationCover, face, {}, station});
            }
        }
        m_total = paged ? int(numberValue(result["total"], m_rows.size() + rows.size())) : rows.size();
        if (!rows.isEmpty()) {
            beginInsertRows({}, m_rows.size(), m_rows.size() + rows.size() - 1);
            m_rows += rows;
            endInsertRows();
        } else {
            m_total = m_rows.size();
            if (m_rows.isEmpty() && m_kind == CoverEntry::AlbumCover && !searching) m_retry.start(10000);
        }
        emit pageLoaded();
    });
}
void LibraryModel::setDemo() {
    ++m_generation; m_loading = false; m_retry.stop();
    beginResetModel(); m_rows.clear();
    for (int i = 0; i < 15; ++i) m_rows.push_back({CoverEntry::AlbumCover,
        {QString::number(i + 1), i == 0 ? "Offline Sessions" : QString("PiTunes Demo %1").arg(i + 1),
        "PiTunes · demonstration", QString::number(2026 - i), {}}, {}, {}});
    m_total = m_rows.size(); endResetModel(); emit pageLoaded();
}
} // namespace PiTunes
