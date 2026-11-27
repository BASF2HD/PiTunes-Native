#pragma once
#include "Domain.h"
#include "networking/ApiClient.h"
#include <QAbstractListModel>
#include <QTimer>

namespace PiTunes {
class LibraryModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit LibraryModel(ApiClient *api, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    const Album *album(int row) const;
    void reload(QString filter = {}, QString sort = "year-desc", QString search = {});
    void loadNext();
    int total() const { return m_total; }
    QString filter() const { return m_filter; }
    QString sortMode() const { return m_sort; }
    bool loading() const { return m_loading; }
    void setDemo();
signals:
    void error(QString message);
    void pageLoaded();
private:
    ApiClient *m_api;
    QVector<Album> m_albums;
    QString m_filter, m_sort, m_search;
    quint64 m_generation = 0;
    int m_total = 0;
    bool m_loading = false;
    QTimer m_retry;
    int m_retryDelayMs = 3000;
};
} // namespace PiTunes
