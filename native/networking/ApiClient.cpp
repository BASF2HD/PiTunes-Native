#include "ApiClient.h"
#include "models/Domain.h"
#include <QJsonDocument>
#include <QJsonParseError>
#include <QTimer>
#include <memory>

namespace PiTunes {
ApiClient::ApiClient(QUrl base, QObject *parent) : QObject(parent), m_network(this), m_base(std::move(base)) {}
QString ApiClient::payloadError(const QJsonObject &o) {
    if (o.contains("error") && !textValue(o["error"]).isEmpty()) return textValue(o["error"]);
    if (o.contains("ok") && !boolValue(o["ok"])) {
        const auto message = textValue(o["message"]);
        return message.isEmpty() ? "The operation was rejected." : message;
    }
    return {};
}
static ApiClient::BytesCallback jsonCallback(ApiClient::JsonCallback callback) {
    return [callback = std::move(callback)](QByteArray bytes, QString error) {
        QJsonParseError parse;
        const auto doc = QJsonDocument::fromJson(bytes, &parse);
        if (error.isEmpty() && (parse.error != QJsonParseError::NoError || !doc.isObject()))
            error = "The API returned invalid JSON.";
        const auto object = doc.object();
        if (error.isEmpty()) error = ApiClient::payloadError(object);
        callback(object, error);
    };
}
QNetworkReply *ApiClient::get(const QString &path, JsonCallback callback, int timeoutMs) {
    return request(path, nullptr, jsonCallback(std::move(callback)), timeoutMs, 2 * 1024 * 1024);
}
QNetworkReply *ApiClient::post(const QString &path, const QJsonObject &body, JsonCallback callback, int timeoutMs) {
    const auto data = QJsonDocument(body).toJson(QJsonDocument::Compact);
    return request(path, &data, jsonCallback(std::move(callback)), timeoutMs, 2 * 1024 * 1024);
}
QNetworkReply *ApiClient::bytes(const QString &path, BytesCallback callback, int maxBytes) {
    return request(path, nullptr, std::move(callback), 10000, maxBytes);
}
QNetworkReply *ApiClient::request(const QString &path, const QByteArray *body, BytesCallback callback,
                                 int timeoutMs, int maxBytes) {
    const auto url = m_base.resolved(QUrl(path));
    if (url.scheme() != m_base.scheme() || url.host() != m_base.host() || url.port() != m_base.port()
        || !url.userInfo().isEmpty() || !url.path().startsWith("/api/")) {
        QTimer::singleShot(0, this, [callback] { callback({}, "Rejected an API URL outside the configured server."); });
        return nullptr;
    }
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    req.setRawHeader("Accept", body ? "application/json" : "*/*");
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    auto *reply = body ? m_network.post(req, *body) : m_network.get(req);
    reply->setReadBufferSize(maxBytes + 1);
    auto data = std::make_shared<QByteArray>();
    auto error = std::make_shared<QString>();
    auto *timer = new QTimer(reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, reply, [reply, error] { *error = "The API request timed out."; reply->abort(); });
    connect(reply, &QNetworkReply::readyRead, reply, [reply, data, error, maxBytes] {
        // Read at most the remaining budget plus one byte; never buffer an unbounded response.
        data->append(reply->read(qMax(1, maxBytes + 1 - data->size())));
        if (data->size() > maxBytes || reply->bytesAvailable() > 0) {
            *error = "The API response exceeded the size limit."; reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [reply, data, error, timer, callback = std::move(callback)] {
        timer->stop();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (error->isEmpty() && (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300))
            *error = QString("API request failed (%1): %2").arg(status).arg(reply->errorString());
        callback(*data, *error);
        reply->deleteLater();
    });
    timer->start(timeoutMs);
    return reply;
}
} // namespace PiTunes
