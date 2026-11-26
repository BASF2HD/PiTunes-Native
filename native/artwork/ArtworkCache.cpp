#include "ArtworkCache.h"
#include <QBuffer>
#include <QFutureWatcher>
#include <QImageReader>
#include <QLinearGradient>
#include <QPainter>
#include <QPointer>
#include <QSvgRenderer>
#include <QUrlQuery>
#include <QtConcurrent>

namespace PiTunes {
ArtworkCache::ArtworkCache(ApiClient *api, int budgetMiB, QObject *parent)
    : QObject(parent), m_api(api), m_cache(qBound(4, budgetMiB, 32) * 1024 * 1024) {
    m_edge = budgetMiB <= 6 ? 256 : 320;
    m_workers.setMaxThreadCount(2);
    m_workers.setExpiryTimeout(5000);
}
ArtworkCache::~ArtworkCache() { m_workers.waitForDone(); }
const Artwork *ArtworkCache::find(const QString &key) const { return m_cache.object(key); }
void ArtworkCache::prefetch(const QStringList &keys) {
    // A desired window must itself fit in cache, or eviction would continuously
    // refetch offscreen artwork even after navigation stopped.
    const int capacity = qMax(1, m_cache.maxCost() / (m_edge * m_edge * 6));
    m_desired = keys.mid(0, qMin(11, capacity));
    // Negative cache is bounded to the current window, so failures cannot grow with the library.
    m_failed.intersect(QSet<QString>(m_desired.begin(), m_desired.end()));
    pump();
}
void ArtworkCache::insert(const QString &key, Artwork artwork) {
    const auto cost = artwork.cover.sizeInBytes() + artwork.reflection.sizeInBytes();
    if (cost > 0 && cost <= m_cache.maxCost()) m_cache.insert(key, new Artwork(std::move(artwork)), int(cost));
}
Artwork ArtworkCache::decode(const QByteArray &data, int size) {
    Artwork result;
    if (data.isEmpty() || data.size() > 4 * 1024 * 1024) return result;
    const int edge = qBound(64, size, 420);
    QImage decoded;
    if (data.trimmed().startsWith('<')) {
        QSvgRenderer svg(data);
        if (!svg.isValid()) return result;
        decoded = QImage(edge, edge, QImage::Format_ARGB32_Premultiplied); decoded.fill(Qt::transparent);
        QPainter painter(&decoded); svg.render(&painter);
    } else {
        QBuffer buffer; buffer.setData(data); buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer);
        const auto original = reader.size();
        // Reject corrupt/oversized inputs before plugins allocate a full decoded bitmap.
        if (!original.isValid() || qint64(original.width()) * original.height() > 4096 * 1024) return result;
        reader.setScaledSize(original.scaled(edge, edge, Qt::KeepAspectRatio));
        reader.setAutoTransform(true); decoded = reader.read();
    }
    if (decoded.isNull()) return result;
    result.cover = decoded.scaled(edge, edge, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                           .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    result.reflection = result.cover.mirrored(false, true).copy(0, 0, result.cover.width(), result.cover.height() / 2);
    QPainter painter(&result.reflection);
    painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    QLinearGradient fade(0, 0, 0, result.reflection.height());
    fade.setColorAt(0, QColor(0, 0, 0, 95)); fade.setColorAt(1, QColor(0, 0, 0, 0));
    painter.fillRect(result.reflection.rect(), fade);
    return result;
}
void ArtworkCache::pump() {
    for (const auto &key : m_desired) {
        if (m_active.size() >= 2) break;
        if (key.isEmpty() || m_cache.contains(key) || m_active.contains(key) || m_failed.contains(key)) continue;
        m_active.insert(key);
        QPointer<ArtworkCache> guard(this);
        QUrl url(key);
        if (url.path() == "/api/art") {
            QUrlQuery query(url); query.removeAllQueryItems("size"); query.addQueryItem("size", "420"); url.setQuery(query);
        }
        m_api->bytes(url.toString(QUrl::FullyEncoded), [this, guard, key](QByteArray bytes, QString error) {
            if (!guard) return;
            if (!error.isEmpty() || !m_desired.contains(key)) {
                m_active.remove(key); m_failed.insert(key); pump(); return;
            }
            auto *watcher = new QFutureWatcher<Artwork>(this);
            connect(watcher, &QFutureWatcher<Artwork>::finished, this, [this, key, watcher] {
                auto artwork = watcher->result();
                m_active.remove(key);
                if (m_desired.contains(key)) {
                    if (artwork.cover.isNull()) { m_failed.insert(key); }
                    else { insert(key, std::move(artwork)); }
                    emit ready(key);
                }
                watcher->deleteLater(); pump();
            });
            const int edge = m_edge;
            watcher->setFuture(QtConcurrent::run(&m_workers, [bytes = std::move(bytes), edge] { return decode(bytes, edge); }));
        });
    }
}
} // namespace PiTunes
