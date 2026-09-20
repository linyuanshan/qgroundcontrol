#include "BoundaryCoverageSupport.h"

#include <algorithm>
#include <cmath>
#include <compare>
#include <cstdint>
#include <iterator>
#include <limits>
#include <utility>

#include "Geometry/MarineGeometry.h"
#include "Geometry/PolygonRegion.h"

namespace {

using Marine::BoundaryCoverageComponent;
using Marine::BoundaryCoverageSupportResult;
using Marine::Point2D;
using Marine::Polygon2D;

struct LatticePoint
{
    std::int64_t x = 0;
    std::int64_t y = 0;

    auto operator<=>(const LatticePoint&) const = default;
};

struct CanonicalBoundary
{
    std::vector<LatticePoint> key;
    std::vector<Point2D> path;
};

BoundaryCoverageSupportResult failure(Marine::CoveragePlanningError error, std::string message)
{
    BoundaryCoverageSupportResult result;
    result.error = error;
    result.message = std::move(message);
    return result;
}

LatticePoint latticePoint(const Point2D& point)
{
    return {.x = static_cast<std::int64_t>(std::llround(point.xM * Marine::Geometry::CoordinateScalePerM)),
            .y = static_cast<std::int64_t>(std::llround(point.yM * Marine::Geometry::CoordinateScalePerM))};
}

CanonicalBoundary orderedBoundary(const Polygon2D& polygon, std::size_t startIndex, bool forward)
{
    CanonicalBoundary result;
    const std::size_t vertexCount = polygon.vertices.size();
    result.key.reserve(vertexCount + 1);
    result.path.reserve(vertexCount + 1);
    for (std::size_t step = 0; step < vertexCount; ++step) {
        const std::size_t index =
            forward ? ((startIndex + step) % vertexCount) : ((startIndex + vertexCount - step) % vertexCount);
        result.key.push_back(latticePoint(polygon.vertices.at(index)));
        result.path.push_back(polygon.vertices.at(index));
    }
    result.key.push_back(result.key.front());
    result.path.push_back(result.path.front());
    return result;
}

CanonicalBoundary canonicalBoundary(const Polygon2D& polygon)
{
    const auto minimum = std::ranges::min_element(polygon.vertices, [](const Point2D& first, const Point2D& second) {
        return latticePoint(first) < latticePoint(second);
    });
    const std::size_t startIndex = static_cast<std::size_t>(std::distance(polygon.vertices.begin(), minimum));
    CanonicalBoundary forward = orderedBoundary(polygon, startIndex, true);
    CanonicalBoundary reverse = orderedBoundary(polygon, startIndex, false);
    if (std::lexicographical_compare(reverse.key.begin(), reverse.key.end(), forward.key.begin(), forward.key.end())) {
        return reverse;
    }
    return forward;
}

bool canonicalBoundaryLess(const CanonicalBoundary& first, const CanonicalBoundary& second)
{
    return std::lexicographical_compare(first.key.begin(), first.key.end(), second.key.begin(), second.key.end());
}

}  // namespace

namespace Marine {

BoundaryCoverageSupportResult generateBoundaryCoverageSupport(const PolygonRegionSet2D& trackFeasibleRegion)
{
    if (trackFeasibleRegion.empty() || !std::ranges::all_of(trackFeasibleRegion, [](const PolygonRegion2D& region) {
            return Geometry::isValidPolygonRegion(region);
        })) {
        return failure(CoveragePlanningError::InvalidGeneratedPath, "Track feasible region is invalid");
    }

    std::vector<CanonicalBoundary> boundaries;
    for (const PolygonRegion2D& region : trackFeasibleRegion) {
        boundaries.push_back(canonicalBoundary(region.outerBoundary));
        for (const Polygon2D& hole : region.holes) {
            boundaries.push_back(canonicalBoundary(hole));
        }
    }
    if (boundaries.empty() || (boundaries.size() > std::numeric_limits<std::uint32_t>::max())) {
        return failure(CoveragePlanningError::InvalidGeneratedPath, "Boundary component count is invalid");
    }

    std::ranges::sort(boundaries, canonicalBoundaryLess);
    if (std::ranges::adjacent_find(boundaries, [](const CanonicalBoundary& first, const CanonicalBoundary& second) {
            return first.key == second.key;
        }) != boundaries.end()) {
        return failure(CoveragePlanningError::InvalidGeneratedPath, "Boundary components must be unique");
    }

    BoundaryCoverageSupportResult result;
    result.components.reserve(boundaries.size());
    for (std::size_t index = 0; index < boundaries.size(); ++index) {
        CanonicalBoundary& boundary = boundaries.at(index);
        BoundaryCoverageComponent component;
        component.id = static_cast<std::uint32_t>(index);
        component.path = std::move(boundary.path);
        component.legRoles.assign(component.path.size() - 1, PathLegRole::Coverage);
        for (std::size_t pointIndex = 1; pointIndex < component.path.size(); ++pointIndex) {
            const Point2D& start = component.path.at(pointIndex - 1);
            const Point2D& end = component.path.at(pointIndex);
            const double lengthM = std::hypot(end.xM - start.xM, end.yM - start.yM);
            if (!start.isFinite() || !end.isFinite() || !std::isfinite(lengthM) || (lengthM <= 0.0) ||
                !Geometry::segmentInsidePolygonRegionForValidatedGeometry(trackFeasibleRegion, start, end)) {
                return failure(CoveragePlanningError::InvalidGeneratedPath,
                               "Boundary component contains an invalid centerline leg");
            }
            component.pathLengthM += lengthM;
        }
        if (!std::isfinite(component.pathLengthM) || (component.pathLengthM <= 0.0)) {
            return failure(CoveragePlanningError::InvalidGeneratedPath, "Boundary component metric is invalid");
        }
        result.components.push_back(std::move(component));
    }

    result.status = PlanningStatus::Success;
    result.error = CoveragePlanningError::None;
    result.message = "Track feasible boundaries converted to coverage-support components";
    return result;
}

}  // namespace Marine
