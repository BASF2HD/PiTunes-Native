#pragma once
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace PiTunes {
QString textValue(const QJsonValue &value);
double numberValue(const QJsonValue &value, double fallback = 0);
bool boolValue(const QJsonValue &value);
struct Album {
    QString id, title, artist, year, artUrl;
    static Album fromJson(const QJsonObject &object);
};
struct Track {
    QString file, title, artist, album, albumId, year;
    double duration = 0;
    QString trackNumber;
    static Track fromJson(const QJsonObject &object);
};
struct PlayerState {
    QString source, state, repeatMode;
    Track track;
    double elapsed = 0, duration = 0;
    int volume = -1;
    bool shuffle = false;
    static PlayerState fromJson(const QJsonObject &object);
};
QString timeLabel(double seconds);
QString albumWithYear(const QString &title, const QString &year);
} // namespace PiTunes
