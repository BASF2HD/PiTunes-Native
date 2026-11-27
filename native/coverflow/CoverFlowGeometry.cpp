#include "CoverFlowGeometry.h"

#include <algorithm>
#include <cmath>

namespace PiTunes {
namespace {
constexpr double sideAngle = 36. * 3.141592653589793 / 180.;
constexpr double sideDepth = .34;
constexpr double focalLength = 1.12;
constexpr double sideInnerEdge = .488;
constexpr double sideStep = .14;
constexpr double horizon = .60;
constexpr double reflectionGap = .004;

double perspective(double depth) {
    return focalLength / (focalLength + depth);
}

double railOffset(double distance) {
    const double farEdgeScale = perspective(sideDepth + .5 * std::sin(sideAngle));
    const double sideCentre = sideInnerEdge + .5 * std::cos(sideAngle) * farEdgeScale;
    const double t = std::min(1., distance);
    const double u = 1. - t;
    return sideCentre * t * t * (3. - 2. * t) + .60 * t * u * u
        - sideStep * t * t * u + std::max(0., distance - 1.) * sideStep;
}

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
    const double t = std::min(1., std::abs(distance));
    const double turn = t * t * (3. - 2. * t);
    return {sign * railOffset(std::abs(distance)), turn * sideDepth, sign * turn * sideAngle};
}

double coverMoveProgress(double seconds) {
    return easeOut(seconds, coverMoveDuration);
}

CoverPose animateCover(const CoverPose &from, const CoverPose &to, double seconds) {
    const double move = coverMoveProgress(seconds);
    // Settle the perspective with the turn, then finish on the screen-space rail.
    const double turn = easeOut(seconds, .28);
    return {blend(from.x, to.x, move), blend(from.depth, to.depth, turn), blend(from.yaw, to.yaw, turn)};
}

CoverQuads projectCover(const CoverPose &pose, double pixels, const QPointF &origin) {
    const double sine = std::sin(pose.yaw);
    const double cosine = std::cos(pose.yaw);
    const auto point = [&](double x, double y) {
        const double scale = perspective(pose.depth - x * sine);
        return origin + pixels * QPointF(pose.x + x * cosine * scale, horizon + (y - horizon) * scale);
    };
    const auto quad = [&](double top, double bottom) {
        return QPolygonF({point(-.5, top), point(.5, top), point(.5, bottom), point(-.5, bottom)});
    };
    return {quad(0., 1.), quad(1. + reflectionGap, 1.5 + reflectionGap)};
}

double coverReach(double halfViewport) {
    return 1. + (halfViewport - sideInnerEdge) / sideStep;
}
} // namespace PiTunes
