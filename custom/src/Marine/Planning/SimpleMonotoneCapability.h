#pragma once

#include <string>

#include "Geometry/GeometryTypes.h"
#include "PlannerSource.h"

namespace Marine {

struct SimpleMonotoneCapabilityResult
{
    bool applicable = false;
    PlannerResolutionReason reason = PlannerResolutionReason::None;
    std::string message;
};

[[nodiscard]] SimpleMonotoneCapabilityResult assessSimpleMonotoneCapability(const PolygonRegionSet2D& coverageTarget,
                                                                            double selectedSweepAngleDeg);

}  // namespace Marine
