#include "MarineGeometry.h"

#include <algorithm>
#include <clipper2/clipper.h>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <utility>

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

double twiceSignedArea(const Marine::Polygon2D& polygon)
{
    double area = 0.0;
    for (std::size_t index = 0; index < polygon.vertices.size(); ++index) {
        const Marine::Point2D& current = polygon.vertices[index];
        const Marine::Point2D& next = polygon.vertices[(index + 1) % polygon.vertices.size()];
        area += (current.xM * next.yM) - (next.xM * current.yM);
    }
    return area;
}

void canonicalize(Marine::Polygon2D& polygon)
{
    if (twiceSignedArea(polygon) < 0.0) {
        std::reverse(polygon.vertices.begin(), polygon.vertices.end());
    }

    const auto first =
        std::min_element(polygon.vertices.begin(), polygon.vertices.end(),
                         [](const Marine::Point2D& left, const Marine::Point2D& right) {
                             return (left.xM < right.xM) || ((left.xM == right.xM) && (left.yM < right.yM));
                         });
    std::rotate(polygon.vertices.begin(), first, polygon.vertices.end());
}

bool toClipperPath(const Marine::Polygon2D& polygon, Clipper2Lib::Path64& path)
{
    constexpr double maximumScaledCoordinate = 1e15;

    path.reserve(polygon.vertices.size());
    for (const Marine::Point2D& vertex : polygon.vertices) {
        const double scaledX = vertex.xM * Marine::Geometry::CoordinateScalePerM;
        const double scaledY = vertex.yM * Marine::Geometry::CoordinateScalePerM;
        if (!std::isfinite(scaledX) || !std::isfinite(scaledY) || (std::abs(scaledX) > maximumScaledCoordinate) ||
            (std::abs(scaledY) > maximumScaledCoordinate)) {
            return false;
        }
        path.emplace_back(static_cast<int64_t>(std::llround(scaledX)), static_cast<int64_t>(std::llround(scaledY)));
    }

    if (!Clipper2Lib::IsPositive(path)) {
        std::reverse(path.begin(), path.end());
    }
    return true;
}

Marine::Polygon2D fromClipperPath(const Clipper2Lib::Path64& path)
{
    Marine::Polygon2D polygon;
    polygon.vertices.reserve(path.size());
    for (const Clipper2Lib::Point64& vertex : path) {
        polygon.vertices.push_back({static_cast<double>(vertex.x) / Marine::Geometry::CoordinateScalePerM,
                                    static_cast<double>(vertex.y) / Marine::Geometry::CoordinateScalePerM});
    }
    canonicalize(polygon);
    return polygon;
}

double normalizedSweepAngle(double sweepAngleDeg)
{
    double normalized = std::fmod(sweepAngleDeg, 180.0);
    if (normalized < 0.0) {
        normalized += 180.0;
    }
    return normalized;
}

std::size_t extremeVertexIndex(const Marine::Polygon2D& polygon, bool minimum)
{
    const auto yIsMoreExtreme = [minimum](double candidate, double current) {
        return minimum ? (candidate < current) : (candidate > current);
    };

    double extremeY = polygon.vertices.front().yM;
    for (const Marine::Point2D& vertex : polygon.vertices) {
        if (yIsMoreExtreme(vertex.yM, extremeY)) {
            extremeY = vertex.yM;
        }
    }

    std::size_t result = 0;
    bool found = false;
    for (std::size_t index = 0; index < polygon.vertices.size(); ++index) {
        const Marine::Point2D& vertex = polygon.vertices[index];
        if ((std::abs(vertex.yM - extremeY) <= Marine::Geometry::LengthEpsilonM) &&
            (!found || (vertex.xM < polygon.vertices[result].xM))) {
            result = index;
            found = true;
        }
    }
    return result;
}

bool chainYIsNonDecreasing(const Marine::Polygon2D& polygon, std::size_t start, std::size_t end, int direction)
{
    const std::size_t vertexCount = polygon.vertices.size();
    std::size_t current = start;
    while (current != end) {
        const std::size_t next =
            (direction > 0) ? ((current + 1) % vertexCount) : ((current + vertexCount - 1) % vertexCount);
        if (polygon.vertices[next].yM + Marine::Geometry::LengthEpsilonM < polygon.vertices[current].yM) {
            return false;
        }
        current = next;
    }
    return true;
}

void appendInterval(std::vector<Marine::Geometry::ScanlineInterval>& intervals, double firstX, double secondX)
{
    const double minimumX = std::min(firstX, secondX);
    const double maximumX = std::max(firstX, secondX);
    if ((maximumX - minimumX) > Marine::Geometry::LengthEpsilonM) {
        intervals.push_back({minimumX, maximumX});
    }
}

