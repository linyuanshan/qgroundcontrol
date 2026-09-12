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

    if (!problem.region.noGoRegions.empty()) {
        return CoveragePlanningError::UnsupportedNoGoRegion;
    }
    return CoveragePlanningError::None;
}

PlanningStatus CoverageProblemValidator::statusForError(CoveragePlanningError error)
{
    switch (error) {
        case CoveragePlanningError::None:
            return PlanningStatus::Success;
        case CoveragePlanningError::InvalidOuterBoundary:
        case CoveragePlanningError::InvalidSwathWidth:
        case CoveragePlanningError::InvalidSafetyMargin:
        case CoveragePlanningError::InvalidSweepAngle:
            return PlanningStatus::InvalidInput;
        case CoveragePlanningError::CoverageImpossibleWithSafetyMargin:
        case CoveragePlanningError::UnsupportedNoGoRegion:
        case CoveragePlanningError::SafetyInsetEmpty:
        case CoveragePlanningError::SafetyInsetDisconnected:
        case CoveragePlanningError::NonMonotoneSweep:
        case CoveragePlanningError::UnsafeConnector:
        case CoveragePlanningError::InvalidGeneratedPath:
        case CoveragePlanningError::GeometryFailure:
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
        case CoveragePlanningError::InvalidSwathWidth:
            return "Coverage swath width must be finite and greater than zero";
        case CoveragePlanningError::InvalidSafetyMargin:
            return "Safety margin must be finite and non-negative";
        case CoveragePlanningError::InvalidSweepAngle:
            return "Manual sweep angle must be finite";
        case CoveragePlanningError::CoverageImpossibleWithSafetyMargin:
            return "The nominal work region cannot be fully covered while keeping centerlines inside the safety inset";
        case CoveragePlanningError::UnsupportedNoGoRegion:
            return "Internal no-go coverage routing is reserved for P2.";
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
    }
    return "Coverage planning failed";
}

}  // namespace Marine
