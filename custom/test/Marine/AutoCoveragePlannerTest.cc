#include "AutoCoveragePlannerTest.h"

#include "CoverageProblemValidator.h"
#include "Geometry/MarineGeometry.h"
#include "Planning/AutoCoveragePlanner.h"
#include "Planning/BoustrophedonCoveragePlanner.h"
#include "Planning/CoverageGeometry.h"
#include "Planning/CoverageSafety.h"
#include "Planning/CoverageStrategySemantics.h"
#include "Planning/GlobalSweepSelector.h"
#include "Planning/SimpleMonotoneCapability.h"

using namespace Marine;

namespace {

Polygon2D rectangle(double left, double bottom, double right, double top)
{
    return {.vertices = {{left, bottom}, {right, bottom}, {right, top}, {left, top}}};
}

CoveragePlanningProblem problem(const Polygon2D& coverage, const Polygon2D& navigation, double swathWidthM = 5.0)
{
    CoveragePlanningProblem result;
    result.region.coverageBoundary = coverage;
    result.region.navigationBoundary = navigation;
    result.swathWidthM = swathWidthM;
    result.safety.hardSafetyMarginM = 0.0;
    result.safety.preferredSafetyMarginM = 0.0;
    result.executionSafety.executionMarginM = 0.0;
    result.sweepAngleMode = SweepAngleMode::Manual;
    result.requestedSweepAngleDeg = 90.0;
    return result;
}

void verifySimpleSource(const CoveragePlanningSolution& solution, SweepAngleMode requestedSweepMode)
{
    QVERIFY(solution.plannerSource.has_value());
    QCOMPARE(solution.plannerSource->requestedPlannerId, std::string(CoverageStrategySemantics::AutoPlannerId));
    QCOMPARE(solution.plannerSource->resolvedStrategy.strategyId,
             std::string(CoverageStrategySemantics::SimpleMonotoneId));
    QCOMPARE(solution.plannerSource->resolvedStrategy.semanticVersion,
             std::string(CoverageStrategySemantics::SimpleMonotoneVersion));
    QCOMPARE(solution.plannerSource->resolutionStatus, PlannerResolutionStatus::Resolved);
    QCOMPARE(solution.plannerSource->resolutionReason, PlannerResolutionReason::None);
    QVERIFY(!solution.plannerSource->escalated);
    QCOMPARE(solution.plannerSource->requestedSweepMode, requestedSweepMode);
}

void verifyBcdSource(const CoveragePlanningSolution& solution, PlannerResolutionReason reason,
                     SweepAngleMode requestedSweepMode = SweepAngleMode::Manual)
{
    QCOMPARE(solution.status, PlanningStatus::Success);
    QVERIFY(!solution.path.empty());
    QVERIFY(!solution.legRoles.empty());
    QVERIFY(solution.outcome.coverageQuality.has_value());
    QVERIFY(solution.outcome.coverageQuality->passesRequirement);
    QCOMPARE(solution.outcome.coverageQuality->strategy.strategyId,
             std::string(CoverageStrategySemantics::BoustrophedonId));
    QCOMPARE(solution.outcome.coverageQuality->strategy.semanticVersion,
             std::string(CoverageStrategySemantics::BoustrophedonVersion));
    QVERIFY(solution.plannerSource.has_value());
    QCOMPARE(solution.plannerSource->requestedPlannerId, std::string(CoverageStrategySemantics::AutoPlannerId));
    QCOMPARE(solution.plannerSource->resolvedStrategy.strategyId,
             std::string(CoverageStrategySemantics::BoustrophedonId));
    QCOMPARE(solution.plannerSource->resolvedStrategy.semanticVersion,
             std::string(CoverageStrategySemantics::BoustrophedonVersion));
    QCOMPARE(solution.plannerSource->resolutionStatus, PlannerResolutionStatus::Resolved);
    QCOMPARE(solution.plannerSource->resolutionReason, reason);
    QVERIFY(solution.plannerSource->escalated);
    QCOMPARE(solution.plannerSource->requestedSweepMode, requestedSweepMode);
    if (requestedSweepMode == SweepAngleMode::Manual) {
        QVERIFY(solution.plannerSource->sweepSemanticVersion.empty());
    } else {
        QCOMPARE(solution.plannerSource->sweepSemanticVersion,
                 std::string(CoverageStrategySemantics::GlobalSweepVersion));
    }
}

}  // namespace

