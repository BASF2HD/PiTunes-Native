#pragma once

#include <QDialog>
#include <QPointer>
#include <QSet>
#include "artwork/ArtworkCache.h"

class QLabel;
class QLineEdit;
class QListView;
class QPushButton;
class QStandardItemModel;

namespace PiTunes {
class RadioDialog : public QDialog {
    Q_OBJECT
public:
    RadioDialog(ApiClient *api, QWidget *parent);

signals:
    void playRequested(QJsonObject station);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void loadStations(const QString &query, int offset = 0);
    void playSelected();
    void saveSelected();
    void syncActions();
    void prefetchVisible();
    void rememberSaved(const QJsonObject &station);
    bool isSaved(const QJsonObject &station) const;

    ApiClient *m_api;
    ArtworkCache m_artwork;
    QLineEdit *m_query;
    QListView *m_list;
    QStandardItemModel *m_model;
    QLabel *m_status;
    QPushButton *m_play, *m_save, *m_more;
    QPointer<QNetworkReply> m_listReply;
    QSet<QString> m_savedUrls, m_savedUuids;
    QString m_search;
    quint64 m_generation = 0;
    int m_nextOffset = 0;
    bool m_saving = false;
};
} // namespace PiTunes
