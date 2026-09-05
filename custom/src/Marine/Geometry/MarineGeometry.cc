#include "MarineGeometry.h"

#include <algorithm>
#include <cmath>

namespace {

double distanceSquared(const Marine::Point2D& first, const Marine::Point2D& second)
{
    const double deltaX = second.xM - first.xM;
    const double deltaY = second.yM - first.yM;
    return (deltaX * deltaX) + (deltaY * deltaY);
}

double crossProduct(const Marine::Point2D& first, const Marine::Point2D& second, const Marine::Point2D& third)
{
    return ((second.xM - first.xM) * (third.yM - first.yM)) - ((second.yM - first.yM) * (third.xM - first.xM));
}

int orientation(const Marine::Point2D& first, const Marine::Point2D& second, const Marine::Point2D& third)
{
    const double firstLength = std::hypot(second.xM - first.xM, second.yM - first.yM);
    const double secondLength = std::hypot(third.xM - first.xM, third.yM - first.yM);
    const double tolerance = Marine::Geometry::LengthEpsilonM * std::max({firstLength, secondLength, 1.0});
    const double cross = crossProduct(first, second, third);
    if (std::abs(cross) <= tolerance) {
        return 0;
    }
    return (cross > 0.0) ? 1 : -1;
}

bool pointOnSegment(const Marine::Point2D& point, const Marine::Point2D& first, const Marine::Point2D& second)
{
    return (orientation(first, second, point) == 0) &&
           (point.xM >= std::min(first.xM, second.xM) - Marine::Geometry::LengthEpsilonM) &&
           (point.xM <= std::max(first.xM, second.xM) + Marine::Geometry::LengthEpsilonM) &&
           (point.yM >= std::min(first.yM, second.yM) - Marine::Geometry::LengthEpsilonM) &&
           (point.yM <= std::max(first.yM, second.yM) + Marine::Geometry::LengthEpsilonM);
}

bool segmentsIntersect(const Marine::Point2D& firstStart, const Marine::Point2D& firstEnd,
                       const Marine::Point2D& secondStart, const Marine::Point2D& secondEnd)
{
    const int firstStartOrientation = orientation(firstStart, firstEnd, secondStart);
    const int firstEndOrientation = orientation(firstStart, firstEnd, secondEnd);
    const int secondStartOrientation = orientation(secondStart, secondEnd, firstStart);
    const int secondEndOrientation = orientation(secondStart, secondEnd, firstEnd);

    if ((firstStartOrientation != firstEndOrientation) && (secondStartOrientation != secondEndOrientation)) {
        return true;
    }
    return ((firstStartOrientation == 0) && pointOnSegment(secondStart, firstStart, firstEnd)) ||
           ((firstEndOrientation == 0) && pointOnSegment(secondEnd, firstStart, firstEnd)) ||
           ((secondStartOrientation == 0) && pointOnSegment(firstStart, secondStart, secondEnd)) ||
           ((secondEndOrientation == 0) && pointOnSegment(firstEnd, secondStart, secondEnd));
}

bool edgesAreAdjacent(std::size_t first, std::size_t second, std::size_t vertexCount)
{
    return (first == second) || (((first + 1) % vertexCount) == second) || (((second + 1) % vertexCount) == first);
}

}  // namespace

namespace Marine::Geometry {

bool isSimpleNonDegeneratePolygon(const Polygon2D& polygon)
{
    const std::vector<Point2D>& vertices = polygon.vertices;
    if ((vertices.size() < 3) || !polygon.isFinite()) {
        return false;
    }

    const double epsilonSquared = LengthEpsilonM * LengthEpsilonM;
    for (std::size_t first = 0; first < vertices.size(); ++first) {
        for (std::size_t second = first + 1; second < vertices.size(); ++second) {
            if (distanceSquared(vertices[first], vertices[second]) <= epsilonSquared) {
                return false;
            }
        }
    }

    for (std::size_t first = 0; first < vertices.size(); ++first) {
        const std::size_t firstEnd = (first + 1) % vertices.size();
        for (std::size_t second = first + 1; second < vertices.size(); ++second) {
            if (edgesAreAdjacent(first, second, vertices.size())) {
                continue;
            }
            const std::size_t secondEnd = (second + 1) % vertices.size();
            if (segmentsIntersect(vertices[first], vertices[firstEnd], vertices[second], vertices[secondEnd])) {
                return false;
            }
        }
    }

    double twiceArea = 0.0;
    double perimeter = 0.0;
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        const Point2D& current = vertices[index];
        const Point2D& next = vertices[(index + 1) % vertices.size()];
        twiceArea += (current.xM * next.yM) - (next.xM * current.yM);
        perimeter += std::hypot(next.xM - current.xM, next.yM - current.yM);
    }

    return std::abs(twiceArea) > (LengthEpsilonM * perimeter);
}

}  // namespace Marine::Geometry