void AutoCoveragePlannerTest::_testManualRectangleResolvesSimpleMonotone()
{
    const AutoCoveragePlanner planner;
    const CoveragePlanningSolution solution =
        planner.plan(problem(rectangle(0.0, 0.0, 20.0, 20.0), rectangle(-5.0, -5.0, 25.0, 25.0)));

    QCOMPARE(planner.id(), std::string(CoverageStrategySemantics::AutoPlannerId));
    QCOMPARE(planner.semanticVersion(), std::string(CoverageStrategySemantics::AutoPlannerVersion));
    QCOMPARE(solution.status, PlanningStatus::Success);
    verifySimpleSource(solution, SweepAngleMode::Manual);
    QCOMPARE(solution.selectedSweepAngleDeg, 90.0);
    QVERIFY(solution.plannerSource->sweepSemanticVersion.empty());
}

void AutoCoveragePlannerTest::_testConcaveMonotoneResolvesSimpleMonotone()
{
    const Polygon2D lShape{
        .vertices = {{0.0, 0.0}, {20.0, 0.0}, {20.0, 10.0}, {10.0, 10.0}, {10.0, 20.0}, {0.0, 20.0}}};
    const CoveragePlanningSolution solution =
        AutoCoveragePlanner{}.plan(problem(lShape, rectangle(-10.0, -10.0, 30.0, 30.0), 4.0));
    QCOMPARE(solution.status, PlanningStatus::Success);
    verifySimpleSource(solution, SweepAngleMode::Manual);
}

