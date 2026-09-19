#pragma once

#include <optional>
#include <span>
#include <string>
#include <vector>

#include "CellCoverage.h"
#include "StaticSafeRouter.h"

namespace Marine {

struct OrderedCellTraversal
{
    CellTraversalState state;
    std::optional<StaticRoute> transitFromPrevious;
};

struct CellOrderingResult
{
    PlanningStatus status = PlanningStatus::Failed;
    std::vector<OrderedCellTraversal> visits;
    double totalTransitLengthM = 0.0;
    CoveragePlanningError error = CoveragePlanningError::SafeTransitNotFound;
    std::string message;
};

[[nodiscard]] CellOrderingResult orderCellTraversals(const PolygonRegionSet2D& trackFeasibleRegion,
                                                     std::span<const CellCoverage> cells,
                                                     std::span<const CellTraversalState> traversalStates);

}  // namespace Marine
