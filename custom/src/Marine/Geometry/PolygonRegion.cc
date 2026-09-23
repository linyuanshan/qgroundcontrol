#include "PolygonRegion.h"

#include <algorithm>
#include <clipper2/clipper.h>
#include <cmath>
#include <cstdint>

#include "MarineGeometry.h"

namespace {

constexpr double ArcToleranceClipperUnits = 0.25;
constexpr double MaximumScaledCoordinate = 1e15;

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

bool toClipperOpenPath(const Marine::Geometry::LineSegment2D& segment, Clipper2Lib::Path64& path)
{
    const double startX = segment.start.xM * Marine::Geometry::CoordinateScalePerM;
    const double startY = segment.start.yM * Marine::Geometry::CoordinateScalePerM;
    const double endX = segment.end.xM * Marine::Geometry::CoordinateScalePerM;
    const double endY = segment.end.yM * Marine::Geometry::CoordinateScalePerM;
    if (!std::isfinite(startX) || !std::isfinite(startY) || !std::isfinite(endX) || !std::isfinite(endY) ||
        (std::abs(startX) > MaximumScaledCoordinate) || (std::abs(startY) > MaximumScaledCoordinate) ||
        (std::abs(endX) > MaximumScaledCoordinate) || (std::abs(endY) > MaximumScaledCoordinate)) {
        return false;
    }

    path = {{static_cast<int64_t>(std::llround(startX)), static_cast<int64_t>(std::llround(startY))},
            {static_cast<int64_t>(std::llround(endX)), static_cast<int64_t>(std::llround(endY))}};
    return path.front() != path.back();
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

bool latticePointOnSegment(const Clipper2Lib::Point64& point, const Clipper2Lib::Point64& a,
                           const Clipper2Lib::Point64& b)
{
    return (Clipper2Lib::CrossProductSign(a, b, point) == 0) && (point.x >= std::min(a.x, b.x)) &&
           (point.x <= std::max(a.x, b.x)) && (point.y >= std::min(a.y, b.y)) && (point.y <= std::max(a.y, b.y));
}

bool latticeSegmentsIntersect(const Clipper2Lib::Point64& a, const Clipper2Lib::Point64& b,
                              const Clipper2Lib::Point64& c, const Clipper2Lib::Point64& d)
{
    const int ac = Clipper2Lib::CrossProductSign(a, b, c);
    const int ad = Clipper2Lib::CrossProductSign(a, b, d);
    const int ca = Clipper2Lib::CrossProductSign(c, d, a);
    const int cb = Clipper2Lib::CrossProductSign(c, d, b);
    return ((ac * ad < 0) && (ca * cb < 0)) || latticePointOnSegment(c, a, b) || latticePointOnSegment(d, a, b) ||
           latticePointOnSegment(a, c, d) || latticePointOnSegment(b, c, d);
}

bool simpleLatticePath(const Clipper2Lib::Path64& path)
{
    if (!isValidClipperPath(path)) {
        return false;
    }
    double perimeter = 0.0;
    for (std::size_t i = 0; i < path.size(); ++i) {
        const auto& a = path[i];
        const auto& b = path[(i + 1) % path.size()];
        const auto& c = path[(i + 2) % path.size()];
        if ((a == b) || latticePointOnSegment(c, a, b) || latticePointOnSegment(a, b, c)) {
            return false;
        }
        perimeter += std::hypot(static_cast<double>(b.x - a.x), static_cast<double>(b.y - a.y));
        for (std::size_t j = i + 1; j < path.size(); ++j) {
            if ((j == i + 1) || ((j + 1) % path.size() == i)) {
                continue;
            }
            if (latticeSegmentsIntersect(a, b, path[j], path[(j + 1) % path.size()])) {
                return false;
            }
        }
    }
    return 2.0 * std::abs(Clipper2Lib::Area(path)) >
           Marine::Geometry::LengthEpsilonM * Marine::Geometry::CoordinateScalePerM * perimeter;
}

bool latticeBoundariesIntersect(const Clipper2Lib::Path64& a, const Clipper2Lib::Path64& b)
{
    for (std::size_t i = 0; i < a.size(); ++i) {
        for (std::size_t j = 0; j < b.size(); ++j) {
            if (latticeSegmentsIntersect(a[i], a[(i + 1) % a.size()], b[j], b[(j + 1) % b.size()])) {
                return true;
            }
        }
    }
    return false;
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

PolygonRegionOperationResult buildTrackFeasibleRegionConservativeMiter(const Polygon2D& outerBoundary,
                                                                       const std::vector<Polygon2D>& noGoRegions,
                                                                       double marginM)
{
    if ((validateNoGoRegions(outerBoundary, noGoRegions) != NoGoValidationStatus::Success) || !std::isfinite(marginM) ||
        (marginM < 0.0)) {
        return {PolygonRegionOperationStatus::InvalidInput, {}};
    }
    if (marginM == 0.0) {
        return buildCoverageTarget(outerBoundary, noGoRegions);
    }

    const double scaledMargin = marginM * CoordinateScalePerM;
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
    noGoOffset.AddPaths(noGoPaths, Clipper2Lib::JoinType::Miter, Clipper2Lib::EndType::Polygon);
    Clipper2Lib::Paths64 inflatedNoGo;
    noGoOffset.Execute(scaledMargin, inflatedNoGo);
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

PolygonRegionOperationResult bufferLineSegments(std::span<const LineSegment2D> segments, double radiusM)
{
    if (!std::isfinite(radiusM) || (radiusM <= 0.0)) {
        return {.status = PolygonRegionOperationStatus::InvalidInput};
    }
    if (segments.empty()) {
        return {.status = PolygonRegionOperationStatus::Success};
    }

    const double scaledRadius = radiusM * CoordinateScalePerM;
    if (!std::isfinite(scaledRadius) || (scaledRadius > MaximumScaledCoordinate)) {
        return {.status = PolygonRegionOperationStatus::GeometryFailure};
    }

    Clipper2Lib::Paths64 paths;
    paths.reserve(segments.size());
    for (const LineSegment2D& segment : segments) {
        Clipper2Lib::Path64 path;
        if (!toClipperOpenPath(segment, path)) {
            return {.status = PolygonRegionOperationStatus::InvalidInput};
        }
        paths.push_back(std::move(path));
    }

    Clipper2Lib::ClipperOffset offset(2.0, ArcToleranceClipperUnits);
    offset.AddPaths(paths, Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Round);
    Clipper2Lib::PolyTree64 tree;
    offset.Execute(scaledRadius, tree);
    if (offset.ErrorCode() != 0) {
        return {.status = PolygonRegionOperationStatus::GeometryFailure};
    }
    return fromPolyTree(tree);
}

PolygonRegionOperationResult differencePolygonRegions(const PolygonRegionSet2D& subjects,
                                                      const PolygonRegionSet2D& clips)
{
    if (!allRegionsValid(subjects) || !allRegionsValid(clips)) {
        return {.status = PolygonRegionOperationStatus::InvalidInput};
    }
    if (subjects.empty()) {
        return {.status = PolygonRegionOperationStatus::Success};
    }

    Clipper2Lib::Paths64 subjectPaths;
    Clipper2Lib::Paths64 clipPaths;
    if (!regionSetToPaths(subjects, subjectPaths) || !regionSetToPaths(clips, clipPaths)) {
        return {.status = PolygonRegionOperationStatus::GeometryFailure};
    }
    return executeBoolean(Clipper2Lib::ClipType::Difference, subjectPaths, clipPaths);
}

PolygonRegionAreaResult polygonRegionArea(const PolygonRegionSet2D& regions)
{
    if (!allRegionsValid(regions)) {
        return {.status = PolygonRegionOperationStatus::InvalidInput};
    }
    if (regions.empty()) {
        return {.status = PolygonRegionOperationStatus::Success};
    }

    Clipper2Lib::Paths64 paths;
    if (!regionSetToPaths(regions, paths)) {
        return {.status = PolygonRegionOperationStatus::GeometryFailure};
    }
    const PolygonRegionOperationResult normalized = executeBoolean(Clipper2Lib::ClipType::Union, paths, {});
    if (normalized.status != PolygonRegionOperationStatus::Success) {
        return {.status = normalized.status};
    }
    if (!regionSetToPaths(normalized.regions, paths)) {
        return {.status = PolygonRegionOperationStatus::GeometryFailure};
    }
    const double scaleSquared = CoordinateScalePerM * CoordinateScalePerM;
    const double areaM2 = std::abs(Clipper2Lib::Area(paths)) / scaleSquared;
    if (!std::isfinite(areaM2)) {
        return {.status = PolygonRegionOperationStatus::GeometryFailure};
    }
    return {.status = PolygonRegionOperationStatus::Success, .areaM2 = areaM2};
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

PolygonRegionOperationResult clipPolygonRegionsToSlab(const PolygonRegionSet2D& regions, double minimumYM,
                                                      double maximumYM)
{
    if (!std::isfinite(minimumYM) || !std::isfinite(maximumYM) || (maximumYM <= minimumYM) ||
        !allRegionsValid(regions)) {
        return {.status = PolygonRegionOperationStatus::InvalidInput};
    }
    if (regions.empty()) {
        return {.status = PolygonRegionOperationStatus::Success};
    }
    Clipper2Lib::Paths64 subjects;
    const double bottom = std::round(minimumYM * CoordinateScalePerM);
    const double top = std::round(maximumYM * CoordinateScalePerM);
    if (!regionSetToPaths(regions, subjects) || !std::isfinite(bottom) || !std::isfinite(top) ||
        (std::abs(bottom) > 1e15) || (std::abs(top) > 1e15) || (top <= bottom)) {
        return {.status = PolygonRegionOperationStatus::GeometryFailure};
    }

    struct Crossing
    {
        double probeX;
        int64_t bottomX;
        int64_t topX;
    };

    std::vector<Crossing> crossings;
    const double probeY = (bottom + top) / 2.0;
    for (const auto& path : subjects) {
        for (std::size_t index = 0; index < path.size(); ++index) {
            const auto& a = path[index];
            const auto& b = path[(index + 1) % path.size()];
            if ((a.y > bottom) && (a.y < top)) {
                return {.status = PolygonRegionOperationStatus::InvalidInput};
            }
            if ((a.y > probeY) == (b.y > probeY)) {
                continue;
            }
            const auto xAt = [&a, &b](double y) {
                return static_cast<double>(a.x) +
                       (y - static_cast<double>(a.y)) * static_cast<double>(b.x - a.x) / static_cast<double>(b.y - a.y);
            };
            crossings.push_back(
                {.probeX = xAt(probeY), .bottomX = std::llround(xAt(bottom)), .topX = std::llround(xAt(top))});
        }
    }
    std::sort(crossings.begin(), crossings.end(),
              [](const Crossing& a, const Crossing& b) { return a.probeX < b.probeX; });
    if (crossings.size() % 2 != 0) {
        return {.status = PolygonRegionOperationStatus::GeometryFailure};
    }
    PolygonRegionOperationResult result{.status = PolygonRegionOperationStatus::Success};
    for (std::size_t index = 0; index < crossings.size(); index += 2) {
        const auto& left = crossings[index];
        const auto& right = crossings[index + 1];
        // A closed global clip can join components at a hole tip on an event level. The two
        // bounding edges of each probe interval define its actual open-slab component.
        Clipper2Lib::Path64 clip{{left.bottomX, static_cast<int64_t>(bottom)},
                                 {right.bottomX, static_cast<int64_t>(bottom)},
                                 {right.topX, static_cast<int64_t>(top)},
                                 {left.topX, static_cast<int64_t>(top)}};
        clip.erase(std::unique(clip.begin(), clip.end()), clip.end());
        if (!clip.empty() && (clip.front() == clip.back())) {
            clip.pop_back();
        }
        if (!isValidClipperPath(clip)) {
            return {.status = PolygonRegionOperationStatus::GeometryFailure};
        }
        auto piece = executeBoolean(Clipper2Lib::ClipType::Intersection, subjects, {clip});
        if ((piece.status != PolygonRegionOperationStatus::Success) || (piece.regions.size() != 1) ||
            !piece.regions.front().holes.empty()) {
            if (((top - bottom) != 1.0) || (clip.size() != 3)) {
                return {.status = PolygonRegionOperationStatus::GeometryFailure};
            }
            // Clipper can discard a one-lattice-unit wedge when the slab clip and subject share a collapsed
            // endpoint. With no subject vertex inside the open slab, the paired crossing edges already define
            // the exact component, so retain that backend-valid polygon without changing event ownership.
            result.regions.push_back({.outerBoundary = fromClipperPath(clip)});
            continue;
        }
        result.regions.push_back(std::move(piece.regions.front()));
    }
    return result;
}

PolygonRegionOperationResult unionPolygonRegions(const PolygonRegionSet2D& regions)
{
    if (!allRegionsValid(regions)) {
        return {.status = PolygonRegionOperationStatus::InvalidInput};
    }
    Clipper2Lib::Paths64 subjects;
    if (!regionSetToPaths(regions, subjects)) {
        return {.status = PolygonRegionOperationStatus::GeometryFailure};
    }
    return executeBoolean(Clipper2Lib::ClipType::Union, subjects, {});
}

bool shareSlabBoundary(const Polygon2D& below, const Polygon2D& above, double eventYM)
{
    if (!below.isFinite() || !above.isFinite() || !std::isfinite(eventYM)) {
        return false;
    }
    // Compare on the backend lattice, so neighboring slabs use exactly the same cut.
    const double cutY = std::round(eventYM * CoordinateScalePerM);
    const auto onCut = [cutY](const Point2D& vertex) { return std::round(vertex.yM * CoordinateScalePerM) == cutY; };
    for (std::size_t first = 0; first < below.vertices.size(); ++first) {
        const Point2D& a = below.vertices[first];
        const Point2D& b = below.vertices[(first + 1) % below.vertices.size()];
        if (!onCut(a) || !onCut(b)) {
            continue;
        }
        for (std::size_t second = 0; second < above.vertices.size(); ++second) {
            const Point2D& c = above.vertices[second];
            const Point2D& d = above.vertices[(second + 1) % above.vertices.size()];
            const double aX = std::round(a.xM * CoordinateScalePerM);
            const double bX = std::round(b.xM * CoordinateScalePerM);
            const double cX = std::round(c.xM * CoordinateScalePerM);
            const double dX = std::round(d.xM * CoordinateScalePerM);
            if (onCut(c) && onCut(d) &&
                (std::min(std::max(aX, bX), std::max(cX, dX)) > std::max(std::min(aX, bX), std::min(cX, dX)))) {
                return true;
            }
        }
    }
    return false;
}

bool isValidPolygonRegion(const PolygonRegion2D& region)
{
    Clipper2Lib::Path64 outer;
    if (!toClipperPath(region.outerBoundary, true, outer) || !simpleLatticePath(outer)) {
        return false;
    }
    Clipper2Lib::Paths64 holes;
    for (const Polygon2D& hole : region.holes) {
        Clipper2Lib::Path64 path;
        if (!toClipperPath(hole, false, path) || !simpleLatticePath(path) || latticeBoundariesIntersect(outer, path) ||
            (Clipper2Lib::PointInPolygon(path.front(), outer) != Clipper2Lib::PointInPolygonResult::IsInside)) {
            return false;
        }
        for (const auto& previous : holes) {
            if (latticeBoundariesIntersect(previous, path) ||
                (Clipper2Lib::PointInPolygon(path.front(), previous) != Clipper2Lib::PointInPolygonResult::IsOutside) ||
                (Clipper2Lib::PointInPolygon(previous.front(), path) != Clipper2Lib::PointInPolygonResult::IsOutside)) {
                return false;
            }
        }
        holes.push_back(std::move(path));
    }
    return true;
}

bool isMonotoneCellPolygon(const Polygon2D& polygon, double mathAngleDeg)
{
    if (!std::isfinite(mathAngleDeg)) {
        return false;
    }
    Clipper2Lib::Path64 path;
    if (!toClipperPath(toSweepFrame(polygon, mathAngleDeg), true, path) || !simpleLatticePath(path)) {
        return false;
    }
    const auto lessY = [](const auto& a, const auto& b) { return (a.y < b.y) || ((a.y == b.y) && (a.x < b.x)); };
    const auto minimum = static_cast<std::size_t>(std::min_element(path.begin(), path.end(), lessY) - path.begin());
    const auto maximum = static_cast<std::size_t>(std::max_element(path.begin(), path.end(), lessY) - path.begin());
    for (const std::size_t step : {std::size_t{1}, path.size() - 1}) {
        std::size_t current = minimum;
        while (current != maximum) {
            const std::size_t next = (current + step) % path.size();
            if (path[next].y < path[current].y) {
                return false;
            }
            current = next;
        }
    }
    return true;
}

}  // namespace Marine::Geometry
