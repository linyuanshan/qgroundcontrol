#include "BoustrophedonCoveragePlanner.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

#include "BoustrophedonDecomposition.h"
#include "CellCoverage.h"
#include "ComplexCoverageAssembly.h"
#include "CoverageGeometry.h"
#include "CoverageProblemValidator.h"
#include "CoverageQualityEvaluator.h"
#include "CoverageRepair.h"
#include "CoverageSafety.h"
#include "CoverageStrategySemantics.h"
#include "Geometry/MarineGeometry.h"
#include "GlobalSweepSelector.h"
#include "GreedyCellOrdering.h"
#include "IntegratedPlanningResult.h"
#include "PlanningPathMetrics.h"

namespace {

using namespace Marine;

struct BcdCandidate
{
    std::vector<Point2D> path;
    std::vector<PathLegRole> legRoles;
    PlanningPathMetrics metrics;
    CoverageQualityEvaluation quality;
    bool preferredSafe = false;
    int cellCount = 0;
    std::size_t generationOrder = 0;
};

struct CandidateAttempt
{
    std::optional<BcdCandidate> candidate;
    CoveragePlanningError error = CoveragePlanningError::None;
    std::string message;
};

CoveragePlanningSolution failure(PlanningStatus status, CoveragePlanningError error, std::string message,
                                 std::optional<PlannerSourceInfo> source = std::nullopt)
{
    CoveragePlanningSolution solution;
    solution.status = status;
    solution.error = error;
    solution.message = message.empty() ? CoverageProblemValidator::messageForError(error) : std::move(message);
    solution.plannerSource = std::move(source);
    if (solution.plannerSource) {
        solution.selectedSweepAngleDeg = solution.plannerSource->selectedSweepAngleDeg;
    }
    return solution;
}

CoveragePlanningSolution failure(CoveragePlanningError error, std::string message = {},
                                 std::optional<PlannerSourceInfo> source = std::nullopt)
{
    return failure(CoverageProblemValidator::statusForError(error), error, std::move(message), std::move(source));
}

PlannerSourceInfo directSource(SweepAngleMode requestedMode, double selectedAngleDeg)
{
    return {.requestedPlannerId = CoverageStrategySemantics::BoustrophedonId,
            .resolvedStrategy = {.strategyId = CoverageStrategySemantics::BoustrophedonId,
                                 .semanticVersion = CoverageStrategySemantics::BoustrophedonVersion},
            .resolutionStatus = PlannerResolutionStatus::Resolved,
            .resolutionReason = PlannerResolutionReason::None,
            .escalated = false,
            .requestedSweepMode = requestedMode,
            .selectedSweepAngleDeg = selectedAngleDeg,
            .sweepSemanticVersion =
                requestedMode == SweepAngleMode::Auto ? CoverageStrategySemantics::GlobalSweepVersion : ""};
}

int qualityCategory(const CoverageQualityEvaluation& quality)
{
    if (((quality.status == CoverageQualityStatus::Complete) ||
         (quality.status == CoverageQualityStatus::Acceptable)) && quality.passesRequirement) {
        return 2;
    }
    if (quality.status == CoverageQualityStatus::Insufficient) {
        return 1;
    }
    return 0;
}

bool candidateBetter(const BcdCandidate& left, const BcdCandidate& right)
{
    const int leftCategory = qualityCategory(left.quality);
    const int rightCategory = qualityCategory(right.quality);
    if (leftCategory != rightCategory) {
        return leftCategory > rightCategory;
    }

    if ((leftCategory == 1) || (leftCategory == 2)) {
        switch (compareCoverageQuality(left.quality, right.quality)) {
        case CoverageQualityComparison::Better:
            return true;
        case CoverageQualityComparison::Worse:
            return false;
        case CoverageQualityComparison::Equivalent:
        case CoverageQualityComparison::NotComparable:
            break;
        }
    }

    if (left.preferredSafe != right.preferredSafe) {
        return left.preferredSafe;
    }
    if (left.metrics.pathLengthM != right.metrics.pathLengthM) {
        return left.metrics.pathLengthM < right.metrics.pathLengthM;
    }
    if (left.metrics.turnCount != right.metrics.turnCount) {
        return left.metrics.turnCount < right.metrics.turnCount;
    }
    return left.generationOrder < right.generationOrder;
}

CandidateAttempt generateCandidate(const CoverageDecompositionResult& decomposition,
                                   const PolygonRegionSet2D& coverageTarget,
                                   const PolygonRegionSet2D& activeTrackRegion,
                                   const SafetyTrackRegionsResult& safetyRegions,
                                   const CoveragePlanningProblem& problem, double selectedAngleDeg,
                                   const PlannerStrategyIdentity& strategy, std::size_t generationOrder,
                                   bool certify = true)
{
    const CellCoverageGenerationResult cellCoverage =
        generateCellCoverage(decomposition.cells, activeTrackRegion, problem.swathWidthM, selectedAngleDeg);
    if (cellCoverage.status != PlanningStatus::Success) {
        return {.error = cellCoverage.error, .message = cellCoverage.message};
    }

    const CellOrderingResult ordering =
        orderCellTraversals(activeTrackRegion, cellCoverage.cells, cellCoverage.traversalStates);
    if (ordering.status != PlanningStatus::Success) {
        return {.error = ordering.error, .message = ordering.message};
    }

    const ComplexCoverageAssemblyResult assembly =
        assembleComplexCoverage(activeTrackRegion, cellCoverage.cells, ordering.visits);
    if (assembly.status != PlanningStatus::Success) {
        return {.error = assembly.error, .message = assembly.message};
    }

    if ((assembly.path.size() < 2) || (assembly.legRoles.size() != assembly.path.size() - 1) ||
        (assembly.cellCount != static_cast<int>(decomposition.cells.size())) || (selectedAngleDeg < 0.0) ||
        (selectedAngleDeg >= 180.0)) {
        return {.error = CoveragePlanningError::InvalidGeneratedPath,
                .message = "BCD assembly returned inconsistent path structure"};
    }
    const std::optional<PlanningPathMetrics> metrics = calculatePlanningPathMetrics(assembly.path, assembly.legRoles);
    if (!metrics || !planningPathMetricsMatch(*metrics, assembly.coverageLengthM, assembly.transitLengthM,
                                               assembly.pathLengthM)) {
        return {.error = CoveragePlanningError::InvalidGeneratedPath,
                .message = "BCD assembly path metrics do not match its canonical geometry"};
    }

    const SafetyCandidateAssessment safety = evaluateSafetyCandidate(safetyRegions, assembly.path);
    if (certify && safety.error != CoveragePlanningError::None) {
        return {.error = safety.error, .message = CoverageProblemValidator::messageForError(safety.error)};
    }

    CoverageQualityEvaluation quality;
    if (certify) {
        quality = evaluateCoverageQuality(coverageTarget, assembly.path, assembly.legRoles, problem.swathWidthM,
                                          problem.coverageRequirement, strategy);
    }
    BcdCandidate candidate{.path = assembly.path,
                           .legRoles = assembly.legRoles,
                           .metrics = *metrics,
                           .quality = std::move(quality),
                           .preferredSafe = safety.tier.has_value() && (*safety.tier == SafetySolutionTier::D0),
                           .cellCount = assembly.cellCount,
                           .generationOrder = generationOrder};
    return {.candidate = std::move(candidate)};
}

}  // namespace

