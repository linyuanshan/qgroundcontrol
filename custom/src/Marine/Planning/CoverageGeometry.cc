#include "CoverageGeometry.h"

#include <algorithm>
#include <utility>

#include "Geometry/MarineGeometry.h"
#include "Geometry/PolygonRegion.h"

namespace Marine {

CoverageGeometryResult buildCoverageGeometry(const Region2D& region)
{
    using Geometry::NoGoValidationStatus;
    using Geometry::PolygonRegionOperationStatus;
    if (!Geometry::isSimpleNonDegeneratePolygon(region.coverageBoundary)) {
        return {.error = CoveragePlanningError::InvalidOuterBoundary};
    }
    if (!Geometry::isSimpleNonDegeneratePolygon(region.navigationBoundary)) {
        return {.error = CoveragePlanningError::InvalidNavigationBoundary};
    }
    const PolygonRegionSet2D coverage{{.outerBoundary = region.coverageBoundary}};
    const PolygonRegionSet2D navigation{{.outerBoundary = region.navigationBoundary}};
    const auto containment = Geometry::isRegionSetContained(coverage, navigation);
    if (containment.status != PolygonRegionOperationStatus::Success) {
        return {.error = CoveragePlanningError::GeometryFailure};
    }
    if (!containment.contained) {
        return {.error = CoveragePlanningError::CoverageOutsideNavigationBoundary};
    }

    switch (Geometry::validateNoGoRegions(region.navigationBoundary, region.noGoRegions)) {
        case NoGoValidationStatus::Success:
            break;
        case NoGoValidationStatus::InvalidOuterBoundary:
            return {.error = CoveragePlanningError::InvalidNavigationBoundary};
        case NoGoValidationStatus::InvalidNoGoRegion:
            return {.error = CoveragePlanningError::InvalidNoGoRegion};
        case NoGoValidationStatus::OutsideOuterBoundary:
            return {.error = CoveragePlanningError::NoGoOutsideBoundary};
        case NoGoValidationStatus::BoundaryConflict:
            return {.error = CoveragePlanningError::NoGoBoundaryConflict};
        case NoGoValidationStatus::OverlapOrTouch:
            return {.error = CoveragePlanningError::NoGoOverlapOrTouch};
    }

    PolygonRegionSet2D obstacles;
    obstacles.reserve(region.noGoRegions.size());
    for (const auto& obstacle : region.noGoRegions) {
        obstacles.push_back({.outerBoundary = obstacle});
    }
    auto target = Geometry::differencePolygonRegions(coverage, obstacles);
    auto freeSpace = Geometry::differencePolygonRegions(navigation, obstacles);
    if (target.status != PolygonRegionOperationStatus::Success ||
        freeSpace.status != PolygonRegionOperationStatus::Success) {
        return {.error = CoveragePlanningError::GeometryFailure};
    }
    if (target.regions.empty()) {
        return {.error = CoveragePlanningError::EmptyCoverageTarget};
    }
    if (!std::ranges::all_of(target.regions, Geometry::isValidPolygonRegion)) {
        return {.error = CoveragePlanningError::InvalidCoverageTarget};
    }
    const auto area = Geometry::polygonRegionArea(target.regions);
    if (area.status != PolygonRegionOperationStatus::Success) {
        return {.error = CoveragePlanningError::GeometryFailure};
    }
    if (area.areaM2 <= 0.0) {
        return {.error = CoveragePlanningError::EmptyCoverageTarget};
    }
    if (freeSpace.regions.empty() || !std::ranges::all_of(freeSpace.regions, Geometry::isValidPolygonRegion)) {
        return {.error = CoveragePlanningError::GeometryFailure};
    }
    return {.error = CoveragePlanningError::None,
            .geometry = {.coverageTarget = std::move(target.regions),
                         .rawNavigationFreeSpace = std::move(freeSpace.regions)}};
}

}  // namespace Marine
