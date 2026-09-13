#include "CoverageFreeSpace.h"

#include "CoverageProblemValidator.h"
#include "Geometry/PolygonRegion.h"

namespace {

Marine::CoverageFreeSpaceResult failure(Marine::CoveragePlanningError error)
{
    return {Marine::CoverageProblemValidator::statusForError(error),
            error,
            {},
            Marine::CoverageProblemValidator::messageForError(error)};
}

Marine::CoveragePlanningError errorForNoGoStatus(Marine::Geometry::NoGoValidationStatus status)
{
    switch (status) {
        case Marine::Geometry::NoGoValidationStatus::Success:
            return Marine::CoveragePlanningError::None;
        case Marine::Geometry::NoGoValidationStatus::InvalidOuterBoundary:
            return Marine::CoveragePlanningError::InvalidOuterBoundary;
        case Marine::Geometry::NoGoValidationStatus::InvalidNoGoRegion:
            return Marine::CoveragePlanningError::InvalidNoGoRegion;
        case Marine::Geometry::NoGoValidationStatus::OutsideOuterBoundary:
            return Marine::CoveragePlanningError::NoGoOutsideBoundary;
        case Marine::Geometry::NoGoValidationStatus::BoundaryConflict:
            return Marine::CoveragePlanningError::NoGoBoundaryConflict;
        case Marine::Geometry::NoGoValidationStatus::OverlapOrTouch:
            return Marine::CoveragePlanningError::NoGoOverlapOrTouch;
    }
    return Marine::CoveragePlanningError::GeometryFailure;
}

}  // namespace

namespace Marine {

CoverageFreeSpaceResult buildCoverageFreeSpace(const CoveragePlanningProblem& problem)
{
    CoveragePlanningProblem normalizedProblem = problem;
    const CoveragePlanningError validationError = CoverageProblemValidator::validateAndNormalize(normalizedProblem);
    if (validationError != CoveragePlanningError::None) {
        return failure(validationError);
    }

    const CoveragePlanningError noGoError = errorForNoGoStatus(
        Geometry::validateNoGoRegions(normalizedProblem.region.outerBoundary, normalizedProblem.region.noGoRegions));
    if (noGoError != CoveragePlanningError::None) {
        return failure(noGoError);
    }
    if (normalizedProblem.safetyMarginM > (normalizedProblem.swathWidthM / 2.0)) {
        return failure(CoveragePlanningError::CoverageImpossibleWithSafetyMargin);
    }

    const Geometry::PolygonRegionOperationResult coverageTarget =
        Geometry::buildCoverageTarget(normalizedProblem.region.outerBoundary, normalizedProblem.region.noGoRegions);
    if ((coverageTarget.status != Geometry::PolygonRegionOperationStatus::Success) ||
        (coverageTarget.regions.size() != 1)) {
        return failure(CoveragePlanningError::GeometryFailure);
    }

    const Geometry::PolygonRegionOperationResult trackFeasible = Geometry::buildTrackFeasibleRegion(
        normalizedProblem.region.outerBoundary, normalizedProblem.region.noGoRegions, normalizedProblem.safetyMarginM);
    if (trackFeasible.status != Geometry::PolygonRegionOperationStatus::Success) {
        return failure(CoveragePlanningError::GeometryFailure);
    }
    if (trackFeasible.regions.empty()) {
        return failure(CoveragePlanningError::NoNavigableArea);
    }
    if (trackFeasible.regions.size() != 1) {
        return failure(CoveragePlanningError::DisconnectedFeasibleRegion);
    }

    const Geometry::PolygonRegionOperationResult reachable =
        Geometry::bufferPolygonRegions(trackFeasible.regions, normalizedProblem.swathWidthM / 2.0);
    if (reachable.status != Geometry::PolygonRegionOperationStatus::Success) {
        return failure(CoveragePlanningError::GeometryFailure);
    }
    const Geometry::PolygonRegionContainmentResult reachability =
        Geometry::isRegionSetContained(coverageTarget.regions, reachable.regions);
    if (reachability.status != Geometry::PolygonRegionOperationStatus::Success) {
        return failure(CoveragePlanningError::GeometryFailure);
    }
    if (!reachability.contained) {
        return failure(CoveragePlanningError::CoverageImpossibleWithSafetyMargin);
    }

    CoverageFreeSpaceResult result;
    result.status = PlanningStatus::Success;
    result.error = CoveragePlanningError::None;
    result.freeSpace.coverageTarget = coverageTarget.regions.front();
    result.freeSpace.trackFeasibleRegion = trackFeasible.regions;
    result.message = "Coverage target and connected track-feasible region generated";
    return result;
}

}  // namespace Marine
