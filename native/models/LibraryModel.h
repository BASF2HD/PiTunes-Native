#pragma once
#include "Domain.h"
#include "networking/ApiClient.h"
#include <QAbstractListModel>
#include <QTimer>

namespace PiTunes {
struct CoverEntry {
    enum Kind { AlbumCover, TrackCover, StationCover };
    Kind kind = AlbumCover;
    Album face;
    Track track;
    QJsonObject station;
    bool matchesPlayback(const PlayerState &player) const;
};

class LibraryModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit LibraryModel(ApiClient *api, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    const Album *album(int row) const;
    const CoverEntry *entry(int row) const;
    void reload(QString filter = {}, QString sort = "year-desc", QString search = {});
    void reloadTracks(QString path = "/api/library/tracks", QString sort = {});
    void reloadRadio();
    void search(QString query);
    void loadNext();
    int total() const { return m_total; }
    QString filter() const { return m_filter; }
    QString sortMode() const { return m_sort; }
    bool isRadioShelf() const { return m_kind == CoverEntry::StationCover; }
    bool loading() const { return m_loading; }
    void setDemo();
signals:
    void error(QString message);
    void pageLoaded();
private:
    ApiClient *m_api;
    void resetShelf();
    QVector<CoverEntry> m_rows;
    CoverEntry::Kind m_kind = CoverEntry::AlbumCover;
    QString m_path = "/api/library/albums";
    QString m_filter, m_sort, m_search;
    quint64 m_generation = 0;
    int m_total = 0;
    bool m_loading = false;
    QTimer m_retry;
    int m_retryDelayMs = 3000;
};
} // namespace PiTunes
