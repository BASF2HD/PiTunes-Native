#include "LibraryModel.h"
#include <QJsonArray>
#include <QUrlQuery>
#include <QPointer>

namespace PiTunes {
LibraryModel::LibraryModel(ApiClient *api, QObject *parent) : QAbstractListModel(parent), m_api(api) {
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, &LibraryModel::loadNext);
}
int LibraryModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_albums.size(); }
QVariant LibraryModel::data(const QModelIndex &index, int role) const {
    const auto *a = album(index.row());
    if (!index.isValid() || !a) return {};
    if (role == Qt::DisplayRole) return a->title + " — " + a->artist;
    if (role == Qt::UserRole) return a->id;
    return {};
}
const Album *LibraryModel::album(int row) const { return row >= 0 && row < m_albums.size() ? &m_albums[row] : nullptr; }
void LibraryModel::reload(QString filter, QString sort, QString search) {
    ++m_generation;
    m_retry.stop(); m_retryDelayMs = 3000;
    m_loading = false;
    m_filter = std::move(filter); m_sort = std::move(sort); m_search = std::move(search);
    beginResetModel(); m_albums.clear(); m_total = 0; endResetModel();
    loadNext();
}
void LibraryModel::loadNext() {
    if (m_loading || (!m_albums.isEmpty() && (m_albums.size() >= m_total || !m_search.isEmpty()))) return;
    m_loading = true;
    const auto generation = m_generation;
    QUrlQuery query;
    query.addQueryItem("offset", QString::number(m_albums.size()));
    query.addQueryItem("limit", m_search.isEmpty() ? "200" : "120");
    query.addQueryItem("filter", m_filter); query.addQueryItem("sort", m_sort);
    query.addQueryItem("q", m_search);
    QPointer<LibraryModel> guard(this);
    m_api->get((m_search.isEmpty() ? "/api/library/albums?" : "/api/search?") + query.toString(QUrl::FullyEncoded),
        [this, guard, generation](QJsonObject object, QString message) {
            if (!guard) return;
            if (generation != m_generation) return;
            m_loading = false;
            if (!message.isEmpty()) {
                emit error(message);
                m_retry.start(m_retryDelayMs); m_retryDelayMs = qMin(30000, m_retryDelayMs * 2);
                return;
            }
            m_retryDelayMs = 3000;
            const auto rows = object["albums"].toArray();
            m_total = m_search.isEmpty() ? int(numberValue(object["total"], rows.size())) : rows.size();
            if (!rows.isEmpty()) {
                beginInsertRows({}, m_albums.size(), m_albums.size() + rows.size() - 1);
                for (const auto &row : rows) m_albums.push_back(Album::fromJson(row.toObject()));
                endInsertRows();
            }
            if (rows.isEmpty()) m_total = m_albums.size(); // Avoid a malformed server causing infinite paging.
            // Mounts and first scans can become ready after a valid empty response.
            // No animation or synchronous loop; retry only the still-empty browse.
            if (m_albums.isEmpty() && m_search.isEmpty()) m_retry.start(10000);
            emit pageLoaded();
        });
}
void LibraryModel::setDemo() {
    ++m_generation; m_loading = false; m_retry.stop();
    beginResetModel(); m_albums.clear();
    for (int i = 0; i < 15; ++i) m_albums.push_back({QString::number(i + 1),
        i == 0 ? "Offline Sessions" : QString("PiTunes Demo %1").arg(i + 1),
        "PiTunes · demonstration", QString::number(2026 - i), {}});
    m_total = m_albums.size(); endResetModel(); emit pageLoaded();
}
} // namespace PiTunes
