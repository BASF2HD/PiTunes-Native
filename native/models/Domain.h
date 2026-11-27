#pragma once
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace PiTunes {
QString textValue(const QJsonValue &value);
double numberValue(const QJsonValue &value, double fallback = 0);
bool boolValue(const QJsonValue &value);
struct CaptionFontSizes {
    static constexpr int minimum = 10;
    static constexpr int maximum = 40;
    int title = 25;
    int artist = 16;
    int album = 16;
};
struct Album {
    QString id, title, artist, year, artUrl;
    QString genre = {};
    static Album fromJson(const QJsonObject &object);
};
struct Track {
    QString file, title, artist, album, albumId, year;
    double duration = 0;
    QString trackNumber;
    QString albumArtist = {}, singer = {}, composer = {}, genre = {}, codec = {};
    double bitRate = 0;
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
