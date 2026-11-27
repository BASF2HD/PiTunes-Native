#include "CoverFlowGeometry.h"

#include <algorithm>
#include <cmath>

namespace PiTunes {
namespace {
// NaviGlassPlayer's plane, pivot, and camera distances, normalized to
// the front cover's width. Qt projects the face and reflection together.
constexpr double frontWidth = 304. * 1.05;
constexpr double sideAngle = 52. * 3.141592653589793 / 180.;
constexpr double sideDepth = 268. / frontWidth;
constexpr double focalLength = 890. / frontWidth;
constexpr double sidePivotX = .5 + 16. / frontWidth;
constexpr double sideStep = 76. / frontWidth;
constexpr double sideScale = .9 / 1.05;
constexpr double horizon = .60;
constexpr double reflectionGap = .004;

double easeOut(double seconds, double duration) {
    const double tail = 1. - std::clamp(seconds / duration, 0., 1.);
    return 1. - tail * tail * tail;
}

double blend(double from, double to, double fraction) {
    return from + (to - from) * fraction;
}
}

CoverPose coverPose(double distance) {
    const double sign = distance < 0. ? -1. : 1.;
    const double near = std::min(1., std::abs(distance));
    const double extra = std::max(0., std::abs(distance) - 1.);
    return {sign * (near * sidePivotX + extra * sideStep), near * sideDepth,
            sign * near * sideAngle, blend(1., sideScale, near), sign * near * .5};
}

double coverMoveProgress(double seconds) {
    return easeOut(seconds, coverMoveDuration);
}

CoverPose animateCover(const CoverPose &from, const CoverPose &to, double seconds) {
    const double move = coverMoveProgress(seconds);
    return {blend(from.x, to.x, move), blend(from.depth, to.depth, move),
            blend(from.yaw, to.yaw, easeOut(seconds, .28)), blend(from.scale, to.scale, easeOut(seconds, .32)),
            blend(from.pivot, to.pivot, move)};
}

CoverQuads projectCover(const CoverPose &pose, double pixels, const QPointF &origin) {
    const double sine = std::sin(pose.yaw);
    const double cosine = std::cos(pose.yaw);
    const auto point = [&](double x, double y) {
        const double local = (x + pose.pivot) * pose.scale;
        const double perspective = focalLength / (focalLength + pose.depth - local * sine);
        return origin + pixels * QPointF((pose.x + local * cosine) * perspective,
                                        horizon + (y * pose.scale + 1. - pose.scale - horizon) * perspective);
    };
    const auto quad = [&](double top, double bottom) {
        return QPolygonF({point(-.5, top), point(.5, top), point(.5, bottom), point(-.5, bottom)});
    };
    return {quad(0., 1.), quad(1. + reflectionGap, 1.5 + reflectionGap)};
}

double coverReach(double halfViewport) {
    const double edgeAtStackDepth = halfViewport * (focalLength + sideDepth) / focalLength;
    return 1. + (edgeAtStackDepth - sidePivotX) / sideStep;
}
} // namespace PiTunes
