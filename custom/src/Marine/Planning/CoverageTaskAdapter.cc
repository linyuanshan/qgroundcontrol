#include "CoverageTaskAdapter.h"

#include <cmath>
#include <utility>

#include "CoverageProblemValidator.h"
#include "CoverageSafety.h"
#include "PlanningPathMetrics.h"

namespace {

bool toLocalPolygon(const Marine::GeoPolygon& geoPolygon, const Marine::GeoReference& geoReference,
                    Marine::Polygon2D& localPolygon)
{
    Marine::Polygon2D converted;
    const auto vertexCount = Marine::openRingVertexCount(geoPolygon);
    converted.vertices.reserve(vertexCount);
    for (std::size_t index = 0; index < vertexCount; ++index) {
        const Marine::GeoPoint& point = geoPolygon.vertices[index];
        const std::optional<Marine::Point2D> localPoint = geoReference.toLocal(point);
        if (!localPoint.has_value()) {
            return false;
        }
        converted.vertices.push_back(*localPoint);
    }

    localPolygon = std::move(converted);
    return true;
}

bool toGeoPath(const std::vector<Marine::Point2D>& local, const Marine::GeoReference& reference,
               std::vector<Marine::GeoPoint>& geo)
{
    geo.clear();
    for (const auto& point : local) {
        const auto converted = reference.toGeo(point);
        if (!converted) {
            return false;
        }
        geo.push_back(*converted);
    }
    return true;
}

bool toGeoRegions(const Marine::PolygonRegionSet2D& local, const Marine::GeoReference& reference,
                  Marine::GeoPolygonRegionSet& geo)
{
    for (const auto& region : local) {
        Marine::GeoPolygonRegion converted;
        if (!toGeoPath(region.outerBoundary.vertices, reference, converted.outerBoundary.vertices)) {
            return false;
        }
        for (const auto& hole : region.holes) {
            Marine::GeoPolygon convertedHole;
            if (!toGeoPath(hole.vertices, reference, convertedHole.vertices)) {
                return false;
            }
            converted.holes.push_back(std::move(convertedHole));
        }
        geo.push_back(std::move(converted));
    }
    return true;
}

bool toGeoQuality(const Marine::CoverageQualityEvaluation& local, const Marine::GeoReference& reference,
                  Marine::GeoCoverageQualityEvaluation& geo)
{
    geo.status = local.status;
    geo.error = local.error;
    geo.strategy = local.strategy;
    geo.requirement = local.requirement;
    geo.policySemanticVersion = local.policySemanticVersion;
    geo.targetAreaM2 = local.targetAreaM2;
    geo.coveredAreaM2 = local.coveredAreaM2;
    geo.uncoveredAreaM2 = local.uncoveredAreaM2;
    geo.coverageRatio = local.coverageRatio;
    geo.criticalUncoveredAreaM2 = local.criticalUncoveredAreaM2;
    geo.numericalToleranceM2 = local.numericalToleranceM2;
    geo.passesRequirement = local.passesRequirement;
    geo.strictFallbackTriggered = local.strictFallbackTriggered;
    geo.wholeTargetStrictFallback = local.wholeTargetStrictFallback;
    geo.availability = local.availability;
    geo.message = local.message;
    const auto convert = [&](bool available, const Marine::PolygonRegionSet2D& input,
                             Marine::GeoPolygonRegionSet& output) {
        return !available || toGeoRegions(input, reference, output);
    };
    return convert(local.availability.criticalCore, local.residual.criticalCoverageCore,
                   geo.residual.criticalCoverageCore) &&
           convert(local.availability.uncovered, local.residual.uncoveredRegion, geo.residual.uncoveredRegion) &&
           convert(local.availability.criticalUncovered, local.residual.criticalUncoveredRegion,
                   geo.residual.criticalUncoveredRegion) &&
           convert(local.availability.boundaryShortfall, local.residual.boundaryShortfallRegion,
                   geo.residual.boundaryShortfallRegion) &&
           convert(local.availability.fallbackComponents, local.residual.strictFallbackTargetComponents,
                   geo.residual.strictFallbackTargetComponents);
}

bool toGeoOutcome(const Marine::PlanningOutcome<Marine::Point2D, Marine::PolygonRegionSet2D>& local,
                  const Marine::GeoReference& reference,
                  Marine::PlanningOutcome<Marine::GeoPoint, Marine::GeoPolygonRegionSet>& geo)
{
    geo.readiness = local.readiness;
    geo.tier = local.tier;
    geo.canonicalLegAssessments = local.canonicalLegAssessments;
    geo.selectedCandidateIndex = local.selectedCandidateIndex;
    geo.issues = local.issues;
    geo.suggestions = local.suggestions;
    if (local.coverageQuality) {
        geo.coverageQuality.emplace();
        if (!toGeoQuality(*local.coverageQuality, reference, *geo.coverageQuality)) {
            return false;
        }
    }
    geo.repair.attempted = local.repair.attempted;
    geo.repair.applied = local.repair.applied;
    geo.repair.reason = local.repair.reason;
    for (const auto& component : local.repair.components) {
        Marine::AppliedRepairComponent<Marine::GeoPoint, Marine::GeoPolygonRegionSet> converted;
        converted.componentId = component.componentId;
        converted.entryIndex = component.entryIndex;
        converted.reverse = component.reverse;
        converted.transitionCostM = component.transitionCostM;
        converted.pathLengthBeforeM = component.pathLengthBeforeM;
        converted.pathLengthAfterM = component.pathLengthAfterM;
        converted.turnCountBefore = component.turnCountBefore;
        converted.turnCountAfter = component.turnCountAfter;
        if (!toGeoPath(component.componentPath, reference, converted.componentPath) ||
            !toGeoQuality(component.before, reference, converted.before) ||
            !toGeoQuality(component.after, reference, converted.after)) {
            return false;
        }
        geo.repair.components.push_back(std::move(converted));
    }
    if (local.diagnosticCandidate) {
        const auto& input = *local.diagnosticCandidate;
        Marine::DiagnosticCandidate<Marine::GeoPoint> converted{.identity = input.identity,
                                                                .legRoles = input.legRoles,
                                                                .legAssessments = input.legAssessments,
                                                                .coverageLengthM = input.coverageLengthM,
                                                                .transitLengthM = input.transitLengthM,
                                                                .pathLengthM = input.pathLengthM,
                                                                .turnCount = input.turnCount};
        if (!toGeoPath(input.path, reference, converted.path)) {
            return false;
        }
        geo.diagnosticCandidate = std::move(converted);
    }
    for (const auto& overlay : local.diagnosticOverlays) {
        Marine::DiagnosticOverlay<Marine::GeoPolygonRegionSet> converted{.kind = overlay.kind,
                                                                         .explanation = overlay.explanation};
        if (!toGeoRegions(overlay.geometry, reference, converted.geometry)) {
            return false;
        }
        geo.diagnosticOverlays.push_back(std::move(converted));
    }
    return true;
}

}  // namespace

