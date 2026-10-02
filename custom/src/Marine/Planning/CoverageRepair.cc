#include "CoverageRepair.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "CoverageRepairSupport.h"
#include "Geometry/MarineGeometry.h"
#include "StaticSafeRouter.h"

namespace Marine {
namespace {

bool samePoint(const Point2D& left, const Point2D& right)
{
    return left.xM == right.xM && left.yM == right.yM;
}

void appendPoint(CoverageRepairCandidate& candidate, const Point2D& point, PathLegRole role)
{
    if (!samePoint(candidate.path.back(), point)) {
        candidate.path.push_back(point);
        candidate.legRoles.push_back(role);
    }
}

}  // namespace

std::optional<CoverageRepairStep> evaluateCoverageRepairTrial(
    const PolygonRegionSet2D& target, const PolygonRegionSet2D& active, const SafetyTrackRegionsResult& safety,
    double swath, CoverageRequirement requirement, const PlannerStrategyIdentity& strategy,
    const CoverageRepairCandidate& current, const BoundaryCoverageComponent& component, std::size_t entry, bool reverse)
{
    if (current.quality.status != CoverageQualityStatus::Insufficient ||
        current.quality.error != CoverageQualityError::None ||
        compareCoverageQuality(current.quality, current.quality) != CoverageQualityComparison::Equivalent ||
        current.path.empty() || component.path.size() < 4 || entry >= component.path.size() - 1 ||
        !samePoint(component.path.front(), component.path.back())) {
        return std::nullopt;
    }
    const StaticRoute route = routeStatic(active, current.path.back(), component.path[entry]);
    if (route.status != PlanningStatus::Success || route.error != CoveragePlanningError::None || route.path.empty() ||
        !samePoint(route.path.front(), current.path.back()) || !samePoint(route.path.back(), component.path[entry])) {
        return std::nullopt;
    }
    CoverageRepairStep trial{.componentId = component.id,
                             .componentPath = component.path,
                             .entryIndex = entry,
                             .reverse = reverse,
                             .transitionCostM = route.lengthM,
                             .before = current.quality,
                             .after = current};
    for (std::size_t index = 1; index < route.path.size(); ++index) {
        appendPoint(trial.after, route.path[index], PathLegRole::Transit);
    }
    const std::size_t count = component.path.size() - 1;
    for (std::size_t step = 1; step <= count; ++step) {
        const std::size_t index = reverse ? (entry + count - step) % count : (entry + step) % count;
        appendPoint(trial.after, component.path[index], PathLegRole::Coverage);
    }
    const auto metrics = calculatePlanningPathMetrics(trial.after.path, trial.after.legRoles);
    const auto assessment = evaluateSafetyCandidate(safety, trial.after.path);
    if (!metrics || assessment.error != CoveragePlanningError::None) {
        return std::nullopt;
    }
    // Also certify the ENTIRE path against the active tier, including the original path and routed connector.
    for (std::size_t index = 1; index < trial.after.path.size(); ++index) {
        if (!Geometry::segmentInsidePolygonRegionForValidatedGeometry(active, trial.after.path[index - 1],
                                                                      trial.after.path[index])) {
            return std::nullopt;
        }
    }
    trial.after.metrics = *metrics;
    trial.after.preferredSafe = assessment.tier == SafetySolutionTier::D0;
    trial.after.quality = evaluateCoverageQuality(target, trial.after.path, trial.after.legRoles, swath, requirement,
                                                  strategy, current.quality.policySemanticVersion);
    if (compareCoverageQuality(trial.after.quality, current.quality) != CoverageQualityComparison::Better) {
        return std::nullopt;
    }
    return trial;
}

bool coveragePolicyPass(const CoverageQualityEvaluation& quality)
{
    return quality.error == CoverageQualityError::None &&
           (quality.status == CoverageQualityStatus::Complete || quality.status == CoverageQualityStatus::Acceptable) &&
           quality.passesRequirement;
}

bool coverageRepairTrialBetter(const CoverageRepairStep& left, const CoverageRepairStep& right)
{
    const auto comparison = compareCoverageQuality(left.after.quality, right.after.quality);
    if (comparison != CoverageQualityComparison::Equivalent) {
        return comparison == CoverageQualityComparison::Better;
    }
    if (left.transitionCostM != right.transitionCostM) {
        return left.transitionCostM < right.transitionCostM;
    }
    if (left.componentId != right.componentId) {
        return left.componentId < right.componentId;
    }
    if (left.entryIndex != right.entryIndex) {
        return left.entryIndex < right.entryIndex;
    }
    return left.reverse < right.reverse;
}

CoverageRepairResult repairCoverageCandidate(const PolygonRegionSet2D& target,
                                             const PolygonRegionSet2D& activeExecutionRegion,
                                             const SafetyTrackRegionsResult& safetyRegions, double swathWidthM,
                                             CoverageRequirement requirement, const PlannerStrategyIdentity& strategy,
                                             const CoverageRepairCandidate& initial)
{
    CoverageRepairResult result{.candidate = initial};
    // Check status BEFORE accessing any failed assessment metrics/residuals. Not a policy failure.
    if (initial.quality.status != CoverageQualityStatus::Insufficient ||
        initial.quality.error != CoverageQualityError::None ||
        compareCoverageQuality(initial.quality, initial.quality) != CoverageQualityComparison::Equivalent ||
        !calculatePlanningPathMetrics(initial.path, initial.legRoles) ||
        evaluateSafetyCandidate(safetyRegions, initial.path).error != CoveragePlanningError::None) {
        return result;
    }
    result.attempted = true;
    const auto support = generateCoverageRepairSupport(target, activeExecutionRegion);
    if (support.status != PlanningStatus::Success) {
        return result;
    }
    result.availableComponentCount = support.components.size();
    std::vector<bool> used(support.components.size(), false);
    // The fixed finite set of logical rings is generated once. Each accepted step consumes ONE ring.
    // Rejected rings may become useful later, but at most |components| accepted steps can occur.
    while (!coveragePolicyPass(result.candidate.quality)) {
        std::optional<CoverageRepairStep> best;
        std::size_t bestComponent = 0;
        for (std::size_t index = 0; index < support.components.size(); ++index) {
            if (used[index]) {
                continue;
            }
            const auto& component = support.components[index];
            std::vector<Geometry::LineSegment2D> legs;
            for (std::size_t leg = 1; leg < component.path.size(); ++leg) {
                legs.push_back({component.path[leg - 1], component.path[leg]});
            }
            const auto footprint = Geometry::bufferLineSegments(legs, swathWidthM / 2.0);
            if (footprint.status != Geometry::PolygonRegionOperationStatus::Success) {
                continue;
            }
            const auto remainder = Geometry::differencePolygonRegions(result.candidate.quality.residual.uncoveredRegion,
                                                                      footprint.regions);
            const auto remainingArea = Geometry::polygonRegionArea(remainder.regions);
            if (remainder.status != Geometry::PolygonRegionOperationStatus::Success ||
                remainingArea.status != Geometry::PolygonRegionOperationStatus::Success ||
                remainingArea.areaM2 >= result.candidate.quality.uncoveredAreaM2) {
                continue;
            }
            // Reliable U drives eligibility only. This is NOT a quality score: every trial still
            // must improve the frozen comparator after complete safety and coverage evaluation.
            for (std::size_t entry = 0; entry + 1 < component.path.size(); ++entry) {
                for (const bool reverse : {false, true}) {
                    auto trial =
                        evaluateCoverageRepairTrial(target, activeExecutionRegion, safetyRegions, swathWidthM,
                                                    requirement, strategy, result.candidate, component, entry, reverse);
                    if (trial && (!best || coverageRepairTrialBetter(*trial, *best))) {
                        best = std::move(trial);
                        bestComponent = index;
                    }
                }
            }
        }
        if (!best) {
            break;
        }
        used[bestComponent] = true;
        result.candidate = best->after;
        result.steps.push_back(std::move(*best));
    }
    return result;
}

}  // namespace Marine
