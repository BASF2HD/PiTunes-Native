#pragma once

#include <QPolygonF>

namespace PiTunes {
struct CoverQuads {
    QPolygonF face;
    QPolygonF reflection;
};

struct CoverPose {
    double x = 0.;
    double depth = 0.;
    double yaw = 0.;
};

constexpr double coverMoveDuration = .42;

CoverPose coverPose(double distance);
CoverPose animateCover(const CoverPose &from, const CoverPose &to, double seconds);
double coverMoveProgress(double seconds);
CoverQuads projectCover(const CoverPose &pose, double pixels, const QPointF &origin);
double coverReach(double halfViewport);
} // namespace PiTunes
