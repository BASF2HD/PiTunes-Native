#include "PlayerIcons.h"
#include <QHash>
#include <QPainter>
#include <QPainterPath>

namespace PiTunes {
namespace {
QPixmap renderIcon(PlayerIcon icon, const QColor &color) {
    QPixmap pixmap(48, 48);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(2., 2.);
    const QPen line(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    const auto stroke = [&](std::initializer_list<QPointF> points) {
        painter.setPen(line);
        painter.setBrush(Qt::NoBrush);
        painter.drawPolyline(QPolygonF(points));
    };
    const auto fill = [&](std::initializer_list<QPointF> points) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawPolygon(QPolygonF(points));
    };
    const auto circle = [&](QPointF at, double radius, bool solid) {
        painter.setPen(solid ? QPen(Qt::NoPen) : line);
        painter.setBrush(solid ? QBrush(color) : QBrush(Qt::NoBrush));
        painter.drawEllipse(at, radius, radius);
    };
    switch (icon) {
    case PlayerIcon::Repeat:
    case PlayerIcon::RepeatOne:
        stroke({{3, 10}, {3, 8}, {5, 6}, {20, 6}});
        stroke({{17, 3}, {20, 6}, {17, 9}});
        stroke({{21, 14}, {21, 16}, {19, 18}, {4, 18}});
        stroke({{7, 15}, {4, 18}, {7, 21}});
        if (icon == PlayerIcon::RepeatOne) stroke({{10.5, 11.}, {12., 10.}, {12., 14.}});
        break;
    case PlayerIcon::Shuffle:
        stroke({{3, 6}, {7, 6}, {9, 8}});
        stroke({{14, 15}, {17, 18}, {21, 18}});
        stroke({{3, 18}, {7, 18}, {17, 6}, {21, 6}});
        stroke({{18, 3}, {21, 6}, {18, 9}});
        stroke({{18, 15}, {21, 18}, {18, 21}});
        break;
    case PlayerIcon::Search:
        circle({10.5, 10.5}, 6., false);
        stroke({{15, 15}, {20, 20}});
        break;
    case PlayerIcon::Mute:
    case PlayerIcon::Volume:
        fill({{3, 9}, {7, 9}, {12, 5}, {12, 19}, {7, 15}, {3, 15}});
        if (icon == PlayerIcon::Volume) {
            painter.setPen(line);
            painter.setBrush(Qt::NoBrush);
            painter.drawArc(QRectF(8, 7, 10, 10), -49 * 16, 98 * 16);
            painter.drawArc(QRectF(4, 4, 17, 16), -52 * 16, 104 * 16);
        } else {
            stroke({{16, 10}, {20, 14}});
            stroke({{20, 10}, {16, 14}});
        }
        break;
    case PlayerIcon::Previous:
        fill({{17, 5}, {8, 12}, {17, 19}});
        fill({{5, 5}, {7, 5}, {7, 19}, {5, 19}});
        break;
    case PlayerIcon::Next:
        fill({{7, 5}, {16, 12}, {7, 19}});
        fill({{17, 5}, {19, 5}, {19, 19}, {17, 19}});
        break;
    case PlayerIcon::Play: fill({{8, 4}, {20, 12}, {8, 20}}); break;
    case PlayerIcon::Pause:
        fill({{6, 5}, {10, 5}, {10, 19}, {6, 19}});
        fill({{14, 5}, {18, 5}, {18, 19}, {14, 19}});
        break;
    case PlayerIcon::Left: fill({{16, 4}, {5, 12}, {16, 20}}); break;
    case PlayerIcon::Right: fill({{8, 4}, {19, 12}, {8, 20}}); break;
    case PlayerIcon::Drawer:
        for (int y : {7, 12, 17}) stroke({{4., double(y)}, {20., double(y)}});
        break;
    case PlayerIcon::Albums:
        circle({12, 12}, 8., false);
        circle({12, 12}, 2.3, true);
        break;
    case PlayerIcon::Songs:
        stroke({{8, 17}, {8, 6}, {20, 4}, {20, 15}});
        circle({5.8, 17.6}, 2.4, false);
        circle({17.8, 15.6}, 2.4, false);
        break;
    case PlayerIcon::Artists: {
        circle({12, 7.5}, 3.3, true);
        QPainterPath shoulders;
        shoulders.moveTo(4.5, 20);
        shoulders.quadTo(4.5, 13, 12, 13);
        shoulders.quadTo(19.5, 13, 19.5, 20);
        painter.fillPath(shoulders, color);
        break;
    }
    case PlayerIcon::Playlist:
        stroke({{3, 6}, {14, 6}});
        stroke({{3, 11}, {13, 11}});
        stroke({{3, 16}, {11, 16}});
        stroke({{20, 15}, {20, 6}});
        circle({17.5, 16.1}, 2.4, false);
        break;
    case PlayerIcon::More:
        for (int y : {5, 12, 19}) circle({12., double(y)}, 1.7, true);
        break;
    case PlayerIcon::Settings: {
        circle({12, 12}, 5.7, false);
        for (int i = 0; i < 8; ++i) {
            painter.save();
            painter.translate(12, 12);
            painter.rotate(i * 45.);
            painter.fillRect(QRectF(-1.3, -9.4, 2.6, 3.4), color);
            painter.restore();
        }
        break;
    }
    case PlayerIcon::Favourite:
    case PlayerIcon::FavouriteFilled: {
        QPainterPath heart;
        heart.moveTo(12, 20);
        heart.cubicTo(9, 17.1, 4, 13.9, 4, 8.9);
        heart.cubicTo(4, 4.9, 9.2, 4, 12, 7.5);
        heart.cubicTo(14.8, 4, 20, 4.9, 20, 8.9);
        heart.cubicTo(20, 13.9, 15, 17.1, 12, 20);
        painter.setPen(line);
        painter.setBrush(icon == PlayerIcon::FavouriteFilled ? QBrush(color) : Qt::NoBrush);
        painter.drawPath(heart);
        break;
    }
    case PlayerIcon::Close:
        stroke({{6, 6}, {18, 18}});
        stroke({{18, 6}, {6, 18}});
        break;
    case PlayerIcon::Stop: painter.fillRect(QRectF(6, 6, 12, 12), color); break;
    }
    painter.end();
    pixmap.setDevicePixelRatio(2.);
    return pixmap;
}
}

QIcon playerIcon(PlayerIcon icon, bool active) {
    static QHash<int, QIcon> icons;
    const int key = 2 * int(icon) + (active ? 1 : 0);
    if (const auto found = icons.constFind(key); found != icons.cend()) return *found;
    QIcon result;
    const QColor color = icon == PlayerIcon::FavouriteFilled ? QColor(Qt::white) : QColor("#f2f4fb");
    result.addPixmap(renderIcon(icon, active ? color : QColor("#999daa")));
    result.addPixmap(renderIcon(icon, QColor("#565962")), QIcon::Disabled);
    icons.insert(key, result);
    return result;
}
} // namespace PiTunes
