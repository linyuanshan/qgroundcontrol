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
#include "CoverageSafety.h"
#include "CoverageStrategySemantics.h"
#include "Geometry/MarineGeometry.h"
#include "GlobalSweepSelector.h"
#include "GreedyCellOrdering.h"
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
                                   const PlannerStrategyIdentity& strategy, std::size_t generationOrder)
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
    if (safety.error != CoveragePlanningError::None) {
        return {.error = safety.error, .message = CoverageProblemValidator::messageForError(safety.error)};
    }

    CoverageQualityEvaluation quality = evaluateCoverageQuality(
        coverageTarget, assembly.path, assembly.legRoles, problem.swathWidthM, problem.coverageRequirement, strategy);
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
        return failure(coverageGeometry.error);
    }
    const SafetyTrackRegionsResult safetyRegions =
        buildSafetyTrackRegions(normalized.region, normalized.safety, normalized.executionSafety);
    if (safetyRegions.error != CoveragePlanningError::None) {
        return failure(safetyRegions.error);
    }

    double selectedSweepAngleDeg = normalized.requestedSweepAngleDeg;
    if (normalized.sweepAngleMode == SweepAngleMode::Auto) {
        const GlobalSweepSelectionResult selection =
            selectGlobalSweepAngle(normalized.region.coverageBoundary,
                                   safetyRegions.regions.hardExecutionTrackRegion, normalized.swathWidthM);
        if (selection.status != PlanningStatus::Success) {
            return failure(selection.status, selection.error, selection.message);
        }
        selectedSweepAngleDeg = selection.selectedSweepAngleDeg;
    }
    const PlannerSourceInfo source = directSource(problem.sweepAngleMode, selectedSweepAngleDeg);

    const CoverageDecompositionResult decomposition =
        decomposeBoustrophedon(coverageGeometry.geometry.coverageTarget, selectedSweepAngleDeg);
    if (decomposition.status != PlanningStatus::Success) {
        CoveragePlanningSolution failed = failure(decomposition.status, decomposition.error, decomposition.message, source);
        failed.selectedSweepAngleDeg = selectedSweepAngleDeg;
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
        return failed;
    }

    const auto best = std::min_element(candidates.begin(), candidates.end(), candidateBetter);
    CoveragePlanningSolution solution;
    solution.coverageQuality = best->quality;
    solution.plannerSource = source;
    solution.selectedSweepAngleDeg = selectedSweepAngleDeg;
    solution.cellCount = static_cast<int>(decomposition.cells.size());
    solution.turnCount = best->metrics.turnCount;

    if (best->quality.status == CoverageQualityStatus::Insufficient) {
        solution.status = PlanningStatus::Failed;
        solution.error = CoveragePlanningError::CoverageIncomplete;
        solution.message = best->quality.message;
        return solution;
    }
    if (best->quality.status == CoverageQualityStatus::AssessmentError) {
        solution.status = PlanningStatus::Failed;
        solution.error = CoveragePlanningError::GeometryFailure;
        solution.message = best->quality.message;
        return solution;
    }
    if (!best->quality.passesRequirement) {
        solution.status = PlanningStatus::Failed;
        solution.error = CoveragePlanningError::GeometryFailure;
        solution.message = "BCD coverage evaluator returned an unclassified non-passing result";
        return solution;
    }

    solution.status = PlanningStatus::Success;
    solution.error = CoveragePlanningError::None;
    solution.path = best->path;
    solution.legRoles = best->legRoles;
    solution.coverageLengthM = best->metrics.coverageLengthM;
    solution.transitLengthM = best->metrics.transitLengthM;
    solution.pathLengthM = best->metrics.pathLengthM;
    solution.message = "BCD produced a hard-safe canonical path that passes the selected coverage policy";
    return solution;
}

}  // namespace Marine
