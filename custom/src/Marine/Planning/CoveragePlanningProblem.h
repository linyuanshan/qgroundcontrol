#pragma once

#include <optional>
#include <string>
#include <vector>

#include "CoveragePlanningError.h"
#include "CoverageRepairData.h"
#include "Geometry/GeometryTypes.h"
#include "MarineTask.h"
#include "MarineTypes.h"
#include "PathLegRole.h"
#include "PlannerSource.h"
#include "PlanningOutcome.h"

namespace Marine {

struct CoveragePlanningProblem
{
    Region2D region;
    double swathWidthM = 0.0;
    SafetyConfig safety;
    ExecutionSafetyProfile executionSafety;
    CoverageRequirement coverageRequirement = CoverageRequirement::Standard;
    SweepAngleMode sweepAngleMode = SweepAngleMode::Auto;
    // Navigation bearing: 0 degrees North, 90 degrees East, clockwise positive.
    double requestedSweepAngleDeg = 0.0;
};

struct CoveragePlanningSolution
{
    PlanningStatus status = PlanningStatus::Failed;
    std::vector<Point2D> path;
    std::vector<PathLegRole> legRoles;
    double coverageLengthM = 0.0;
    double transitLengthM = 0.0;
    double pathLengthM = 0.0;
    // Navigation bearing: 0 degrees North, 90 degrees East, clockwise positive.
    double selectedSweepAngleDeg = 0.0;
    int cellCount = 0;
    int turnCount = 0;
    CoveragePlanningError error = CoveragePlanningError::None;
    PlanningOutcome<Point2D, PolygonRegionSet2D> outcome;
    std::optional<PlannerSourceInfo> plannerSource;
    // Internal V05-07 candidate records; only the selected outcome is persisted.
    std::vector<CoverageRepairResult> repairCandidates;
    std::string message;
};

}  // namespace Marine
