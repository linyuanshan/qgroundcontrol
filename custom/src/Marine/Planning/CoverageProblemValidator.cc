#include "CoverageProblemValidator.h"

#include <cmath>

#include "Geometry/MarineGeometry.h"

namespace Marine {

CoveragePlanningError CoverageProblemValidator::validateAndNormalize(CoveragePlanningProblem& problem)
{
    if (!Geometry::isSimpleNonDegeneratePolygon(problem.region.outerBoundary)) {
        return CoveragePlanningError::InvalidOuterBoundary;
    }
    if (!std::isfinite(problem.swathWidthM) || (problem.swathWidthM <= 0.0)) {
        return CoveragePlanningError::InvalidSwathWidth;
    }
    if (!std::isfinite(problem.safetyMarginM) || (problem.safetyMarginM < 0.0)) {
        return CoveragePlanningError::InvalidSafetyMargin;
    }

    switch (problem.sweepAngleMode) {
        case SweepAngleMode::Auto:
            problem.requestedSweepAngleDeg = 0.0;
            break;
        case SweepAngleMode::Manual:
            if (!std::isfinite(problem.requestedSweepAngleDeg)) {
                return CoveragePlanningError::InvalidSweepAngle;
            }
            problem.requestedSweepAngleDeg = std::fmod(problem.requestedSweepAngleDeg, 180.0);
            if (problem.requestedSweepAngleDeg < 0.0) {
                problem.requestedSweepAngleDeg += 180.0;
            }
            if (problem.requestedSweepAngleDeg == 0.0) {
                problem.requestedSweepAngleDeg = 0.0;
            }
            break;
        default:
            return CoveragePlanningError::InvalidSweepAngle;
    }

    return CoveragePlanningError::None;
}

PlanningStatus CoverageProblemValidator::statusForError(CoveragePlanningError error)
{
    switch (error) {
        case CoveragePlanningError::None:
            return PlanningStatus::Success;
        case CoveragePlanningError::InvalidOuterBoundary:
        case CoveragePlanningError::InvalidNoGoRegion:
        case CoveragePlanningError::NoGoOutsideBoundary:
        case CoveragePlanningError::NoGoBoundaryConflict:
        case CoveragePlanningError::NoGoOverlapOrTouch:
        case CoveragePlanningError::InvalidSwathWidth:
        case CoveragePlanningError::InvalidSafetyMargin:
        case CoveragePlanningError::InvalidSweepAngle:
            return PlanningStatus::InvalidInput;
        case CoveragePlanningError::CoverageImpossibleWithSafetyMargin:
        case CoveragePlanningError::NoNavigableArea:
        case CoveragePlanningError::DisconnectedFeasibleRegion:
        case CoveragePlanningError::UnsupportedNoGoRegion:
        case CoveragePlanningError::SafetyInsetEmpty:
        case CoveragePlanningError::SafetyInsetDisconnected:
        case CoveragePlanningError::NonMonotoneSweep:
        case CoveragePlanningError::UnsafeConnector:
        case CoveragePlanningError::InvalidGeneratedPath:
        case CoveragePlanningError::GeometryFailure:
        case CoveragePlanningError::DecompositionFailed:
        case CoveragePlanningError::InvalidCoverageCell:
        case CoveragePlanningError::CellCoverageFailed:
        case CoveragePlanningError::SafeTransitNotFound:
        case CoveragePlanningError::CoverageIncomplete:
            return PlanningStatus::Failed;
    }
    return PlanningStatus::Failed;
}

std::string CoverageProblemValidator::messageForError(CoveragePlanningError error)
{
    switch (error) {
        case CoveragePlanningError::None:
            return {};
        case CoveragePlanningError::InvalidOuterBoundary:
            return "Work region outer boundary must be a finite, non-self-intersecting polygon with at least three "
                   "distinct points and non-zero area";
        case CoveragePlanningError::InvalidNoGoRegion:
            return "Each no-go region must be a finite, simple polygon with at least three distinct points and "
                   "non-zero area";
        case CoveragePlanningError::NoGoOutsideBoundary:
            return "Each no-go region must be fully inside the work-region outer boundary";
        case CoveragePlanningError::NoGoBoundaryConflict:
            return "No-go regions must not touch or cross the work-region outer boundary";
        case CoveragePlanningError::NoGoOverlapOrTouch:
            return "No-go regions must not overlap, touch, or contain one another";
        case CoveragePlanningError::InvalidSwathWidth:
            return "Coverage swath width must be finite and greater than zero";
        case CoveragePlanningError::InvalidSafetyMargin:
            return "Safety margin must be finite and non-negative";
        case CoveragePlanningError::InvalidSweepAngle:
            return "Manual sweep angle must be finite";
        case CoveragePlanningError::CoverageImpossibleWithSafetyMargin:
            return "The nominal work region cannot be fully covered while keeping centerlines inside the safety inset";
        case CoveragePlanningError::NoNavigableArea:
            return "Safety processing leaves no navigable centerline area";
        case CoveragePlanningError::DisconnectedFeasibleRegion:
            return "Safety processing produces multiple disconnected centerline regions";
        case CoveragePlanningError::UnsupportedNoGoRegion:
            return "The selected coverage planner does not support no-go regions";
        case CoveragePlanningError::SafetyInsetEmpty:
            return "Safety inset leaves no plannable coverage region";
        case CoveragePlanningError::SafetyInsetDisconnected:
            return "Safety inset produces disconnected coverage regions";
        case CoveragePlanningError::NonMonotoneSweep:
            return "Work region cannot be covered by a single monotone sweep";
        case CoveragePlanningError::UnsafeConnector:
            return "Coverage path requires a connector outside the safe region";
        case CoveragePlanningError::InvalidGeneratedPath:
            return "Coverage planner generated an invalid path";
        case CoveragePlanningError::GeometryFailure:
            return "Coverage geometry operation failed";
        case CoveragePlanningError::DecompositionFailed:
            return "Coverage region decomposition failed";
        case CoveragePlanningError::InvalidCoverageCell:
            return "Decomposition produced an invalid coverage cell";
        case CoveragePlanningError::CellCoverageFailed:
            return "Coverage generation failed for a decomposition cell";
        case CoveragePlanningError::SafeTransitNotFound:
            return "No safe static transit route exists between the requested points";
        case CoveragePlanningError::CoverageIncomplete:
            return "The nominal coverage footprint leaves part of the coverage target uncovered";
    }
    return "Coverage planning failed";
}

}  // namespace Marine
