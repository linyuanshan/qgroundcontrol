#include "CoverageQualityEvaluator.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>
#include <vector>

#include "CoverageQualityPolicy.h"
#include "Geometry/PolygonRegion.h"
#include "PlanningPathMetrics.h"

namespace Marine {
namespace {

constexpr double MinimumNumericalCoverageToleranceM2 = 0.01;
constexpr double RelativeNumericalCoverageTolerance = 1e-6;

double areaTolerance(double areaM2)
{
    return std::max(MinimumNumericalCoverageToleranceM2, RelativeNumericalCoverageTolerance * areaM2);
}

CoverageQualityEvaluation assessmentError(CoverageQualityEvaluation result, CoverageQualityError error,
                                          std::string message)
{
    result.status = CoverageQualityStatus::AssessmentError;
    result.error = error;
    result.passesRequirement = false;
    result.message = std::move(message);
    return result;
}

bool validRequirement(CoverageRequirement requirement)
{
    return requirement == CoverageRequirement::Standard || requirement == CoverageRequirement::Strict;
}

bool validComparableStatus(CoverageQualityStatus status)
{
    return status == CoverageQualityStatus::Complete || status == CoverageQualityStatus::Acceptable ||
           status == CoverageQualityStatus::Insufficient;
}

bool validMetrics(const CoverageQualityEvaluation& result)
{
    return result.error == CoverageQualityError::None && validComparableStatus(result.status) &&
           std::isfinite(result.targetAreaM2) && (result.targetAreaM2 > 0.0) && std::isfinite(result.coveredAreaM2) &&
           (result.coveredAreaM2 >= 0.0) && std::isfinite(result.uncoveredAreaM2) && (result.uncoveredAreaM2 >= 0.0) &&
           std::isfinite(result.coverageRatio) && (result.coverageRatio >= 0.0) &&
           std::isfinite(result.criticalUncoveredAreaM2) && (result.criticalUncoveredAreaM2 >= 0.0) &&
           std::isfinite(result.numericalToleranceM2) && (result.numericalToleranceM2 > 0.0);
}

long double quantize(double value, double tolerance)
{
    return std::floor(static_cast<long double>(value) / static_cast<long double>(tolerance) + 0.5L);
}

}  // namespace

CoverageQualityEvaluation evaluateCoverageQuality(const PolygonRegionSet2D& coverageTarget,
                                                  std::span<const Point2D> path, std::span<const PathLegRole> legRoles,
                                                  double swathWidthM, CoverageRequirement requirement,
                                                  const PlannerStrategyIdentity& strategy,
                                                  std::string_view policySemanticVersion)
{
    CoverageQualityEvaluation result;
    result.requirement = requirement;
    result.strategy = strategy;
    result.policySemanticVersion = policySemanticVersion;

    if (coverageTarget.empty() || !std::ranges::all_of(coverageTarget, [](const PolygonRegion2D& region) {
            return Geometry::isValidPolygonRegion(region);
        })) {
        return assessmentError(std::move(result), CoverageQualityError::InvalidTarget, "Coverage target is invalid");
    }
    const Geometry::PolygonRegionAreaResult targetArea = Geometry::polygonRegionArea(coverageTarget);
    if (targetArea.status != Geometry::PolygonRegionOperationStatus::Success) {
        return assessmentError(std::move(result), CoverageQualityError::GeometryFailure,
                               "Coverage target area calculation failed");
    }
    if (!std::isfinite(targetArea.areaM2) || (targetArea.areaM2 <= 0.0)) {
        return assessmentError(std::move(result), CoverageQualityError::InvalidTarget,
                               "Coverage target must have positive finite area");
    }
    result.targetAreaM2 = targetArea.areaM2;
    result.numericalToleranceM2 = areaTolerance(targetArea.areaM2);
    result.availability.targetArea = true;
    result.availability.numericalTolerance = true;

    if (!std::isfinite(swathWidthM) || (swathWidthM <= 0.0)) {
        return assessmentError(std::move(result), CoverageQualityError::InvalidSwathWidth,
                               "Coverage swath width must be finite and greater than zero");
    }
    if (!validRequirement(requirement)) {
        return assessmentError(std::move(result), CoverageQualityError::InvalidRequirement,
                               "Coverage requirement must be Standard or Strict");
    }
    if (policySemanticVersion != CoverageQualityPolicySemanticVersion) {
        return assessmentError(std::move(result), CoverageQualityError::UnsupportedPolicySemantics,
                               "Coverage quality policy semantics are unsupported");
    }
    if (strategy.strategyId.empty() || strategy.semanticVersion.empty()) {
        return assessmentError(std::move(result), CoverageQualityError::InvalidStrategy,
                               "Planner strategy identity must be non-empty");
    }
    if (!calculatePlanningPathMetrics(path, legRoles).has_value()) {
        return assessmentError(std::move(result), CoverageQualityError::InvalidPath,
                               "Coverage path and leg roles are invalid");
    }

    std::vector<Geometry::LineSegment2D> coverageSegments;
    coverageSegments.reserve(legRoles.size());
    for (std::size_t index = 0; index < legRoles.size(); ++index) {
        if (legRoles[index] == PathLegRole::Coverage) {
            coverageSegments.push_back({.start = path[index], .end = path[index + 1]});
        }
    }
    const Geometry::PolygonRegionOperationResult footprint =
        Geometry::bufferLineSegments(coverageSegments, swathWidthM / 2.0);
    if (footprint.status != Geometry::PolygonRegionOperationStatus::Success) {
        return assessmentError(std::move(result), CoverageQualityError::GeometryFailure,
                               "Coverage footprint buffering failed");
    }
    const Geometry::PolygonRegionOperationResult uncovered =
        Geometry::differencePolygonRegions(coverageTarget, footprint.regions);
    if (uncovered.status != Geometry::PolygonRegionOperationStatus::Success) {
        return assessmentError(std::move(result), CoverageQualityError::GeometryFailure,
                               "Coverage residual boolean operation failed");
    }
    result.residual.uncoveredRegion = uncovered.regions;
    result.availability.uncovered = true;
    const Geometry::PolygonRegionAreaResult uncoveredArea = Geometry::polygonRegionArea(uncovered.regions);
    if (uncoveredArea.status != Geometry::PolygonRegionOperationStatus::Success) {
        return assessmentError(std::move(result), CoverageQualityError::GeometryFailure,
                               "Coverage residual area calculation failed");
    }
    double uncoveredM2 = uncoveredArea.areaM2;
    const double toleranceM2 = result.numericalToleranceM2;
    if (!std::isfinite(uncoveredM2) || (uncoveredM2 < 0.0) || (uncoveredM2 > result.targetAreaM2 + toleranceM2)) {
        return assessmentError(std::move(result), CoverageQualityError::NumericalFailure,
                               "Coverage residual area is numerically inconsistent");
    }
    uncoveredM2 = std::clamp(uncoveredM2, 0.0, result.targetAreaM2);
    // Use one residual truth rather than independent boolean operations with different lattice rounding.
    const double coveredM2 = result.targetAreaM2 - uncoveredM2;
    result.coveredAreaM2 = coveredM2;
    result.uncoveredAreaM2 = uncoveredM2;
    result.coverageRatio = coveredM2 / result.targetAreaM2;
    result.availability.coveredArea = true;
    result.availability.uncoveredArea = true;
    result.availability.ratio = true;

    bool componentFallbacksPass = true;
    for (const PolygonRegion2D& component : coverageTarget) {
        const PolygonRegionSet2D singleComponent{component};
        const Geometry::PolygonRegionAreaResult componentArea = Geometry::polygonRegionArea(singleComponent);
        if (componentArea.status != Geometry::PolygonRegionOperationStatus::Success ||
            !std::isfinite(componentArea.areaM2) || (componentArea.areaM2 <= 0.0)) {
            return assessmentError(std::move(result), CoverageQualityError::GeometryFailure,
                                   "Coverage component area calculation failed");
        }
        const double componentToleranceM2 = areaTolerance(componentArea.areaM2);
        const Geometry::PolygonRegionOperationResult componentCore =
            Geometry::insetPolygonRegions(singleComponent, StandardCoveragePolicy::boundaryToleranceM);
        if (componentCore.status != Geometry::PolygonRegionOperationStatus::Success) {
            return assessmentError(std::move(result), CoverageQualityError::GeometryFailure,
                                   "Critical coverage core inset failed");
        }
        const Geometry::PolygonRegionAreaResult coreArea = Geometry::polygonRegionArea(componentCore.regions);
        if (coreArea.status != Geometry::PolygonRegionOperationStatus::Success || !std::isfinite(coreArea.areaM2)) {
            return assessmentError(std::move(result), CoverageQualityError::GeometryFailure,
                                   "Critical coverage core area calculation failed");
        }
        if (componentCore.regions.empty() || (coreArea.areaM2 <= componentToleranceM2)) {
            result.strictFallbackTriggered = true;
            result.residual.strictFallbackTargetComponents.push_back(component);
            const Geometry::PolygonRegionOperationResult componentUncovered =
                Geometry::differencePolygonRegions(singleComponent, footprint.regions);
            if (componentUncovered.status != Geometry::PolygonRegionOperationStatus::Success) {
                return assessmentError(std::move(result), CoverageQualityError::GeometryFailure,
                                       "Strict fallback residual operation failed");
            }
            const Geometry::PolygonRegionAreaResult componentUncoveredArea =
                Geometry::polygonRegionArea(componentUncovered.regions);
            if (componentUncoveredArea.status != Geometry::PolygonRegionOperationStatus::Success ||
                !std::isfinite(componentUncoveredArea.areaM2)) {
                return assessmentError(std::move(result), CoverageQualityError::GeometryFailure,
                                       "Strict fallback residual area calculation failed");
            }
            componentFallbacksPass &= componentUncoveredArea.areaM2 <= componentToleranceM2;
        } else {
            result.residual.criticalCoverageCore.insert(result.residual.criticalCoverageCore.end(),
                                                        componentCore.regions.begin(), componentCore.regions.end());
        }
    }

    const Geometry::PolygonRegionOperationResult normalizedCore =
        Geometry::unionPolygonRegions(result.residual.criticalCoverageCore);
    if (normalizedCore.status != Geometry::PolygonRegionOperationStatus::Success) {
        return assessmentError(std::move(result), CoverageQualityError::GeometryFailure,
                               "Critical coverage core union failed");
    }
    result.residual.criticalCoverageCore = normalizedCore.regions;
    result.availability.criticalCore = true;
    result.availability.fallbackComponents = true;
    const Geometry::PolygonRegionAreaResult combinedCoreArea =
        Geometry::polygonRegionArea(result.residual.criticalCoverageCore);
    if (combinedCoreArea.status != Geometry::PolygonRegionOperationStatus::Success ||
        !std::isfinite(combinedCoreArea.areaM2)) {
        return assessmentError(std::move(result), CoverageQualityError::GeometryFailure,
                               "Combined critical coverage core area calculation failed");
    }
    if (result.residual.criticalCoverageCore.empty() || (combinedCoreArea.areaM2 <= toleranceM2)) {
        result.wholeTargetStrictFallback = true;
        result.strictFallbackTriggered = true;
        result.residual.strictFallbackTargetComponents = coverageTarget;
        componentFallbacksPass &= uncoveredM2 <= toleranceM2;
    }

    const Geometry::PolygonRegionOperationResult boundaryShortfall =
        Geometry::differencePolygonRegions(uncovered.regions, result.residual.criticalCoverageCore);
    if (boundaryShortfall.status != Geometry::PolygonRegionOperationStatus::Success) {
        return assessmentError(std::move(result), CoverageQualityError::GeometryFailure,
                               "Boundary shortfall difference failed");
    }
    result.residual.boundaryShortfallRegion = boundaryShortfall.regions;
    result.availability.boundaryShortfall = true;

    // Reconstruct the critical residual relative to the fixed uncovered subject, not a generic intersection.
    const Geometry::PolygonRegionOperationResult criticalUncovered =
        Geometry::differencePolygonRegions(uncovered.regions, boundaryShortfall.regions);
    if (criticalUncovered.status != Geometry::PolygonRegionOperationStatus::Success) {
        return assessmentError(std::move(result), CoverageQualityError::GeometryFailure,
                               "Critical uncovered reconstruction failed");
    }
    result.residual.criticalUncoveredRegion = criticalUncovered.regions;
    result.availability.criticalUncovered = true;
    const Geometry::PolygonRegionAreaResult criticalArea = Geometry::polygonRegionArea(criticalUncovered.regions);
    if (criticalArea.status != Geometry::PolygonRegionOperationStatus::Success) {
        return assessmentError(std::move(result), CoverageQualityError::GeometryFailure,
                               "Critical uncovered area calculation failed");
    }
    if (!std::isfinite(criticalArea.areaM2) || (criticalArea.areaM2 < 0.0) ||
        (criticalArea.areaM2 > uncoveredM2 + toleranceM2)) {
        return assessmentError(std::move(result), CoverageQualityError::NumericalFailure,
                               "Critical uncovered area is numerically inconsistent");
    }
    const Geometry::PolygonRegionAreaResult boundaryArea = Geometry::polygonRegionArea(boundaryShortfall.regions);
    if (boundaryArea.status != Geometry::PolygonRegionOperationStatus::Success) {
        return assessmentError(std::move(result), CoverageQualityError::GeometryFailure,
                               "Boundary shortfall area calculation failed");
    }
    if (!std::isfinite(boundaryArea.areaM2) || (boundaryArea.areaM2 < 0.0) ||
        (boundaryArea.areaM2 > uncoveredM2 + toleranceM2)) {
        return assessmentError(std::move(result), CoverageQualityError::NumericalFailure,
                               "Boundary shortfall area is numerically inconsistent");
    }
    result.criticalUncoveredAreaM2 = std::clamp(criticalArea.areaM2, 0.0, uncoveredM2);
    result.availability.criticalUncoveredArea = true;

    if (uncoveredM2 <= toleranceM2) {
        result.status = CoverageQualityStatus::Complete;
        result.passesRequirement = true;
        result.message = "Coverage is numerically complete";
    } else if (requirement == CoverageRequirement::Strict) {
        result.status = CoverageQualityStatus::Insufficient;
        result.passesRequirement = false;
        result.message = "Strict coverage requirement failed";
    } else {
        result.passesRequirement = (result.coverageRatio >= StandardCoveragePolicy::minimumCoverageRatio) &&
                                   (result.criticalUncoveredAreaM2 <= toleranceM2) && componentFallbacksPass;
        result.status =
            result.passesRequirement ? CoverageQualityStatus::Acceptable : CoverageQualityStatus::Insufficient;
        result.message = result.passesRequirement ? "Coverage satisfies the Standard policy"
                                                  : "Coverage does not satisfy the Standard policy";
    }
    return result;
}

CoverageQualityComparison compareCoverageQuality(const CoverageQualityEvaluation& left,
                                                 const CoverageQualityEvaluation& right)
{
    if (!validMetrics(left) || !validMetrics(right) || (left.policySemanticVersion != right.policySemanticVersion) ||
        (left.numericalToleranceM2 != right.numericalToleranceM2)) {
        return CoverageQualityComparison::NotComparable;
    }
    const long double leftCritical = quantize(left.criticalUncoveredAreaM2, left.numericalToleranceM2);
    const long double rightCritical = quantize(right.criticalUncoveredAreaM2, right.numericalToleranceM2);
    if (leftCritical != rightCritical) {
        return leftCritical < rightCritical ? CoverageQualityComparison::Better : CoverageQualityComparison::Worse;
    }
    const long double leftUncovered = quantize(left.uncoveredAreaM2, left.numericalToleranceM2);
    const long double rightUncovered = quantize(right.uncoveredAreaM2, right.numericalToleranceM2);
    if (leftUncovered == rightUncovered) {
        return CoverageQualityComparison::Equivalent;
    }
    return leftUncovered < rightUncovered ? CoverageQualityComparison::Better : CoverageQualityComparison::Worse;
}

}  // namespace Marine