namespace Marine {

bool CoverageTaskAdapter::buildProblem(const MarineTask& task, CoveragePlanningProblem& problem,
                                       std::optional<GeoReference>& geoReference, CoveragePlanningError& error)
{
    problem = {};
    geoReference.reset();
    error = CoveragePlanningError::None;
    error = validateSafetyMargins(task.safety, task.planner.executionSafety);
    if (error != CoveragePlanningError::None) {
        return false;
    }
    if (!task.isValid()) {
        error = CoveragePlanningError::InvalidOuterBoundary;
        return false;
    }

    std::optional<GeoReference> reference = GeoReference::create(task.region.coverageBoundary);
    if (!reference.has_value()) {
        error = CoveragePlanningError::InvalidOuterBoundary;
        return false;
    }

    CoveragePlanningProblem converted;
    if (!toLocalPolygon(task.region.coverageBoundary, *reference, converted.region.coverageBoundary)) {
        error = CoveragePlanningError::InvalidOuterBoundary;
        return false;
    }
    if (!toLocalPolygon(task.region.navigationBoundary, *reference, converted.region.navigationBoundary)) {
        error = CoveragePlanningError::InvalidNavigationBoundary;
        return false;
    }

    converted.region.noGoRegions.reserve(task.region.noGoRegions.size());
    for (const GeoPolygon& noGoRegion : task.region.noGoRegions) {
        Polygon2D localNoGo;
        if (!toLocalPolygon(noGoRegion, *reference, localNoGo)) {
            error = CoveragePlanningError::InvalidNoGoRegion;
            return false;
        }
        converted.region.noGoRegions.push_back(std::move(localNoGo));
    }

    converted.swathWidthM = task.coverage.swathWidthM;
    converted.safety = task.safety;
    converted.executionSafety = task.planner.executionSafety;
    converted.coverageRequirement = task.coverage.coverageRequirement;
    converted.sweepAngleMode = task.coverage.sweepAngleMode;
    converted.requestedSweepAngleDeg = task.coverage.sweepAngleDeg;

    error = CoverageProblemValidator::validateAndNormalize(converted);
    if (error != CoveragePlanningError::None) {
        return false;
    }
    problem = std::move(converted);
    geoReference = std::move(reference);
    return true;
}

PlanningResult CoverageTaskAdapter::toPlanningResult(const CoveragePlanningSolution& solution,
                                                     const GeoReference& geoReference)
{
    PlanningResult result;
    result.status = solution.status;
    result.error = solution.error;
    result.message = solution.message;
    result.plannerSource = solution.plannerSource;
    if (!toGeoOutcome(solution.outcome, geoReference, result.outcome)) {
        result = {};
        result.message = "Planning outcome contains an invalid local coordinate";
        return result;
    }
    result.selectedSweepAngleDeg = solution.selectedSweepAngleDeg;
    result.cellCount = solution.cellCount;
    result.turnCount = solution.turnCount;
    if (solution.path.empty()) {
        return result;
    }
    const auto metrics = calculatePlanningPathMetrics(solution.path, solution.legRoles);
    if (!metrics ||
        !planningPathMetricsMatch(*metrics, solution.coverageLengthM, solution.transitLengthM, solution.pathLengthM)) {
        result = {};
        result.message = "Coverage solution contains an invalid path or inconsistent path metrics";
        return result;
    }

    result.path.reserve(solution.path.size());
    for (const Point2D& point : solution.path) {
        const std::optional<GeoPoint> geoPoint = geoReference.toGeo(point);
        if (!geoPoint.has_value()) {
            result = {};
            result.message = "Coverage solution contains an invalid local coordinate";
            return result;
        }
        result.path.push_back(*geoPoint);
    }

    result.legRoles = solution.legRoles;
    result.coverageLengthM = solution.coverageLengthM;
    result.transitLengthM = solution.transitLengthM;
    result.pathLengthM = solution.pathLengthM;
    result.selectedSweepAngleDeg = solution.selectedSweepAngleDeg;
    result.cellCount = solution.cellCount;
    result.turnCount = solution.turnCount;
    return result;
}

}  // namespace Marine
