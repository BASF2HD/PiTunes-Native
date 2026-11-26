#include "Domain.h"
#include <QtMath>
#include <cmath>

namespace PiTunes {
QString textValue(const QJsonValue &v) {
    if (v.isString()) return v.toString();
    if (v.isDouble()) return QString::number(v.toDouble(), 'g', 16);
    return {};
}
double numberValue(const QJsonValue &v, double fallback) {
    bool ok = false;
    const double value = v.isDouble() ? v.toDouble() : textValue(v).toDouble(&ok);
    return (v.isDouble() || ok) && std::isfinite(value) ? value : fallback;
}
bool boolValue(const QJsonValue &v) {
    return v.isBool() ? v.toBool() : v.isDouble() ? v.toDouble() != 0
        : v.toString() == "1" || v.toString().compare("true", Qt::CaseInsensitive) == 0;
}
static QString first(const QJsonObject &o, std::initializer_list<const char *> keys) {
    for (auto key : keys) { const auto value = textValue(o.value(key)); if (!value.isEmpty()) return value; }
    return {};
}
Album Album::fromJson(const QJsonObject &o) {
    return {first(o, {"id", "albumId"}), first(o, {"title", "album"}),
        first(o, {"albumArtist", "artist"}), textValue(o["year"]), first(o, {"artUrl", "art_url"})};
}
Track Track::fromJson(const QJsonObject &o) {
    return {first(o, {"file", "id"}), first(o, {"title", "Title", "Name"}),
        first(o, {"trackArtist", "artist", "Artist", "singer"}), first(o, {"album", "Album"}),
        first(o, {"albumId", "album_id"}), first(o, {"year", "Date"}),
        numberValue(o.contains("duration") ? o["duration"] : o["Time"])};
}
PlayerState PlayerState::fromJson(const QJsonObject &o) {
    PlayerState p;
    const auto s = o["status"].isObject() ? o["status"].toObject() : o;
    p.source = textValue(o["inputSource"]);
    p.state = textValue(s["state"]);
    p.track = Track::fromJson(o["song"].isObject() ? o["song"].toObject() : s["song"].toObject());
    p.elapsed = qMax(0., numberValue(s["elapsed"]));
    p.duration = qMax(0., numberValue(s["duration"], p.track.duration));
    p.volume = qBound(-1, int(numberValue(s["volume"], -1)), 100);
    p.shuffle = boolValue(s["random"]);
    p.repeatMode = !boolValue(s["repeat"]) ? "off" : boolValue(s["single"]) ? "song" : "all";
    return p;
}
QString timeLabel(double seconds) {
    const int s = qBound(0, int(qMin(seconds, 864000.)), 864000);
    return QString::number(s / 60) + ":" + QString::number(s % 60).rightJustified(2, '0');
}
} // namespace PiTunes
