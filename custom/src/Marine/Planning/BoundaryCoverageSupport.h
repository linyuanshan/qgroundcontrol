#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "CoveragePlanningProblem.h"

namespace Marine {

struct BoundaryCoverageComponent
{
    std::uint32_t id = 0;
    std::vector<Point2D> path;
    std::vector<PathLegRole> legRoles;
    double pathLengthM = 0.0;
};

struct BoundaryCoverageSupportResult
{
    PlanningStatus status = PlanningStatus::Failed;
    std::vector<BoundaryCoverageComponent> components;
    CoveragePlanningError error = CoveragePlanningError::InvalidGeneratedPath;
    std::string message;
};

[[nodiscard]] BoundaryCoverageSupportResult generateBoundaryCoverageSupport(
    const PolygonRegionSet2D& trackFeasibleRegion);

}  // namespace Marine
