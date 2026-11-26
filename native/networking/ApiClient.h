#pragma once
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <functional>

namespace PiTunes {
class ApiClient : public QObject {
    Q_OBJECT
public:
    using JsonCallback = std::function<void(QJsonObject, QString)>;
    using BytesCallback = std::function<void(QByteArray, QString)>;
    explicit ApiClient(QUrl base, QObject *parent = nullptr);
    QUrl baseUrl() const { return m_base; }
    QNetworkReply *get(const QString &path, JsonCallback callback, int timeoutMs = 10000);
    QNetworkReply *post(const QString &path, const QJsonObject &body, JsonCallback callback,
                        int timeoutMs = 30000);
    QNetworkReply *bytes(const QString &path, BytesCallback callback, int maxBytes = 4 * 1024 * 1024);
    static QString payloadError(const QJsonObject &object);
private:
    QNetworkReply *request(const QString &path, const QByteArray *body, BytesCallback callback,
                           int timeoutMs, int maxBytes);
    QNetworkAccessManager m_network;
    QUrl m_base;
};
} // namespace PiTunes