std::vector<Marine::Geometry::ScanlineInterval> mergeIntervals(
    std::vector<Marine::Geometry::ScanlineInterval> intervals)
{
    if (intervals.empty()) {
        return {};
    }

    std::sort(intervals.begin(), intervals.end(), [](const auto& left, const auto& right) {
        return (left.minimumXM < right.minimumXM) ||
               ((left.minimumXM == right.minimumXM) && (left.maximumXM < right.maximumXM));
    });

    std::vector<Marine::Geometry::ScanlineInterval> merged;
    merged.reserve(intervals.size());
    merged.push_back(intervals.front());
    for (std::size_t index = 1; index < intervals.size(); ++index) {
        Marine::Geometry::ScanlineInterval& current = merged.back();
        const Marine::Geometry::ScanlineInterval& next = intervals[index];
        if (next.minimumXM <= current.maximumXM + Marine::Geometry::LengthEpsilonM) {
            current.maximumXM = std::max(current.maximumXM, next.maximumXM);
        } else {
            merged.push_back(next);
        }
    }
    return merged;
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

PolygonInsetResult insetPolygon(const Polygon2D& polygon, double marginM)
{
    if (!isSimpleNonDegeneratePolygon(polygon) || !std::isfinite(marginM) || (marginM < 0.0)) {
        return {PolygonInsetStatus::InvalidInput, {}};
    }
    if (marginM == 0.0) {
        return {PolygonInsetStatus::Success, polygon};
    }

    Clipper2Lib::Path64 inputPath;
    if (!toClipperPath(polygon, inputPath)) {
        return {PolygonInsetStatus::GeometryFailure, {}};
    }

    const double scaledMargin = marginM * CoordinateScalePerM;
    if (!std::isfinite(scaledMargin) || (scaledMargin > 1e15)) {
        return {PolygonInsetStatus::GeometryFailure, {}};
    }

    const Clipper2Lib::Paths64 insetPaths = Clipper2Lib::InflatePaths(
        {inputPath}, -scaledMargin, Clipper2Lib::JoinType::Miter, Clipper2Lib::EndType::Polygon);
    if (insetPaths.empty()) {
        return {PolygonInsetStatus::Empty, {}};
    }
    if (insetPaths.size() != 1) {
        return {PolygonInsetStatus::Disconnected, {}};
    }

    Polygon2D inset = fromClipperPath(insetPaths.front());
    if (!isSimpleNonDegeneratePolygon(inset)) {
        return {PolygonInsetStatus::GeometryFailure, {}};
    }
    return {PolygonInsetStatus::Success, std::move(inset)};
}

Point2D toSweepFrame(const Point2D& point, double sweepAngleDeg)
{
    const double angleRad = normalizedSweepAngle(sweepAngleDeg) * std::numbers::pi / 180.0;
    const double cosine = std::cos(angleRad);
    const double sine = std::sin(angleRad);
    return {(cosine * point.xM) + (sine * point.yM), (-sine * point.xM) + (cosine * point.yM)};
}

Polygon2D toSweepFrame(const Polygon2D& polygon, double sweepAngleDeg)
{
    Polygon2D transformed;
    transformed.vertices.reserve(polygon.vertices.size());
    for (const Point2D& vertex : polygon.vertices) {
        transformed.vertices.push_back(toSweepFrame(vertex, sweepAngleDeg));
    }
    return transformed;
}

Point2D fromSweepFrame(const Point2D& point, double sweepAngleDeg)
{
    const double angleRad = normalizedSweepAngle(sweepAngleDeg) * std::numbers::pi / 180.0;
    const double cosine = std::cos(angleRad);
    const double sine = std::sin(angleRad);
    return {(cosine * point.xM) - (sine * point.yM), (sine * point.xM) + (cosine * point.yM)};
}

bool isSweepMonotone(const Polygon2D& polygon, double sweepAngleDeg)
{
    if (!isSimpleNonDegeneratePolygon(polygon) || !std::isfinite(sweepAngleDeg)) {
        return false;
    }

    const Polygon2D sweepPolygon = toSweepFrame(polygon, sweepAngleDeg);
    const std::size_t minimum = extremeVertexIndex(sweepPolygon, true);
    const std::size_t maximum = extremeVertexIndex(sweepPolygon, false);
    return chainYIsNonDecreasing(sweepPolygon, minimum, maximum, 1) &&
           chainYIsNonDecreasing(sweepPolygon, minimum, maximum, -1);
}

ScanlineResult intersectScanline(const Polygon2D& sweepAlignedPolygon, double yM)
{
    if (!isSimpleNonDegeneratePolygon(sweepAlignedPolygon) || !std::isfinite(yM)) {
        return {ScanlineStatus::InvalidInput, {}};
    }

    double scanY = yM;
    double closestDistance = LengthEpsilonM;
    for (const Point2D& vertex : sweepAlignedPolygon.vertices) {
        const double distance = std::abs(vertex.yM - yM);
        if (distance <= closestDistance) {
            scanY = vertex.yM;
            closestDistance = distance;
        }
    }

    std::vector<double> crossings;
    std::vector<ScanlineInterval> intervals;
    for (std::size_t index = 0; index < sweepAlignedPolygon.vertices.size(); ++index) {
        const Point2D& first = sweepAlignedPolygon.vertices[index];
        const Point2D& second = sweepAlignedPolygon.vertices[(index + 1) % sweepAlignedPolygon.vertices.size()];
        const double deltaY = second.yM - first.yM;
        if (std::abs(deltaY) <= LengthEpsilonM) {
            if (std::abs(scanY - first.yM) <= LengthEpsilonM) {
                appendInterval(intervals, first.xM, second.xM);
            }
            continue;
        }

        const bool crosses =
            ((first.yM <= scanY) && (scanY < second.yM)) || ((second.yM <= scanY) && (scanY < first.yM));
        if (crosses) {
            const double ratio = (scanY - first.yM) / deltaY;
            crossings.push_back(first.xM + (ratio * (second.xM - first.xM)));
        }
    }

    std::sort(crossings.begin(), crossings.end());
    if ((crossings.size() % 2) != 0) {
        return {ScanlineStatus::GeometryFailure, {}};
    }
    for (std::size_t index = 0; index < crossings.size(); index += 2) {
        appendInterval(intervals, crossings[index], crossings[index + 1]);
    }

    intervals = mergeIntervals(std::move(intervals));
    if (intervals.empty()) {
        return {ScanlineStatus::NoIntersection, {}};
    }
    return {ScanlineStatus::Success, std::move(intervals)};
}

}  // namespace Marine::Geometry
