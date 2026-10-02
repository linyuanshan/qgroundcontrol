#pragma once

#include <optional>
#include <string>
#include <vector>

#include "CoverageQualityEvaluator.h"
#include "CoverageRepairData.h"
#include "Geometry/GeometryTypes.h"
#include "MarineTask.h"
#include "MarineTypes.h"
#include "PathLegRole.h"
#include "PlannerSource.h"

namespace Marine {

enum class CoveragePlanningError
{
    None,
    InvalidOuterBoundary,
    InvalidNavigationBoundary,
    CoverageOutsideNavigationBoundary,
    EmptyCoverageTarget,
    InvalidCoverageTarget,
    UnsupportedSeparateBoundaries,
    InvalidSwathWidth,
    InvalidSafetyMargin,
    InvalidPreferredSafetyMargin,
    InvalidExecutionMargin,
    InvalidSweepAngle,
    InvalidCoverageRequirement,
    CoverageImpossibleWithSafetyMargin,
    CoverageImpossibleWithExecutionMargin,
    UnsupportedExecutionSafetyProfile,
    ExecutionRegionNotConservative,
    UnsupportedNoGoRegion,
    SafetyInsetEmpty,
    SafetyInsetDisconnected,
    NonMonotoneSweep,
    UnsafeConnector,
    InvalidGeneratedPath,
    GeometryFailure,
    InvalidNoGoRegion,
    NoGoOutsideBoundary,
    NoGoBoundaryConflict,
    NoGoOverlapOrTouch,
    NoNavigableArea,
    DisconnectedFeasibleRegion,
    DecompositionFailed,
    InvalidCoverageCell,
    CellCoverageFailed,
    SafeTransitNotFound,
    CoverageIncomplete,
    UnsupportedStrategyCapability,
    ResolvedStrategyUnavailable,
};

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
    std::optional<CoverageQualityEvaluation> coverageQuality;
    std::optional<PlannerSourceInfo> plannerSource;
    // V05-07 pure runtime facts. Failed/CoverageIncomplete still exposes no executable path;
    // the retained candidate in these records is not consumed by the Mission adapter or persisted.
    std::vector<CoverageRepairResult> repairCandidates;
    std::string message;
};

}  // namespace Marine
