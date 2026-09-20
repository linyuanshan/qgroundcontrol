#pragma once

#include <span>
#include <string>

#include "CoveragePlanningProblem.h"

namespace Marine {

struct CoverageCompletenessResult
{
    PlanningStatus status = PlanningStatus::Failed;
    double coverageTargetAreaM2 = 0.0;
    double coveredTargetAreaM2 = 0.0;
    double uncoveredAreaM2 = 0.0;
    double toleranceM2 = 0.0;
    CoveragePlanningError error = CoveragePlanningError::InvalidGeneratedPath;
    std::string message;
};

[[nodiscard]] CoverageCompletenessResult validateNominalCoverage(const PolygonRegionSet2D& coverageTarget,
                                                                 std::span<const Point2D> path,
                                                                 std::span<const PathLegRole> legRoles,
                                                                 double swathWidthM);

}  // namespace Marine
