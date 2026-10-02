#include "IntegratedPlanningResult.h"

#include <algorithm>
#include <utility>

#include "CoverageRepair.h"
#include "Geometry/MarineGeometry.h"

namespace Marine {

void publishCanonicalOutcome(CoveragePlanningSolution& result, const SafetyCandidateAssessment& safety,
                             std::size_t selectedIndex, const CoverageRepairResult& repair,
                             const PlanningPathMetrics& initialMetrics, bool initialPolicyPass)
{
    auto& outcome = result.outcome;
    result.status = PlanningStatus::Success;
    result.error = CoveragePlanningError::None;
    outcome.tier = safety.tier;
    outcome.canonicalLegAssessments = safety.legs;
    outcome.selectedCandidateIndex = selectedIndex;
    outcome.readiness =
        coveragePolicyPass(*result.outcome.coverageQuality)
            ? (safety.tier == SafetySolutionTier::D0 ? MissionReadiness::Ready : MissionReadiness::ReadyWithWarning)
            : MissionReadiness::ReviewRequired;
    outcome.repair.attempted = repair.attempted;
    outcome.repair.applied = !repair.steps.empty();
    outcome.repair.reason = initialPolicyPass ? CoverageRepairReason::InitialPolicyPass
                            : result.outcome.coverageQuality->status == CoverageQualityStatus::AssessmentError
                                ? CoverageRepairReason::AssessmentUnavailable
                            : repair.steps.empty() ? CoverageRepairReason::NoUsefulRepair
                            : coveragePolicyPass(repair.candidate.quality)
                                ? CoverageRepairReason::AppliedPolicyPass
                                : CoverageRepairReason::AppliedStillInsufficient;
    PlanningPathMetrics previous = initialMetrics;
    for (const auto& step : repair.steps) {
        outcome.repair.components.push_back({step.componentId, step.componentPath, step.entryIndex, step.reverse,
                                             step.transitionCostM, step.before, step.after.quality,
                                             previous.pathLengthM, step.after.metrics.pathLengthM, previous.turnCount,
                                             step.after.metrics.turnCount});
        previous = step.after.metrics;
    }
    populatePlanningAdvice(result);
}

void populatePlanningAdvice(CoveragePlanningSolution& result)
{
    auto& outcome = result.outcome;
    outcome.issues.clear();
    outcome.suggestions.clear();
    const auto issue = [&](PlanningIssueCode code, PlanningIssueSeverity severity, const char* message,
                           std::optional<PlanningReference> reference = std::nullopt) {
        outcome.issues.push_back({code, severity, message, reference});
    };
    if (result.plannerSource && result.plannerSource->escalated) {
        issue(PlanningIssueCode::PlannerEscalated, PlanningIssueSeverity::Info,
              "Target topology required the resolved decomposition strategy");
    }
    if (outcome.tier == SafetySolutionTier::D1) {
        const auto first = std::ranges::find(outcome.canonicalLegAssessments, SafetyLegClass::HardSafeWarning);
        const auto index = static_cast<std::size_t>(first - outcome.canonicalLegAssessments.begin());
        issue(
            PlanningIssueCode::PreferredSafetyViolated, PlanningIssueSeverity::Warning,
            "Hard clearance is certified; preferred clearance is not satisfied on every leg",
            PlanningReference{.kind = PlanningReferenceKind::CanonicalPathLegRange, .firstLeg = index, .legCount = 1});
    }
    if (outcome.coverageQuality) {
        const auto& quality = *outcome.coverageQuality;
        if (quality.status == CoverageQualityStatus::AssessmentError) {
            issue(PlanningIssueCode::CoverageAssessmentFailed, PlanningIssueSeverity::Blocking,
                  "Coverage assessment could not be completed reliably");
        } else {
            const auto residualRef = [](PlanningResidualKind kind) {
                return PlanningReference{.kind = PlanningReferenceKind::CoverageResidual, .residual = kind};
            };
            if (!quality.residual.boundaryShortfallRegion.empty()) {
                issue(PlanningIssueCode::CoverageBoundaryShortfall, PlanningIssueSeverity::Warning,
                      "A reliably evaluated boundary shortfall remains",
                      residualRef(PlanningResidualKind::BoundaryShortfall));
            }
            if (quality.criticalUncoveredAreaM2 > quality.numericalToleranceM2) {
                issue(PlanningIssueCode::CriticalCoverageGap, PlanningIssueSeverity::Blocking,
                      "A critical coverage gap remains", residualRef(PlanningResidualKind::CriticalUncovered));
            }
            if (quality.status == CoverageQualityStatus::Insufficient) {
                issue(PlanningIssueCode::CoverageBelowRequirement, PlanningIssueSeverity::Blocking,
                      "The selected coverage requirement was not met", residualRef(PlanningResidualKind::Uncovered));
                if (outcome.repair.attempted) {
                    issue(PlanningIssueCode::CoverageRepairInsufficient, PlanningIssueSeverity::Blocking,
                          "Supported coverage repair did not meet the selected requirement");
                }
            }
        }
    }
    if (outcome.repair.applied) {
        issue(PlanningIssueCode::CoverageRepairApplied, PlanningIssueSeverity::Info,
              "Safe target-relative repair improved the selected candidate");
    }
    if (outcome.tier == SafetySolutionTier::D0 || outcome.tier == SafetySolutionTier::D1) {
        issue(PlanningIssueCode::IngressNotAssessed, PlanningIssueSeverity::Info,
              "Launch, ingress and recovery routes are not assessed by this planning result");
    }
    if (outcome.diagnosticCandidate) {
        const auto& legs = outcome.diagnosticCandidate->legAssessments;
        issue(PlanningIssueCode::UnsafeDiagnosticCandidate, PlanningIssueSeverity::Blocking,
              "Raw navigation diagnostic route is not executable",
              PlanningReference{.kind = PlanningReferenceKind::DiagnosticCandidateLegRange, .legCount = legs.size()});
        if (std::ranges::find(legs, SafetyLegClass::HardUnsafe) != legs.end()) {
            issue(PlanningIssueCode::HardSafetyUnavailable, PlanningIssueSeverity::Blocking,
                  "The diagnostic route violates nominal hard clearance");
        }
        if (std::ranges::find(legs, SafetyLegClass::ExecutionUnsafe) != legs.end()) {
            issue(PlanningIssueCode::ExecutionReserveUnavailable, PlanningIssueSeverity::Blocking,
                  "The diagnostic route violates the hard execution reserve");
        }
    }
    for (std::size_t index = 0; index < outcome.diagnosticOverlays.size(); ++index) {
        const auto& overlay = outcome.diagnosticOverlays[index];
        const auto code = overlay.kind == DiagnosticOverlayKind::DisconnectedNavigation
                              ? PlanningIssueCode::NavigationRegionDisconnected
                          : overlay.kind == DiagnosticOverlayKind::UnsupportedGeometry
                              ? PlanningIssueCode::NavigationRegionUnsupported
                              : PlanningIssueCode::UnresolvedConnection;
        issue(code, PlanningIssueSeverity::Blocking, overlay.explanation.c_str(),
              PlanningReference{.kind = PlanningReferenceKind::DiagnosticOverlay, .index = index});
    }
    std::ranges::stable_sort(outcome.issues, {}, &PlanningIssue::code);
    const auto suggest = [&](PlanningIssueCode cause, PlanningSuggestionCode code, const char* message) {
        const auto found = std::ranges::find(outcome.issues, cause, &PlanningIssue::code);
        if (found != outcome.issues.end()) {
            outcome.suggestions.push_back({code, cause, message, found->reference});
        }
    };
    suggest(PlanningIssueCode::PreferredSafetyViolated, PlanningSuggestionCode::ExpandNavigationArea,
            "Review whether the navigation area can safely include more room");
    suggest(PlanningIssueCode::PreferredSafetyViolated, PlanningSuggestionCode::ReviewPreferredClearance,
            "Review the configured preferred clearance");
    suggest(PlanningIssueCode::CriticalCoverageGap, PlanningSuggestionCode::ReviewSwathWidth,
            "Review the explicitly configured swath width");
    suggest(PlanningIssueCode::CriticalCoverageGap, PlanningSuggestionCode::IncreaseNavigationRoom,
            "Review navigation room near the critical coverage gap");
    auto cause = std::ranges::find(outcome.issues, PlanningIssueCode::HardSafetyUnavailable, &PlanningIssue::code);
    if (cause == outcome.issues.end()) {
        cause = std::ranges::find(outcome.issues, PlanningIssueCode::ExecutionReserveUnavailable, &PlanningIssue::code);
    }
    if (cause == outcome.issues.end() && outcome.readiness == MissionReadiness::DiagnosticOnly &&
        !outcome.issues.empty()) {
        for (const auto code :
             {PlanningIssueCode::NavigationRegionDisconnected, PlanningIssueCode::NavigationRegionUnsupported,
              PlanningIssueCode::UnresolvedConnection}) {
            cause = std::ranges::find(outcome.issues, code, &PlanningIssue::code);
            if (cause != outcome.issues.end()) {
                break;
            }
        }
    }
    if (cause != outcome.issues.end()) {
        outcome.suggestions.push_back({PlanningSuggestionCode::ReviewHardNavigationFeasibility, cause->code,
                                       "Review hard navigation feasibility without relaxing H or E", cause->reference});
    }
    suggest(PlanningIssueCode::CoverageRepairApplied, PlanningSuggestionCode::InspectRepairCost,
            "Inspect the selected repair's added route length and turns");
}

bool publishDiagnosticOutcome(CoveragePlanningSolution& result, const SafetyTrackRegionsResult& safety,
                              const PolygonRegionSet2D& raw, const std::vector<Point2D>& path,
                              const std::vector<PathLegRole>& roles)
{
    const auto metrics = calculatePlanningPathMetrics(path, roles);
    if (!metrics) {
        return false;
    }
    DiagnosticCandidate<Point2D> diagnostic{.identity = "raw-navigation.0", .path = path, .legRoles = roles};
    bool unsafe = false;
    for (std::size_t index = 1; index < path.size(); ++index) {
        const auto inside = [&](const PolygonRegionSet2D& region) {
            return Geometry::segmentInsidePolygonRegionForValidatedGeometry(region, path[index - 1], path[index]);
        };
        if (!inside(raw)) {
            return false;
        }
        SafetyLegClass leg = SafetyLegClass::HardUnsafe;
        if (inside(safety.regions.preferredExecutionTrackRegion)) {
            leg = SafetyLegClass::PreferredSafe;
        } else if (inside(safety.regions.hardExecutionTrackRegion)) {
            leg = SafetyLegClass::HardSafeWarning;
        } else if (inside(safety.regions.nominalHardTrackRegion)) {
            leg = SafetyLegClass::ExecutionUnsafe;
        }
        unsafe |= leg == SafetyLegClass::HardUnsafe || leg == SafetyLegClass::ExecutionUnsafe;
        diagnostic.legAssessments.push_back(leg);
    }
    if (!unsafe) {
        return false;
    }
    diagnostic.coverageLengthM = metrics->coverageLengthM;
    diagnostic.transitLengthM = metrics->transitLengthM;
    diagnostic.pathLengthM = metrics->pathLengthM;
    diagnostic.turnCount = metrics->turnCount;
    result.status = PlanningStatus::Failed;
    result.outcome.readiness = MissionReadiness::DiagnosticOnly;
    result.outcome.tier = SafetySolutionTier::D2;
    result.outcome.diagnosticCandidate = std::move(diagnostic);
    populatePlanningAdvice(result);
    return true;
}

void publishUnresolvedOutcome(CoveragePlanningSolution& result, const PolygonRegionSet2D& target,
                              const PolygonRegionSet2D& raw, bool unsupported)
{
    result.status = PlanningStatus::Failed;
    result.outcome.readiness = MissionReadiness::DiagnosticOnly;
    result.outcome.tier = SafetySolutionTier::D3;
    result.outcome.diagnosticOverlays.push_back({unsupported      ? DiagnosticOverlayKind::UnsupportedGeometry
                                                 : raw.size() > 1 ? DiagnosticOverlayKind::DisconnectedNavigation
                                                                  : DiagnosticOverlayKind::UnresolvedConnection,
                                                 unsupported ? target : raw, result.message});
    populatePlanningAdvice(result);
}

}  // namespace Marine
