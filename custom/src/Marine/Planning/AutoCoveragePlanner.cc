#include "AutoCoveragePlanner.h"

#include <optional>
#include <utility>

#include "BoustrophedonCoveragePlanner.h"
#include "CoverageGeometry.h"
#include "CoverageProblemValidator.h"
#include "CoverageSafety.h"
#include "CoverageStrategySemantics.h"
#include "GlobalSweepSelector.h"
#include "SimpleMonotoneCapability.h"
#include "SimpleMonotoneCoveragePlanner.h"

namespace Marine {
namespace {

CoveragePlanningSolution failure(CoveragePlanningError error, std::string message = {},
                                 std::optional<PlannerSourceInfo> source = std::nullopt)
{
    CoveragePlanningSolution result;
    result.status = CoverageProblemValidator::statusForError(error);
    result.error = error;
    result.message = message.empty() ? CoverageProblemValidator::messageForError(error) : std::move(message);
    result.plannerSource = std::move(source);
    return result;
}

PlannerSourceInfo sourceInfo(PlannerResolutionStatus status, PlannerResolutionReason reason, bool escalated,
                             SweepAngleMode requestedMode, double selectedAngleDeg, std::string strategyId,
                             std::string strategyVersion)
{
    return {.requestedPlannerId = CoverageStrategySemantics::AutoPlannerId,
            .resolvedStrategy = {.strategyId = std::move(strategyId), .semanticVersion = std::move(strategyVersion)},
            .resolutionStatus = status,
            .resolutionReason = reason,
            .escalated = escalated,
            .requestedSweepMode = requestedMode,
            .selectedSweepAngleDeg = selectedAngleDeg,
            .sweepSemanticVersion =
                requestedMode == SweepAngleMode::Auto ? CoverageStrategySemantics::GlobalSweepVersion : ""};
}

}  // namespace

std::string AutoCoveragePlanner::id() const
{
    return CoverageStrategySemantics::AutoPlannerId;
}

std::string AutoCoveragePlanner::semanticVersion() const
{
    return CoverageStrategySemantics::AutoPlannerVersion;
}

std::string AutoCoveragePlanner::displayName() const
{
    return "Automatic Coverage Planner";
}

CoveragePlanningSolution AutoCoveragePlanner::plan(const CoveragePlanningProblem& problem) const
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

    double selectedAngleDeg = normalized.requestedSweepAngleDeg;
    if (normalized.sweepAngleMode == SweepAngleMode::Auto) {
        const GlobalSweepSelectionResult selection = selectGlobalSweepAngle(
            normalized.region.coverageBoundary, safetyRegions.regions.hardExecutionTrackRegion, normalized.swathWidthM);
        if (selection.status != PlanningStatus::Success) {
            return failure(selection.error, selection.message);
        }
        selectedAngleDeg = selection.selectedSweepAngleDeg;
    }

    const SimpleMonotoneCapabilityResult capability =
        assessSimpleMonotoneCapability(coverageGeometry.geometry.coverageTarget, selectedAngleDeg);
    if (!capability.applicable) {
        CoveragePlanningProblem delegatedProblem = normalized;
        delegatedProblem.sweepAngleMode = SweepAngleMode::Manual;
        delegatedProblem.requestedSweepAngleDeg = selectedAngleDeg;
        BoustrophedonCoveragePlanner bcdPlanner;
        CoveragePlanningSolution result = bcdPlanner.plan(delegatedProblem);
        result.plannerSource = sourceInfo(PlannerResolutionStatus::Resolved, capability.reason, true,
                                          problem.sweepAngleMode, selectedAngleDeg,
                                          CoverageStrategySemantics::BoustrophedonId,
                                          CoverageStrategySemantics::BoustrophedonVersion);
        return result;
    }

    CoveragePlanningProblem delegatedProblem = normalized;
    delegatedProblem.sweepAngleMode = SweepAngleMode::Manual;
    delegatedProblem.requestedSweepAngleDeg = selectedAngleDeg;
    SimpleMonotoneCoveragePlanner simplePlanner;
    CoveragePlanningSolution result = simplePlanner.plan(delegatedProblem);
    result.plannerSource =
        sourceInfo(PlannerResolutionStatus::Resolved, PlannerResolutionReason::None, false, problem.sweepAngleMode,
                   selectedAngleDeg, CoverageStrategySemantics::SimpleMonotoneId,
                   CoverageStrategySemantics::SimpleMonotoneVersion);
    return result;
}

}  // namespace Marine
