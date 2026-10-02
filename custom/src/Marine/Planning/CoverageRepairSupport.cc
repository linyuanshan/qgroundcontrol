#include "CoverageRepairSupport.h"

#include <algorithm>

#include "Geometry/PolygonRegion.h"

namespace Marine {

BoundaryCoverageSupportResult generateCoverageRepairSupport(const PolygonRegionSet2D& coverageTarget,
                                                            const PolygonRegionSet2D& activeExecutionRegion)
{
    const auto valid = [](const PolygonRegionSet2D& regions) {
        return !regions.empty() && std::ranges::all_of(regions, Geometry::isValidPolygonRegion);
    };
    if (!valid(coverageTarget) || !valid(activeExecutionRegion)) {
        return {.error = CoveragePlanningError::InvalidCoverageTarget,
                .message = "Repair support requires valid target and execution regions"};
    }
    const auto support = Geometry::intersectPolygonRegions(coverageTarget, activeExecutionRegion);
    if (support.status != Geometry::PolygonRegionOperationStatus::Success) {
        return {.error = CoveragePlanningError::GeometryFailure,
                .message = "Target-relative repair support intersection failed"};
    }
    if (support.regions.empty()) {
        return {.status = PlanningStatus::Success, .error = CoveragePlanningError::None};
    }
    // Backend-derived rings have lattice coordinates; the existing primitive canonicalizes
    // cyclic starts, winding and ring order and independently validates every complete ring leg.
    return generateBoundaryCoverageSupport(support.regions);
}

}  // namespace Marine
