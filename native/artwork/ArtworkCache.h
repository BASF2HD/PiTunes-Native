#pragma once
#include "networking/ApiClient.h"
#include <QCache>
#include <QImage>
#include <QSet>
#include <QThreadPool>
#include <QVector>

namespace PiTunes {
struct Artwork { QImage cover, reflection; };
class ArtworkCache : public QObject {
    Q_OBJECT
public:
    explicit ArtworkCache(ApiClient *api, int budgetMiB = 12, QObject *parent = nullptr);
    ~ArtworkCache() override;
    const Artwork *find(const QString &key) const;
    // Only the most recent visible window can enqueue work. At most two network/decode jobs.
    void prefetch(const QStringList &keys);
    int bytesUsed() const { return m_cache.totalCost(); }
    int budgetBytes() const { return m_cache.maxCost(); }
    int activeJobs() const { return m_active.size(); }
    static Artwork decode(const QByteArray &data, int size = 320);
    void insert(const QString &key, Artwork artwork);
signals:
    void ready(QString key);
private:
    void pump();
    ApiClient *m_api;
    mutable QCache<QString, Artwork> m_cache;
    QThreadPool m_workers;
    QStringList m_desired;
    QSet<QString> m_active, m_failed;
    int m_edge = 320;
};
} // namespace PiTunes
