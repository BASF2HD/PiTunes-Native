#pragma once

#include "networking/ApiClient.h"
#include <QJsonArray>
#include <QTimer>
#include <memory>

namespace PiTunes {
class StandaloneBackend : public QObject {
    Q_OBJECT
public:
    explicit StandaloneBackend(QString initialFolder = {}, QObject *parent = nullptr,
                               QString indexPath = {}, bool silentOutput = false);
    ~StandaloneBackend() override;
    ApiClient::LocalDispatch transport();

signals:
    void libraryChanged();

private:
    struct Data;
    std::unique_ptr<Data> m_data;
    QString m_folder;
    QString m_index;
    bool m_scanning = false;
    QTimer m_finishPlayback;
    void scan();
    void dispatch(const QUrl &url, bool mutation, const QByteArray &body, ApiClient::BytesCallback callback);
    QJsonObject query(const QUrl &url);
    QJsonObject mutate(const QString &path, const QJsonObject &body);
    QJsonObject status() const;
    void play(int position);
    void stop();
};
}
