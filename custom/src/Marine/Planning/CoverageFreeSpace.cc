#include "CoverageFreeSpace.h"

#include <cmath>

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

    const Geometry::PolygonRegionOperationResult nominalTrackFeasible = Geometry::buildTrackFeasibleRegion(
        normalizedProblem.region.outerBoundary, normalizedProblem.region.noGoRegions, normalizedProblem.safetyMarginM);
    if (nominalTrackFeasible.status != Geometry::PolygonRegionOperationStatus::Success) {
        return failure(CoveragePlanningError::GeometryFailure);
    }
    if (nominalTrackFeasible.regions.empty()) {
        return failure(CoveragePlanningError::NoNavigableArea);
    }
    if (nominalTrackFeasible.regions.size() != 1) {
        return failure(CoveragePlanningError::DisconnectedFeasibleRegion);
    }

    const Geometry::PolygonRegionOperationResult reachable =
        Geometry::bufferPolygonRegions(nominalTrackFeasible.regions, normalizedProblem.swathWidthM / 2.0);
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

    const double totalMarginM = normalizedProblem.safetyMarginM + normalizedProblem.executionSafety.executionMarginM;
    if (!std::isfinite(totalMarginM)) {
        return failure(CoveragePlanningError::GeometryFailure);
    }
    const Geometry::PolygonRegionOperationResult executionTrackFeasible =
        Geometry::buildTrackFeasibleRegionConservativeMiter(normalizedProblem.region.outerBoundary,
                                                            normalizedProblem.region.noGoRegions, totalMarginM);
    if (executionTrackFeasible.status != Geometry::PolygonRegionOperationStatus::Success) {
        return failure(CoveragePlanningError::GeometryFailure);
    }
    if (executionTrackFeasible.regions.empty()) {
        return failure(CoveragePlanningError::NoNavigableArea);
    }
    if (executionTrackFeasible.regions.size() != 1) {
        return failure(CoveragePlanningError::DisconnectedFeasibleRegion);
    }
    const Geometry::PolygonRegionContainmentResult conservative =
        Geometry::isRegionSetContained(executionTrackFeasible.regions, nominalTrackFeasible.regions);
    if (conservative.status != Geometry::PolygonRegionOperationStatus::Success) {
        return failure(CoveragePlanningError::GeometryFailure);
    }
    if (!conservative.contained) {
        return failure(CoveragePlanningError::ExecutionRegionNotConservative);
    }
    const Geometry::PolygonRegionOperationResult executionReachable =
        Geometry::bufferPolygonRegions(executionTrackFeasible.regions, normalizedProblem.swathWidthM / 2.0);
    if (executionReachable.status != Geometry::PolygonRegionOperationStatus::Success) {
        return failure(CoveragePlanningError::GeometryFailure);
    }
    const Geometry::PolygonRegionContainmentResult executionReachability =
        Geometry::isRegionSetContained(coverageTarget.regions, executionReachable.regions);
    if (executionReachability.status != Geometry::PolygonRegionOperationStatus::Success) {
        return failure(CoveragePlanningError::GeometryFailure);
    }
    if (!executionReachability.contained) {
        return failure(CoveragePlanningError::CoverageImpossibleWithExecutionMargin);
    }

    CoverageFreeSpaceResult result;
    result.status = PlanningStatus::Success;
    result.error = CoveragePlanningError::None;
    result.freeSpace.coverageTarget = coverageTarget.regions.front();
    result.freeSpace.nominalTrackFeasibleRegion = nominalTrackFeasible.regions;
    result.freeSpace.executionTrackFeasibleRegion = executionTrackFeasible.regions;
    result.message = "Coverage target and connected nominal and execution regions generated";
    return result;
}

}  // namespace Marine
