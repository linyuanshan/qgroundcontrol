#pragma once

#include <cmath>
#include <vector>

namespace Marine {

struct Point2D
{
    [[nodiscard]] bool isFinite() const { return std::isfinite(xM) && std::isfinite(yM); }

    double xM = 0.0;
    double yM = 0.0;
};

struct Polygon2D
{
    [[nodiscard]] bool isFinite() const
    {
        for (const Point2D& vertex : vertices) {
            if (!vertex.isFinite()) {
                return false;
            }
        }
        return true;
    }

    std::vector<Point2D> vertices;
};

struct Region2D
{
    [[nodiscard]] bool isFinite() const
    {
        if (!outerBoundary.isFinite()) {
            return false;
        }
        for (const Polygon2D& noGoRegion : noGoRegions) {
            if (!noGoRegion.isFinite()) {
                return false;
            }
        }
        return true;
    }

    Polygon2D outerBoundary;
    std::vector<Polygon2D> noGoRegions;
};

}  // namespace Marine
