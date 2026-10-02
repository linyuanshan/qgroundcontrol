#pragma once

#include <optional>
#include <string>
#include <vector>

#include "CoveragePlanningError.h"
#include "MarineTypes.h"
#include "PathLegRole.h"
#include "PlannerSource.h"
#include "PlanningOutcome.h"

namespace Marine {

struct PlanningResult
{
    PlanningStatus status = PlanningStatus::Failed;
    CoveragePlanningError error = CoveragePlanningError::None;
    PlanningOutcome<GeoPoint, GeoPolygonRegionSet> outcome;
    std::vector<GeoPoint> path;
    std::vector<PathLegRole> legRoles;
    double coverageLengthM = 0.0;
    double transitLengthM = 0.0;
    double pathLengthM = 0.0;
    // Navigation bearing: 0 degrees North, 90 degrees East, clockwise positive.
    double selectedSweepAngleDeg = 0.0;
    int cellCount = 0;
    int turnCount = 0;
    std::optional<PlannerSourceInfo> plannerSource;
    std::string message;
};

}  // namespace Marine