namespace Marine {

std::string BoustrophedonCoveragePlanner::id() const
{
    return CoverageStrategySemantics::BoustrophedonId;
}

std::string BoustrophedonCoveragePlanner::semanticVersion() const
{
    return CoverageStrategySemantics::BoustrophedonVersion;
}

std::string BoustrophedonCoveragePlanner::displayName() const
{
    return "Boustrophedon Coverage Planner";
}

CoveragePlanningSolution BoustrophedonCoveragePlanner::plan(const CoveragePlanningProblem& problem) const
{
    CoveragePlanningProblem normalized = problem;
    const CoveragePlanningError validationError = CoverageProblemValidator::validateAndNormalize(normalized);
    if (validationError != CoveragePlanningError::None) {
        return failure(validationError);
    }

    const CoverageGeometryResult coverageGeometry = buildCoverageGeometry(normalized.region);
    if (coverageGeometry.error != CoveragePlanningError::None) {
        auto result = failure(coverageGeometry.error);
        publishUnresolvedOutcome(result, {}, {}, false);
        return result;
    }
    const SafetyTrackRegionsResult safetyRegions =
        buildSafetyTrackRegions(normalized.region, normalized.safety, normalized.executionSafety);
    if (safetyRegions.error != CoveragePlanningError::None) {
        auto result = failure(safetyRegions.error);
        publishUnresolvedOutcome(result, coverageGeometry.geometry.coverageTarget,
                                 coverageGeometry.geometry.rawNavigationFreeSpace, false);
        return result;
    }

    double selectedSweepAngleDeg = normalized.requestedSweepAngleDeg;
    if (normalized.sweepAngleMode == SweepAngleMode::Auto) {
        const GlobalSweepSelectionResult selection = selectGlobalSweepAngle(
            normalized.region.coverageBoundary,
            (safetyRegions.regions.hardExecutionTrackRegion.empty() ? coverageGeometry.geometry.rawNavigationFreeSpace
                                                                    : safetyRegions.regions.hardExecutionTrackRegion),
            normalized.swathWidthM);
        if (selection.status != PlanningStatus::Success) {
            auto result = failure(selection.status, selection.error, selection.message);
            publishUnresolvedOutcome(result, coverageGeometry.geometry.coverageTarget,
                                     coverageGeometry.geometry.rawNavigationFreeSpace, false);
            return result;
        }
        selectedSweepAngleDeg = selection.selectedSweepAngleDeg;
    }
    const PlannerSourceInfo source = directSource(problem.sweepAngleMode, selectedSweepAngleDeg);

    const CoverageDecompositionResult decomposition =
        decomposeBoustrophedon(coverageGeometry.geometry.coverageTarget, selectedSweepAngleDeg);
    if (decomposition.status != PlanningStatus::Success) {
        CoveragePlanningSolution failed = failure(decomposition.status, decomposition.error, decomposition.message, source);
        failed.selectedSweepAngleDeg = selectedSweepAngleDeg;
        publishUnresolvedOutcome(failed, coverageGeometry.geometry.coverageTarget,
                                 coverageGeometry.geometry.rawNavigationFreeSpace, true);
        return failed;
    }

    const PlannerStrategyIdentity strategy{.strategyId = CoverageStrategySemantics::BoustrophedonId,
                                           .semanticVersion = CoverageStrategySemantics::BoustrophedonVersion};
    std::vector<BcdCandidate> candidates;
    CoveragePlanningError hardFailureError = CoveragePlanningError::NoNavigableArea;
    std::string hardFailureMessage = CoverageProblemValidator::messageForError(hardFailureError);

    const auto attemptTier = [&](const PolygonRegionSet2D& activeTrackRegion, std::size_t generationOrder,
                                 bool hardTier) {
        if (activeTrackRegion.empty()) {
            if (hardTier) {
                hardFailureError = CoveragePlanningError::NoNavigableArea;
                hardFailureMessage = CoverageProblemValidator::messageForError(hardFailureError);
            }
            return;
        }
        CandidateAttempt attempt = generateCandidate(decomposition, coverageGeometry.geometry.coverageTarget,
                                                     activeTrackRegion, safetyRegions, normalized,
                                                     selectedSweepAngleDeg, strategy, generationOrder);
        if (attempt.candidate) {
            candidates.push_back(std::move(*attempt.candidate));
        } else if (hardTier) {
            hardFailureError = attempt.error;
            hardFailureMessage = std::move(attempt.message);
        }
    };

    attemptTier(safetyRegions.regions.preferredExecutionTrackRegion, 0, false);
    attemptTier(safetyRegions.regions.hardExecutionTrackRegion, 1, true);
    if (candidates.empty()) {
        CoveragePlanningSolution failed = failure(hardFailureError, std::move(hardFailureMessage), source);
        failed.selectedSweepAngleDeg = selectedSweepAngleDeg;
        const auto& raw = coverageGeometry.geometry.rawNavigationFreeSpace;
        auto rawAttempt = generateCandidate(decomposition, coverageGeometry.geometry.coverageTarget, raw, safetyRegions,
                                            normalized, selectedSweepAngleDeg, strategy, 1, false);
        if (rawAttempt.candidate) {
            auto& candidate = *rawAttempt.candidate;
            const auto certification = evaluateSafetyCandidate(safetyRegions, candidate.path);
            if (certification.error == CoveragePlanningError::None) {
                candidate.preferredSafe = certification.tier == SafetySolutionTier::D0;
                candidate.quality = evaluateCoverageQuality(coverageGeometry.geometry.coverageTarget, candidate.path,
                                                            candidate.legRoles, normalized.swathWidthM,
                                                            normalized.coverageRequirement, strategy);
                candidates.push_back(std::move(candidate));
            } else if (publishDiagnosticOutcome(failed, safetyRegions, raw, candidate.path, candidate.legRoles)) {
                return failed;
            }
        }
        if (candidates.empty()) {
            if (!rawAttempt.message.empty()) {
                failed.message = rawAttempt.message;
            }
            publishUnresolvedOutcome(failed, coverageGeometry.geometry.coverageTarget, raw, false);
            return failed;
        }
    }

    const bool initialPass = std::ranges::any_of(
        candidates, [](const BcdCandidate& candidate) { return coveragePolicyPass(candidate.quality); });
    std::vector<CoverageRepairResult> repairCandidates;
    std::vector<PlanningPathMetrics> initialMetrics;
    for (BcdCandidate& candidate : candidates) {
        initialMetrics.push_back(candidate.metrics);
        CoverageRepairCandidate initial{candidate.path, candidate.legRoles, candidate.metrics, candidate.quality,
                                        candidate.preferredSafe};
        CoverageRepairResult repair{.candidate = initial};
        if (!initialPass) {
            const auto& active = candidate.generationOrder == 0 ? safetyRegions.regions.preferredExecutionTrackRegion
                                                                : safetyRegions.regions.hardExecutionTrackRegion;
            repair = repairCoverageCandidate(coverageGeometry.geometry.coverageTarget, active, safetyRegions,
                                             normalized.swathWidthM, normalized.coverageRequirement, strategy, initial);
            candidate.path = repair.candidate.path;
            candidate.legRoles = repair.candidate.legRoles;
            candidate.metrics = repair.candidate.metrics;
            candidate.quality = repair.candidate.quality;
            candidate.preferredSafe = repair.candidate.preferredSafe;
        }
        repairCandidates.push_back(std::move(repair));
    }

    const auto best = std::min_element(candidates.begin(), candidates.end(), candidateBetter);
    CoveragePlanningSolution solution;
    solution.repairCandidates = std::move(repairCandidates);
    solution.outcome.coverageQuality = best->quality;
    solution.plannerSource = source;
    solution.selectedSweepAngleDeg = selectedSweepAngleDeg;
    solution.cellCount = static_cast<int>(decomposition.cells.size());
    solution.turnCount = best->metrics.turnCount;

    solution.path = best->path;
    solution.legRoles = best->legRoles;
    solution.coverageLengthM = best->metrics.coverageLengthM;
    solution.transitLengthM = best->metrics.transitLengthM;
    solution.pathLengthM = best->metrics.pathLengthM;
    solution.message = best->quality.message;
    const auto selectedIndex = static_cast<std::size_t>(best - candidates.begin());
    publishCanonicalOutcome(solution, evaluateSafetyCandidate(safetyRegions, best->path), selectedIndex,
                            solution.repairCandidates[selectedIndex], initialMetrics[selectedIndex], initialPass);
    return solution;
}

}  // namespace Marine
