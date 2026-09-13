#include "PolygonRegion.h"

#include <algorithm>
#include <clipper2/clipper.h>
#include <cmath>
#include <cstdint>

#include "MarineGeometry.h"

namespace {

constexpr double ArcToleranceClipperUnits = 0.25;

using Marine::Point2D;
using Marine::Polygon2D;
using Marine::PolygonRegion2D;
using Marine::PolygonRegionSet2D;
using Marine::Geometry::PolygonRegionOperationResult;
using Marine::Geometry::PolygonRegionOperationStatus;

double crossProduct(const Point2D& first, const Point2D& second, const Point2D& third)
{
    return ((second.xM - first.xM) * (third.yM - first.yM)) - ((second.yM - first.yM) * (third.xM - first.xM));
}

int orientation(const Point2D& first, const Point2D& second, const Point2D& third)
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

bool pointOnSegment(const Point2D& point, const Point2D& first, const Point2D& second)
{
    return (orientation(first, second, point) == 0) &&
           (point.xM >= std::min(first.xM, second.xM) - Marine::Geometry::LengthEpsilonM) &&
           (point.xM <= std::max(first.xM, second.xM) + Marine::Geometry::LengthEpsilonM) &&
           (point.yM >= std::min(first.yM, second.yM) - Marine::Geometry::LengthEpsilonM) &&
           (point.yM <= std::max(first.yM, second.yM) + Marine::Geometry::LengthEpsilonM);
}

bool segmentsIntersect(const Point2D& firstStart, const Point2D& firstEnd, const Point2D& secondStart,
                       const Point2D& secondEnd)
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

bool polygonBoundariesIntersect(const Polygon2D& first, const Polygon2D& second)
{
    for (std::size_t firstIndex = 0; firstIndex < first.vertices.size(); ++firstIndex) {
        const Point2D& firstStart = first.vertices[firstIndex];
        const Point2D& firstEnd = first.vertices[(firstIndex + 1) % first.vertices.size()];
        for (std::size_t secondIndex = 0; secondIndex < second.vertices.size(); ++secondIndex) {
            const Point2D& secondStart = second.vertices[secondIndex];
            const Point2D& secondEnd = second.vertices[(secondIndex + 1) % second.vertices.size()];
            if (segmentsIntersect(firstStart, firstEnd, secondStart, secondEnd)) {
                return true;
            }
        }
    }
    return false;
}

double twiceSignedArea(const Polygon2D& polygon)
{
    double area = 0.0;
    for (std::size_t index = 0; index < polygon.vertices.size(); ++index) {
        const Point2D& current = polygon.vertices[index];
        const Point2D& next = polygon.vertices[(index + 1) % polygon.vertices.size()];
        area += (current.xM * next.yM) - (next.xM * current.yM);
    }
    return area;
}

void canonicalize(Polygon2D& polygon)
{
    if (twiceSignedArea(polygon) < 0.0) {
        std::reverse(polygon.vertices.begin(), polygon.vertices.end());
    }
    const auto first = std::min_element(
        polygon.vertices.begin(), polygon.vertices.end(), [](const Point2D& left, const Point2D& right) {
            return (left.xM < right.xM) || ((left.xM == right.xM) && (left.yM < right.yM));
        });
    std::rotate(polygon.vertices.begin(), first, polygon.vertices.end());
}

bool toClipperPath(const Polygon2D& polygon, bool positive, Clipper2Lib::Path64& path)
{
    constexpr double MaximumScaledCoordinate = 1e15;
    path.clear();
    path.reserve(polygon.vertices.size());
    for (const Point2D& vertex : polygon.vertices) {
        const double scaledX = vertex.xM * Marine::Geometry::CoordinateScalePerM;
        const double scaledY = vertex.yM * Marine::Geometry::CoordinateScalePerM;
        if (!std::isfinite(scaledX) || !std::isfinite(scaledY) || (std::abs(scaledX) > MaximumScaledCoordinate) ||
            (std::abs(scaledY) > MaximumScaledCoordinate)) {
            return false;
        }
        path.emplace_back(static_cast<int64_t>(std::llround(scaledX)), static_cast<int64_t>(std::llround(scaledY)));
    }
    if (Clipper2Lib::IsPositive(path) != positive) {
        std::reverse(path.begin(), path.end());
    }
    return true;
}

Polygon2D fromClipperPath(const Clipper2Lib::Path64& path)
{
    Polygon2D polygon;
    polygon.vertices.reserve(path.size());
    for (const Clipper2Lib::Point64& vertex : path) {
        polygon.vertices.push_back({static_cast<double>(vertex.x) / Marine::Geometry::CoordinateScalePerM,
                                    static_cast<double>(vertex.y) / Marine::Geometry::CoordinateScalePerM});
    }
    canonicalize(polygon);
    return polygon;
}

bool polygonLess(const Polygon2D& left, const Polygon2D& right)
{
    if (left.vertices.empty() || right.vertices.empty()) {
        return left.vertices.size() < right.vertices.size();
    }
    const Point2D& leftFirst = left.vertices.front();
    const Point2D& rightFirst = right.vertices.front();
    if (leftFirst.xM != rightFirst.xM) {
        return leftFirst.xM < rightFirst.xM;
    }
    if (leftFirst.yM != rightFirst.yM) {
        return leftFirst.yM < rightFirst.yM;
    }
    return left.vertices.size() < right.vertices.size();
}

bool isValidClipperPath(const Clipper2Lib::Path64& path)
{
    return (path.size() >= 3) && (std::abs(Clipper2Lib::Area(path)) >= 1.0);
}

bool appendOuterNode(const Clipper2Lib::PolyPath64& node, PolygonRegionSet2D& regions)
{
    if (node.IsHole()) {
        return false;
    }

    PolygonRegion2D region;
    if (!isValidClipperPath(node.Polygon())) {
        return false;
    }
    region.outerBoundary = fromClipperPath(node.Polygon());
    for (const auto& child : node) {
        if (!child->IsHole()) {
            return false;
        }
        if (!isValidClipperPath(child->Polygon())) {
            return false;
        }
        Polygon2D hole = fromClipperPath(child->Polygon());
        region.holes.push_back(std::move(hole));
    }
    std::sort(region.holes.begin(), region.holes.end(), polygonLess);
    regions.push_back(std::move(region));

    for (const auto& hole : node) {
        for (const auto& island : *hole) {
            if (!appendOuterNode(*island, regions)) {
                return false;
            }
        }
    }
    return true;
}

PolygonRegionOperationResult fromPolyTree(const Clipper2Lib::PolyTree64& tree)
{
    PolygonRegionSet2D regions;
    regions.reserve(tree.Count());
    for (const auto& child : tree) {
        if (!appendOuterNode(*child, regions)) {
            return {PolygonRegionOperationStatus::GeometryFailure, {}};
        }
    }
    std::sort(regions.begin(), regions.end(), [](const PolygonRegion2D& left, const PolygonRegion2D& right) {
        return polygonLess(left.outerBoundary, right.outerBoundary);
    });
    return {PolygonRegionOperationStatus::Success, std::move(regions)};
}

bool polygonsToPositivePaths(const std::vector<Polygon2D>& polygons, Clipper2Lib::Paths64& paths)
{
    paths.clear();
    paths.reserve(polygons.size());
    for (const Polygon2D& polygon : polygons) {
        Clipper2Lib::Path64 path;
        if (!toClipperPath(polygon, true, path)) {
            return false;
        }
        paths.push_back(std::move(path));
    }
    return true;
}

bool regionSetToPaths(const PolygonRegionSet2D& regions, Clipper2Lib::Paths64& paths)
{
    paths.clear();
    for (const PolygonRegion2D& region : regions) {
        Clipper2Lib::Path64 outer;
        if (!toClipperPath(region.outerBoundary, true, outer)) {
            return false;
        }
        paths.push_back(std::move(outer));
        for (const Polygon2D& hole : region.holes) {
            Clipper2Lib::Path64 holePath;
            if (!toClipperPath(hole, false, holePath)) {
                return false;
            }
            paths.push_back(std::move(holePath));
        }
    }
    return true;
}

PolygonRegionOperationResult executeBoolean(Clipper2Lib::ClipType operation, const Clipper2Lib::Paths64& subjects,
                                            const Clipper2Lib::Paths64& clips)
{
    Clipper2Lib::Clipper64 clipper;
    clipper.AddSubject(subjects);
    clipper.AddClip(clips);
    Clipper2Lib::PolyTree64 tree;
    if (!clipper.Execute(operation, Clipper2Lib::FillRule::NonZero, tree)) {
        return {PolygonRegionOperationStatus::GeometryFailure, {}};
    }
    return fromPolyTree(tree);
}

bool allRegionsValid(const PolygonRegionSet2D& regions)
{
    for (const PolygonRegion2D& region : regions) {
        Clipper2Lib::Path64 outer;
        if (!toClipperPath(region.outerBoundary, true, outer) || !isValidClipperPath(outer)) {
            return false;
        }
        for (const Polygon2D& hole : region.holes) {
            Clipper2Lib::Path64 holePath;
            if (!toClipperPath(hole, false, holePath) || !isValidClipperPath(holePath)) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace

namespace Marine::Geometry {

NoGoValidationStatus validateNoGoRegions(const Polygon2D& outerBoundary, const std::vector<Polygon2D>& noGoRegions)
{
    if (!isSimpleNonDegeneratePolygon(outerBoundary)) {
        return NoGoValidationStatus::InvalidOuterBoundary;
    }

    for (const Polygon2D& noGo : noGoRegions) {
        if (!isSimpleNonDegeneratePolygon(noGo)) {
            return NoGoValidationStatus::InvalidNoGoRegion;
        }
        if (polygonBoundariesIntersect(outerBoundary, noGo)) {
            return NoGoValidationStatus::BoundaryConflict;
        }
        if (!containsPoint(outerBoundary, noGo.vertices.front())) {
            return NoGoValidationStatus::OutsideOuterBoundary;
        }
    }

    for (std::size_t first = 0; first < noGoRegions.size(); ++first) {
        for (std::size_t second = first + 1; second < noGoRegions.size(); ++second) {
            if (polygonBoundariesIntersect(noGoRegions[first], noGoRegions[second]) ||
                containsPoint(noGoRegions[first], noGoRegions[second].vertices.front()) ||
                containsPoint(noGoRegions[second], noGoRegions[first].vertices.front())) {
                return NoGoValidationStatus::OverlapOrTouch;
            }
        }
    }
    return NoGoValidationStatus::Success;
}

PolygonRegionOperationResult buildCoverageTarget(const Polygon2D& outerBoundary,
                                                 const std::vector<Polygon2D>& noGoRegions)
{
    if (validateNoGoRegions(outerBoundary, noGoRegions) != NoGoValidationStatus::Success) {
        return {PolygonRegionOperationStatus::InvalidInput, {}};
    }

    Clipper2Lib::Path64 outerPath;
    Clipper2Lib::Paths64 noGoPaths;
    if (!toClipperPath(outerBoundary, true, outerPath) || !polygonsToPositivePaths(noGoRegions, noGoPaths)) {
        return {PolygonRegionOperationStatus::GeometryFailure, {}};
    }
    return executeBoolean(Clipper2Lib::ClipType::Difference, {outerPath}, noGoPaths);
}

PolygonRegionOperationResult buildTrackFeasibleRegion(const Polygon2D& outerBoundary,
                                                      const std::vector<Polygon2D>& noGoRegions, double safetyMarginM)
{
    if ((validateNoGoRegions(outerBoundary, noGoRegions) != NoGoValidationStatus::Success) ||
        !std::isfinite(safetyMarginM) || (safetyMarginM < 0.0)) {
        return {PolygonRegionOperationStatus::InvalidInput, {}};
    }
    if (safetyMarginM == 0.0) {
        return buildCoverageTarget(outerBoundary, noGoRegions);
    }

    const double scaledMargin = safetyMarginM * CoordinateScalePerM;
    if (!std::isfinite(scaledMargin) || (scaledMargin > 1e15)) {
        return {PolygonRegionOperationStatus::GeometryFailure, {}};
    }

    Clipper2Lib::Path64 outerPath;
    Clipper2Lib::Paths64 noGoPaths;
    if (!toClipperPath(outerBoundary, true, outerPath) || !polygonsToPositivePaths(noGoRegions, noGoPaths)) {
        return {PolygonRegionOperationStatus::GeometryFailure, {}};
    }

    const Clipper2Lib::Paths64 insetOuter = Clipper2Lib::InflatePaths(
        {outerPath}, -scaledMargin, Clipper2Lib::JoinType::Miter, Clipper2Lib::EndType::Polygon);
    if (insetOuter.empty()) {
        return {PolygonRegionOperationStatus::Success, {}};
    }
    Clipper2Lib::ClipperOffset noGoOffset(2.0, ArcToleranceClipperUnits);
    noGoOffset.AddPaths(noGoPaths, Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);
    Clipper2Lib::Paths64 inflatedNoGo;
    noGoOffset.Execute(scaledMargin + ArcToleranceClipperUnits, inflatedNoGo);
    if (noGoOffset.ErrorCode() != 0) {
        return {PolygonRegionOperationStatus::GeometryFailure, {}};
    }
    return executeBoolean(Clipper2Lib::ClipType::Difference, insetOuter, inflatedNoGo);
}

PolygonRegionOperationResult bufferPolygonRegions(const PolygonRegionSet2D& regions, double distanceM)
{
    if (!std::isfinite(distanceM) || (distanceM < 0.0) || !allRegionsValid(regions)) {
        return {PolygonRegionOperationStatus::InvalidInput, {}};
    }
    if (regions.empty() || (distanceM == 0.0)) {
        return {PolygonRegionOperationStatus::Success, regions};
    }

    const double scaledDistance = distanceM * CoordinateScalePerM;
    if (!std::isfinite(scaledDistance) || (scaledDistance > 1e15)) {
        return {PolygonRegionOperationStatus::GeometryFailure, {}};
    }

    Clipper2Lib::Paths64 inputPaths;
    if (!regionSetToPaths(regions, inputPaths)) {
        return {PolygonRegionOperationStatus::GeometryFailure, {}};
    }
    Clipper2Lib::ClipperOffset offset(2.0, ArcToleranceClipperUnits);
    offset.AddPaths(inputPaths, Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);
    Clipper2Lib::PolyTree64 tree;
    offset.Execute(scaledDistance + ArcToleranceClipperUnits, tree);
    if (offset.ErrorCode() != 0) {
        return {PolygonRegionOperationStatus::GeometryFailure, {}};
    }
    return fromPolyTree(tree);
}

PolygonRegionContainmentResult isRegionSetContained(const PolygonRegionSet2D& target,
                                                    const PolygonRegionSet2D& container)
{
    if (!allRegionsValid(target) || !allRegionsValid(container)) {
        return {PolygonRegionOperationStatus::InvalidInput, false};
    }
    if (target.empty()) {
        return {PolygonRegionOperationStatus::Success, true};
    }
    if (container.empty()) {
        return {PolygonRegionOperationStatus::Success, false};
    }

    Clipper2Lib::Paths64 targetPaths;
    Clipper2Lib::Paths64 containerPaths;
    if (!regionSetToPaths(target, targetPaths) || !regionSetToPaths(container, containerPaths)) {
        return {PolygonRegionOperationStatus::GeometryFailure, false};
    }
    const PolygonRegionOperationResult difference =
        executeBoolean(Clipper2Lib::ClipType::Difference, targetPaths, containerPaths);
    if (difference.status != PolygonRegionOperationStatus::Success) {
        return {difference.status, false};
    }
    return {PolygonRegionOperationStatus::Success, difference.regions.empty()};
}

}  // namespace Marine::Geometry
