#pragma once

#include <QPolygonF>

namespace PiTunes {
struct CoverQuads {
    QPolygonF face;
    QPolygonF reflection;
};

// Geometry is normalized to the centre cover. Its floor is at y=1 and its
// centre is x=0. Each side cover projects onto the same floor plane.
CoverQuads projectCover(double distance, double pixels, const QPointF &origin, double rush = 0.);
double coverReach(double halfViewport);
double carouselAmount(double velocity);

struct CoverMotion {
    double position = 0;
    double velocity = 0;
};

CoverMotion advanceCoverMotion(CoverMotion start, double target, double seconds, double response = 15.);
} // namespace PiTunes
