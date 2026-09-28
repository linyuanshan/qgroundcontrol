#pragma once

#include "CoveragePlanningProblem.h"

namespace Marine {

struct CoverageGeometry
{
    PolygonRegionSet2D coverageTarget;
    PolygonRegionSet2D rawNavigationFreeSpace;
};

struct CoverageGeometryResult
{
    CoveragePlanningError error = CoveragePlanningError::GeometryFailure;
    CoverageGeometry geometry;
};

/// Validates C/N/O topology and builds C - O and N - O without offsets or reachability policy.
[[nodiscard]] CoverageGeometryResult buildCoverageGeometry(const Region2D& region);

}  // namespace Marine