void AutoCoveragePlannerTest::_testCapabilityEscalationDelegatesToBcd()
{
    const Polygon2D uShape{
        .vertices = {
            {0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {7.0, 10.0}, {7.0, 3.0}, {3.0, 3.0}, {3.0, 10.0}, {0.0, 10.0}}};
    verifyBcdSource(AutoCoveragePlanner{}.plan(problem(uShape, rectangle(-5.0, -5.0, 15.0, 15.0))),
                    PlannerResolutionReason::TargetNonMonotoneForSelectedSweep);

    CoveragePlanningProblem hole = problem(rectangle(0.0, 0.0, 20.0, 20.0), rectangle(-5.0, -5.0, 25.0, 25.0));
    hole.region.noGoRegions.push_back(rectangle(8.0, 8.0, 12.0, 12.0));
    verifyBcdSource(AutoCoveragePlanner{}.plan(hole), PlannerResolutionReason::TargetHasHoles);

    CoveragePlanningProblem split = problem(rectangle(0.0, 0.0, 20.0, 20.0), rectangle(-5.0, -5.0, 25.0, 25.0), 4.0);
    split.region.noGoRegions.push_back(rectangle(9.0, -2.0, 11.0, 22.0));
    const CoveragePlanningSolution splitResult = AutoCoveragePlanner{}.plan(split);
    verifyBcdSource(splitResult, PlannerResolutionReason::TargetHasMultipleComponents);
    QVERIFY(splitResult.cellCount >= 2);
    const CoverageGeometryResult geometry = buildCoverageGeometry(split.region);
    QCOMPARE(geometry.error, CoveragePlanningError::None);
    bool transitOutsideTarget = false;
    for (std::size_t leg = 0; leg < splitResult.legRoles.size(); ++leg) {
        if (splitResult.legRoles[leg] == PathLegRole::Transit) {
            transitOutsideTarget |= !Geometry::segmentInsidePolygonRegion(
                geometry.geometry.coverageTarget, splitResult.path[leg], splitResult.path[leg + 1]);
        }
    }
    QVERIFY(transitOutsideTarget);
}

void AutoCoveragePlannerTest::_testNoGoOutsideCoverageDoesNotEscalate()
{
    CoveragePlanningProblem input = problem(rectangle(0.0, 0.0, 20.0, 20.0), rectangle(-5.0, -5.0, 25.0, 25.0));
    input.region.noGoRegions.push_back(rectangle(22.0, 8.0, 24.0, 12.0));
    const CoveragePlanningSolution solution = AutoCoveragePlanner{}.plan(input);
    QCOMPARE(solution.status, PlanningStatus::Success);
    verifySimpleSource(solution, SweepAngleMode::Manual);
}

void AutoCoveragePlannerTest::_testCoverageAssessmentAndSafetyFailureDoNotEscalate()
{
    CoveragePlanningProblem incomplete = problem(rectangle(0.0, 0.0, 20.0, 20.0), rectangle(0.0, 0.0, 20.0, 20.0));
    incomplete.safety.hardSafetyMarginM = 2.0;
    incomplete.safety.preferredSafetyMarginM = 2.0;
    // Test post-repair failure, rather than freezing the pre-repair Standard deficit.
    incomplete.coverageRequirement = CoverageRequirement::Strict;
    const CoveragePlanningSolution coverageFailure = AutoCoveragePlanner{}.plan(incomplete);
    QCOMPARE(coverageFailure.status, PlanningStatus::Success);
    QCOMPARE(coverageFailure.outcome.readiness, MissionReadiness::ReviewRequired);
    QCOMPARE(coverageFailure.error, CoveragePlanningError::None);
    QCOMPARE(coverageFailure.outcome.coverageQuality->status, CoverageQualityStatus::Insufficient);
    verifySimpleSource(coverageFailure, SweepAngleMode::Manual);

    CoveragePlanningProblem assessmentError =
        problem(rectangle(0.0, 0.0, 20.0, 20.0), rectangle(-5.0, -5.0, 25.0, 25.0), 1.0e150);
    const CoveragePlanningSolution assessmentFailure = AutoCoveragePlanner{}.plan(assessmentError);
    QCOMPARE(assessmentFailure.status, PlanningStatus::Success);
    QCOMPARE(assessmentFailure.outcome.readiness, MissionReadiness::ReviewRequired);
    QCOMPARE(assessmentFailure.error, CoveragePlanningError::None);
    QCOMPARE(assessmentFailure.outcome.coverageQuality->status, CoverageQualityStatus::AssessmentError);
    verifySimpleSource(assessmentFailure, SweepAngleMode::Manual);

    CoveragePlanningProblem noTrack = problem(rectangle(0.0, 0.0, 20.0, 20.0), rectangle(0.0, 0.0, 20.0, 20.0));
    noTrack.executionSafety.executionMarginM = 20.0;
    const CoveragePlanningSolution safetyFailure = AutoCoveragePlanner{}.plan(noTrack);
    QCOMPARE(safetyFailure.status, PlanningStatus::Failed);
    verifySimpleSource(safetyFailure, SweepAngleMode::Manual);
}

void AutoCoveragePlannerTest::_testBcdFailurePreservesResolvedStrategyProvenance()
{
    CoveragePlanningProblem input =
        problem(rectangle(0.0, 0.0, 20.0, 20.0), rectangle(-5.0, -5.0, 25.0, 25.0));
    input.region.noGoRegions.push_back(rectangle(8.0, 8.0, 12.0, 12.0));
    input.executionSafety.executionMarginM = 100.0;

    const CoverageGeometryResult geometry = buildCoverageGeometry(input.region);
    QCOMPARE(geometry.error, CoveragePlanningError::None);
    const SimpleMonotoneCapabilityResult capability =
        assessSimpleMonotoneCapability(geometry.geometry.coverageTarget, input.requestedSweepAngleDeg);
    QVERIFY(!capability.applicable);
    QCOMPARE(capability.reason, PlannerResolutionReason::TargetHasHoles);

    const SafetyTrackRegionsResult safety =
        buildSafetyTrackRegions(input.region, input.safety, input.executionSafety);
    QCOMPARE(safety.error, CoveragePlanningError::None);
    QVERIFY(safety.regions.hardExecutionTrackRegion.empty());

    const CoveragePlanningSolution solution = AutoCoveragePlanner{}.plan(input);
    QCOMPARE(solution.status, PlanningStatus::Failed);
    QCOMPARE(solution.error, CoveragePlanningError::NoNavigableArea);
    QVERIFY(solution.path.empty());
    QVERIFY(solution.legRoles.empty());
    QVERIFY(solution.plannerSource.has_value());
    QCOMPARE(solution.plannerSource->requestedPlannerId,
             std::string(CoverageStrategySemantics::AutoPlannerId));
    QCOMPARE(solution.plannerSource->resolvedStrategy.strategyId,
             std::string(CoverageStrategySemantics::BoustrophedonId));
    QCOMPARE(solution.plannerSource->resolvedStrategy.semanticVersion,
             std::string(CoverageStrategySemantics::BoustrophedonVersion));
    QCOMPARE(solution.plannerSource->resolutionStatus, PlannerResolutionStatus::Resolved);
    QCOMPARE(solution.plannerSource->resolutionReason, capability.reason);
    QVERIFY(solution.plannerSource->escalated);
}

void AutoCoveragePlannerTest::_testAutoSweepIsDeterministicAndDistinctFromPlannerAuto()
{
    CoveragePlanningProblem automaticSweep =
        problem(rectangle(0.0, 0.0, 20.0, 10.0), rectangle(-5.0, -5.0, 25.0, 15.0));
    automaticSweep.sweepAngleMode = SweepAngleMode::Auto;
    const CoveragePlanningSolution first = AutoCoveragePlanner{}.plan(automaticSweep);
    const CoveragePlanningSolution second = AutoCoveragePlanner{}.plan(automaticSweep);
    QCOMPARE(first.status, PlanningStatus::Success);
    verifySimpleSource(first, SweepAngleMode::Auto);
    QCOMPARE(first.plannerSource->sweepSemanticVersion, std::string(CoverageStrategySemantics::GlobalSweepVersion));
    QCOMPARE(first.selectedSweepAngleDeg, second.selectedSweepAngleDeg);
    QCOMPARE(first.path.size(), second.path.size());
    for (std::size_t index = 0; index < first.path.size(); ++index) {
        QCOMPARE(first.path[index].xM, second.path[index].xM);
        QCOMPARE(first.path[index].yM, second.path[index].yM);
    }
    QVERIFY(first.selectedSweepAngleDeg >= 0.0 && first.selectedSweepAngleDeg < 180.0);

    CoveragePlanningProblem manualSweep = automaticSweep;
    manualSweep.sweepAngleMode = SweepAngleMode::Manual;
    manualSweep.requestedSweepAngleDeg = 37.0;
    const CoveragePlanningSolution manual = AutoCoveragePlanner{}.plan(manualSweep);
    QCOMPARE(manual.status, PlanningStatus::Success);
    verifySimpleSource(manual, SweepAngleMode::Manual);
    QCOMPARE(manual.selectedSweepAngleDeg, 37.0);
    QVERIFY(manual.plannerSource->sweepSemanticVersion.empty());
}

void AutoCoveragePlannerTest::_testAutoSweepEscalationSelectsOnce()
{
    CoveragePlanningProblem input =
        problem(rectangle(0.0, 0.0, 20.0, 20.0), rectangle(-5.0, -5.0, 25.0, 25.0), 4.0);
    input.region.noGoRegions.push_back(rectangle(8.0, 8.0, 12.0, 12.0));
    input.sweepAngleMode = SweepAngleMode::Auto;
    const CoverageGeometryResult geometry = buildCoverageGeometry(input.region);
    QCOMPARE(geometry.error, CoveragePlanningError::None);
    const SafetyTrackRegionsResult safety =
        buildSafetyTrackRegions(input.region, input.safety, input.executionSafety);
    QCOMPARE(safety.error, CoveragePlanningError::None);
    const GlobalSweepSelectionResult expected = selectGlobalSweepAngle(
        input.region.coverageBoundary, safety.regions.hardExecutionTrackRegion, input.swathWidthM);
    QCOMPARE(expected.status, PlanningStatus::Success);

    const CoveragePlanningSolution first = AutoCoveragePlanner{}.plan(input);
    const CoveragePlanningSolution second = AutoCoveragePlanner{}.plan(input);
    verifyBcdSource(first, PlannerResolutionReason::TargetHasHoles, SweepAngleMode::Auto);
    QCOMPARE(first.selectedSweepAngleDeg, expected.selectedSweepAngleDeg);
    QCOMPARE(first.plannerSource->selectedSweepAngleDeg, expected.selectedSweepAngleDeg);
    QCOMPARE(first.selectedSweepAngleDeg, second.selectedSweepAngleDeg);
    QCOMPARE(first.path.size(), second.path.size());
    for (std::size_t index = 0; index < first.path.size(); ++index) {
        QCOMPARE(first.path[index].xM, second.path[index].xM);
        QCOMPARE(first.path[index].yM, second.path[index].yM);
    }
}

UT_REGISTER_TEST_LIGHTWEIGHT(AutoCoveragePlannerTest, TestLabel::Unit)
