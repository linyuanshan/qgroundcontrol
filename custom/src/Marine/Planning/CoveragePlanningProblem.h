#pragma once

#include <string>
#include <vector>

#include "Geometry/GeometryTypes.h"
#include "MarineTypes.h"
#include "PathLegRole.h"

namespace Marine {

enum class CoveragePlanningError
{
    None,
    InvalidOuterBoundary,
    InvalidSwathWidth,
    InvalidSafetyMargin,
    InvalidSweepAngle,
    CoverageImpossibleWithSafetyMargin,
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
};

struct CoveragePlanningProblem
{
    Region2D region;
    double swathWidthM = 0.0;
    double safetyMarginM = 0.0;
    SweepAngleMode sweepAngleMode = SweepAngleMode::Auto;
    // Navigation bearing: 0 degrees North, 90 degrees East, clockwise positive.
    double requestedSweepAngleDeg = 0.0;
};

struct CoveragePlanningSolution
{
    PlanningStatus status = PlanningStatus::Failed;
    std::vector<Point2D> path;
    std::vector<PathLegRole> legRoles;
    double pathLengthM = 0.0;
    // Navigation bearing: 0 degrees North, 90 degrees East, clockwise positive.
    double selectedSweepAngleDeg = 0.0;
    int turnCount = 0;
    CoveragePlanningError error = CoveragePlanningError::None;
    std::string message;
};

}  // namespace Marine
