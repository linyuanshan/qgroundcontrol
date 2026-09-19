#pragma once

#include <string>
#include <vector>

#include "CoveragePlanningProblem.h"

namespace Marine {

struct StaticRoute
{
    PlanningStatus status = PlanningStatus::Failed;
    std::vector<Point2D> path;
    double lengthM = 0.0;
    CoveragePlanningError error = CoveragePlanningError::SafeTransitNotFound;
    std::string message;
};

[[nodiscard]] StaticRoute routeStatic(const PolygonRegionSet2D& trackFeasibleRegion, const Point2D& start,
                                      const Point2D& goal);

}  // namespace Marine
