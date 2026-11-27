#include "CoverFlowGeometry.h"

#include <algorithm>
#include <cmath>

namespace PiTunes {
namespace {
constexpr double sideStep = .14;
constexpr double sideInnerEdge = .488;
constexpr double sideYaw = 36. * 3.141592653589793 / 180.;
constexpr double focalLength = 1.12;
constexpr double sideDepth = .34;
constexpr double horizon = .60;
constexpr double centreTangent = .60;
constexpr double reflectionGap = .004;

double perspective(double depth) {
    return focalLength / (focalLength + depth);
}

double sideCentre() {
    const double farEdgeScale = perspective(sideDepth + .5 * std::sin(sideYaw));
    return sideInnerEdge + .5 * std::cos(sideYaw) * farEdgeScale;
}

double railOffset(double distance) {
    const double t = std::min(1., distance);
    const double u = 1. - t;
    // Join the wide centre slot to the compact side stack with matched slopes.
    // This keeps a cover's horizontal speed continuous as it reaches the stack.
    const double centre = sideCentre() * t * t * (3. - 2. * t) + centreTangent * t * u * u
        - sideStep * t * t * u;
    return centre + std::max(0., distance - 1.) * sideStep;
}
}

CoverQuads projectCover(double distance, double pixels, const QPointF &origin, double velocity) {
    const double sign = distance < 0 ? -1. : 1.;
    const double offset = std::abs(distance);
    const double t = std::min(1., offset);
    const double shoulder = 16. * t * t * (1. - t) * (1. - t);
    const double direction = sign * velocity / (std::abs(velocity) + .7);
    // An outgoing cover starts to turn as it leaves the front. An incoming
    // cover unwinds sooner and finishes with a nearly flat translation.
    // Both curves join the same resting poses with zero angular slope.
    const double rush = carouselAmount(velocity);
    const double restingTurn = t * t * (3. - 2. * t) - .16 * direction * shoulder;
    const double passing = std::clamp((t - .18) / .82, 0., 1.);
    const double passingTurn = passing * passing * (3. - 2. * passing);
    const double turn = restingTurn + rush * (passingTurn - restingTurn);
    const double yaw = sign * sideYaw * turn;
    // At speed the two front covers share a broad, nearly flat pass. Keep
    // the side stack compact and leave room where the two faces cross.
    const double centre = sign * (railOffset(offset) + rush * (.015 * shoulder - .045 * turn));
    const double faceScale = 1. - .13 * rush * (1. - turn);
    // The turning edge comes forward slightly before settling onto the side.
    const double depth = sideDepth * turn - .045 * 4. * turn * (1. - turn);
    const double sine = std::sin(yaw);
    const double cosine = std::cos(yaw);
    const auto point = [&](double x, double y) {
        const double scale = perspective(depth - x * sine);
        return origin + pixels * QPointF(centre + x * cosine * scale, horizon + (y - horizon) * scale);
    };
    const auto quad = [&](double top, double bottom) {
        const double edge = .5 * faceScale;
        return QPolygonF({point(-edge, top), point(edge, top), point(edge, bottom), point(-edge, bottom)});
    };
    // The cached image contains the lower half of the cover, already mirrored
    // and faded. Project that half-plane directly below the cover's floor edge.
    return {quad(1. - faceScale, 1.), quad(1. + reflectionGap, 1. + .5 * faceScale + reflectionGap)};
}

double coverReach(double halfViewport) {
    return 1. + (halfViewport - sideInnerEdge) / sideStep;
}

double carouselAmount(double velocity) {
    const double t = std::clamp((std::abs(velocity) - 6.) / 12., 0., 1.);
    return t * t * (3. - 2. * t);
}

CoverMotion advanceCoverMotion(CoverMotion start, double target, double seconds, double response) {
    // Critical damping gives a still cover a soft launch. A new target can
    // inherit both the current position and velocity without a visible kick.
    const double displacement = start.position - target;
    const double slope = start.velocity + response * displacement;
    const double decay = std::exp(-response * seconds);
    const double offset = displacement + slope * seconds;
    return {target + offset * decay, (slope - response * offset) * decay};
}

CoverMotion advanceCarouselMotion(CoverMotion start, double target, double seconds, double response) {
    constexpr double speedLimit = 20.;
    // Keep the full gesture destination while pacing the visible crossings.
    // Use the normal spring until it would exceed the cruise speed.
    for (double remaining = std::min(seconds, .12); remaining > 0.;) {
        const double step = std::min(remaining, .008);
        const auto next = advanceCoverMotion(start, target, step, response);
        const double speed = std::clamp(next.velocity, -speedLimit, speedLimit);
        if (speed == next.velocity) {
            start = next;
        } else {
            const double previous = std::clamp(start.velocity, -speedLimit, speedLimit);
            start = {start.position + (previous + speed) * .5 * step, speed};
        }
        remaining -= step;
    }
    return start;
}
} // namespace PiTunes
