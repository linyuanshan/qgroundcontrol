#include "CoverageSafety.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "CoverageGeometry.h"
#include "Geometry/MarineGeometry.h"
#include "Geometry/PolygonRegion.h"

namespace Marine {
namespace {

CoveragePlanningError validateRegionHierarchy(const SafetyTrackRegions& regions)
{
    const auto preferred =
        Geometry::isRegionSetContained(regions.preferredExecutionTrackRegion, regions.hardExecutionTrackRegion);
    const auto hard = Geometry::isRegionSetContained(regions.hardExecutionTrackRegion, regions.nominalHardTrackRegion);
    if (preferred.status != Geometry::PolygonRegionOperationStatus::Success ||
        hard.status != Geometry::PolygonRegionOperationStatus::Success) {
        return CoveragePlanningError::GeometryFailure;
    }
    if (!preferred.contained || !hard.contained) {
        return CoveragePlanningError::ExecutionRegionNotConservative;
    }
    return CoveragePlanningError::None;
}

}  // namespace

CoveragePlanningError validateSafetyMargins(const SafetyConfig& safety, const ExecutionSafetyProfile& executionSafety)
{
    if (!std::isfinite(safety.hardSafetyMarginM) || safety.hardSafetyMarginM < 0.0) {
        return CoveragePlanningError::InvalidSafetyMargin;
    }
    if (!std::isfinite(safety.preferredSafetyMarginM) || safety.preferredSafetyMarginM < safety.hardSafetyMarginM) {
        return CoveragePlanningError::InvalidPreferredSafetyMargin;
    }
    if (!std::isfinite(executionSafety.executionMarginM) || executionSafety.executionMarginM < 0.0) {
        return CoveragePlanningError::InvalidExecutionMargin;
    }
    return CoveragePlanningError::None;
}

SafetyTrackRegionsResult buildSafetyTrackRegions(const Region2D& region, const SafetyConfig& safety,
                                                 const ExecutionSafetyProfile& executionSafety)
{
    const auto safetyError = validateSafetyMargins(safety, executionSafety);
    if (safetyError != CoveragePlanningError::None) {
        return {.error = safetyError};
    }
    const auto geometry = buildCoverageGeometry(region);
    if (geometry.error != CoveragePlanningError::None) {
        return {.error = geometry.error};
    }
    const double hardMarginM = safety.hardSafetyMarginM + executionSafety.executionMarginM;
    const double preferredMarginM = safety.preferredSafetyMarginM + executionSafety.executionMarginM;
    if (!std::isfinite(hardMarginM) || !std::isfinite(preferredMarginM)) {
        return {.error = CoveragePlanningError::GeometryFailure};
    }
    auto nominal =
        Geometry::buildTrackFeasibleRegion(region.navigationBoundary, region.noGoRegions, safety.hardSafetyMarginM);
    auto hard =
        Geometry::buildTrackFeasibleRegionConservativeMiter(region.navigationBoundary, region.noGoRegions, hardMarginM);
    auto preferred = Geometry::buildTrackFeasibleRegionConservativeMiter(region.navigationBoundary, region.noGoRegions,
                                                                         preferredMarginM);
    if (nominal.status != Geometry::PolygonRegionOperationStatus::Success ||
        hard.status != Geometry::PolygonRegionOperationStatus::Success ||
        preferred.status != Geometry::PolygonRegionOperationStatus::Success) {
        return {.error = CoveragePlanningError::GeometryFailure};
    }
    SafetyTrackRegions regions{std::move(nominal.regions), std::move(hard.regions), std::move(preferred.regions)};
    const auto hierarchyError = validateRegionHierarchy(regions);
    if (hierarchyError != CoveragePlanningError::None) {
        return {.error = hierarchyError};
    }
    return {.error = CoveragePlanningError::None, .regions = std::move(regions)};
}

SafetyCandidateAssessment evaluateSafetyCandidate(const SafetyTrackRegionsResult& regions,
                                                  std::span<const Point2D> path)
{
    if (regions.error != CoveragePlanningError::None) {
        return {.error = regions.error};
    }
    const auto hierarchyError = validateRegionHierarchy(regions.regions);
    if (hierarchyError != CoveragePlanningError::None) {
        return {.error = hierarchyError};
    }
    if (regions.regions.hardExecutionTrackRegion.empty()) {
        return {.error = CoveragePlanningError::NoNavigableArea};
    }
    if (path.size() < 2 || !std::ranges::all_of(path, &Point2D::isFinite)) {
        return {.error = CoveragePlanningError::InvalidGeneratedPath};
    }
    SafetyCandidateAssessment result{.error = CoveragePlanningError::None, .tier = SafetySolutionTier::D0};
    double totalLengthM = 0.0;
    for (std::size_t i = 1; i < path.size(); ++i) {
        const double lengthM = std::hypot(path[i].xM - path[i - 1].xM, path[i].yM - path[i - 1].yM);
        totalLengthM += lengthM;
        if (!std::isfinite(totalLengthM) || lengthM <= 0.0) {
            return {.error = CoveragePlanningError::InvalidGeneratedPath};
        }
        if (!Geometry::segmentInsidePolygonRegionForValidatedGeometry(regions.regions.hardExecutionTrackRegion,
                                                                      path[i - 1], path[i])) {
            return {.error = CoveragePlanningError::UnsafeConnector};
        }
        const bool preferred = Geometry::segmentInsidePolygonRegionForValidatedGeometry(
            regions.regions.preferredExecutionTrackRegion, path[i - 1], path[i]);
        result.legs.push_back(preferred ? SafetyLegClass::PreferredSafe : SafetyLegClass::HardSafeWarning);
        if (!preferred) {
            result.tier = SafetySolutionTier::D1;
        }
    }
    return result;
}

SafetyCandidateSelection selectPreferredOrHardCandidate(const SafetyTrackRegionsResult& regions,
                                                        std::span<const Point2D> preferredCandidate,
                                                        std::span<const Point2D> hardCandidate)
{
    auto preferred = evaluateSafetyCandidate(regions, preferredCandidate);
    if (preferred.error == CoveragePlanningError::None && preferred.tier == SafetySolutionTier::D0) {
        return {.assessment = std::move(preferred), .path = {preferredCandidate.begin(), preferredCandidate.end()}};
    }
    auto hard = evaluateSafetyCandidate(regions, hardCandidate);
    if (hard.error == CoveragePlanningError::None) {
        return {.assessment = std::move(hard),
                .path = {hardCandidate.begin(), hardCandidate.end()},
                .usedHardFallback = true};
    }
    // A supplied preferred attempt can itself be retained as a hard-safe fallback after full certification.
    if (preferred.error == CoveragePlanningError::None) {
        return {.assessment = std::move(preferred),
                .path = {preferredCandidate.begin(), preferredCandidate.end()},
                .usedHardFallback = true};
    }
    return {.assessment = std::move(hard)};
}

}  // namespace Marine
