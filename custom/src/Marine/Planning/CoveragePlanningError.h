#pragma once

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

}  // namespace Marine
