#pragma once

#include <span>
#include <string>
#include <vector>

#include "GreedyCellOrdering.h"

namespace Marine {

struct ComplexCoverageAssemblyResult
{
    PlanningStatus status = PlanningStatus::Failed;
    std::vector<Point2D> path;
    std::vector<PathLegRole> legRoles;
    double coverageLengthM = 0.0;
    double transitLengthM = 0.0;
    double pathLengthM = 0.0;
    int cellCount = 0;
    int turnCount = 0;
    CoveragePlanningError error = CoveragePlanningError::InvalidGeneratedPath;
    std::string message;
};

[[nodiscard]] ComplexCoverageAssemblyResult assembleComplexCoverage(const PolygonRegionSet2D& trackFeasibleRegion,
                                                                    std::span<const CellCoverage> cells,
                                                                    std::span<const OrderedCellTraversal> visits);

}  // namespace Marine
