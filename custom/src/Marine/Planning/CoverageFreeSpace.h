#pragma once

#include "CoveragePlanningProblem.h"

namespace Marine {

struct CoverageFreeSpace
{
    PolygonRegion2D coverageTarget;
    PolygonRegionSet2D nominalTrackFeasibleRegion;
    PolygonRegionSet2D executionTrackFeasibleRegion;
};

struct CoverageFreeSpaceResult
{
    PlanningStatus status = PlanningStatus::Failed;
    CoveragePlanningError error = CoveragePlanningError::GeometryFailure;
    CoverageFreeSpace freeSpace;
    std::string message;
};

[[nodiscard]] CoverageFreeSpaceResult buildCoverageFreeSpace(const CoveragePlanningProblem& problem);

}  // namespace Marine
