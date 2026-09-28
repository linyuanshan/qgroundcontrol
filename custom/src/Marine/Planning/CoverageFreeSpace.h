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

/// Historical C=N safety/reachability pipeline. V05 raw C/N/O geometry uses buildCoverageGeometry instead.
[[nodiscard]] CoverageFreeSpaceResult buildCoverageFreeSpace(const CoveragePlanningProblem& problem);

}  // namespace Marine
