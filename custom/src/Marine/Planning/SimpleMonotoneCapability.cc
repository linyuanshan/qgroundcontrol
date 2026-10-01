#include "SimpleMonotoneCapability.h"

#include <cmath>
#include <utility>

#include "Geometry/MarineGeometry.h"
#include "Geometry/PolygonRegion.h"

namespace Marine {
namespace {

SimpleMonotoneCapabilityResult notApplicable(PlannerResolutionReason reason, std::string message)
{
    return {.applicable = false, .reason = reason, .message = std::move(message)};
}

}  // namespace

SimpleMonotoneCapabilityResult assessSimpleMonotoneCapability(const PolygonRegionSet2D& coverageTarget,
                                                              double selectedSweepAngleDeg)
{
    if (coverageTarget.size() != 1) {
        return notApplicable(PlannerResolutionReason::TargetHasMultipleComponents,
                             "SimpleMonotone requires exactly one coverage-target component");
    }
    if (!coverageTarget.front().holes.empty()) {
        return notApplicable(PlannerResolutionReason::TargetHasHoles, "SimpleMonotone does not support target holes");
    }
    if (!std::isfinite(selectedSweepAngleDeg) || selectedSweepAngleDeg < 0.0 || selectedSweepAngleDeg >= 180.0 ||
        !Geometry::isValidPolygonRegion(coverageTarget.front()) ||
        !Geometry::isMonotoneCellPolygon(coverageTarget.front().outerBoundary,
                                         Geometry::navigationAngleToMathAngle(selectedSweepAngleDeg))) {
        return notApplicable(PlannerResolutionReason::TargetNonMonotoneForSelectedSweep,
                             "Coverage target is not monotone for the selected sweep");
    }
    return {.applicable = true, .reason = PlannerResolutionReason::None};
}

}  // namespace Marine
