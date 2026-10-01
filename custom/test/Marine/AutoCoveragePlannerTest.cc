#include "AutoCoveragePlannerTest.h"

#include "CoverageProblemValidator.h"
#include "Geometry/MarineGeometry.h"
#include "Planning/AutoCoveragePlanner.h"
#include "Planning/CoverageStrategySemantics.h"

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

void verifyPendingBcd(const CoveragePlanningSolution& solution, PlannerResolutionReason reason)
{
    QCOMPARE(solution.status, PlanningStatus::Failed);
    QCOMPARE(solution.error, CoveragePlanningError::ResolvedStrategyUnavailable);
    QVERIFY(solution.path.empty());
    QVERIFY(solution.legRoles.empty());
    QVERIFY(solution.message.find("Auto resolved this task to the v0.5 BCD strategy, which is implemented in V05-06") !=
            std::string::npos);
    QVERIFY(solution.plannerSource.has_value());
    QCOMPARE(solution.plannerSource->requestedPlannerId, std::string(CoverageStrategySemantics::AutoPlannerId));
    QCOMPARE(solution.plannerSource->resolvedStrategy.strategyId,
             std::string(CoverageStrategySemantics::BoustrophedonId));
    QCOMPARE(solution.plannerSource->resolvedStrategy.semanticVersion,
             std::string(CoverageStrategySemantics::BoustrophedonPendingVersion));
    QCOMPARE(solution.plannerSource->resolutionStatus, PlannerResolutionStatus::ResolvedStrategyUnavailable);
    QCOMPARE(solution.plannerSource->resolutionReason, reason);
    QVERIFY(solution.plannerSource->escalated);
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

void AutoCoveragePlannerTest::_testCapabilityEscalationIsPendingBcd()
{
    const Polygon2D uShape{
        .vertices = {
            {0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {7.0, 10.0}, {7.0, 3.0}, {3.0, 3.0}, {3.0, 10.0}, {0.0, 10.0}}};
    verifyPendingBcd(AutoCoveragePlanner{}.plan(problem(uShape, rectangle(-5.0, -5.0, 15.0, 15.0))),
                     PlannerResolutionReason::TargetNonMonotoneForSelectedSweep);

    CoveragePlanningProblem hole = problem(rectangle(0.0, 0.0, 20.0, 20.0), rectangle(-5.0, -5.0, 25.0, 25.0));
    hole.region.noGoRegions.push_back(rectangle(8.0, 8.0, 12.0, 12.0));
    verifyPendingBcd(AutoCoveragePlanner{}.plan(hole), PlannerResolutionReason::TargetHasHoles);

    CoveragePlanningProblem split = problem(rectangle(0.0, 0.0, 20.0, 20.0), rectangle(-5.0, -5.0, 25.0, 25.0));
    split.region.noGoRegions.push_back(rectangle(9.0, -1.0, 11.0, 21.0));
    verifyPendingBcd(AutoCoveragePlanner{}.plan(split), PlannerResolutionReason::TargetHasMultipleComponents);
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
    incomplete.safety.hardSafetyMarginM = 1.0;
    incomplete.safety.preferredSafetyMarginM = 1.0;
    const CoveragePlanningSolution coverageFailure = AutoCoveragePlanner{}.plan(incomplete);
    QCOMPARE(coverageFailure.status, PlanningStatus::Failed);
    QCOMPARE(coverageFailure.error, CoveragePlanningError::CoverageIncomplete);
    QCOMPARE(coverageFailure.coverageQuality->status, CoverageQualityStatus::Insufficient);
    verifySimpleSource(coverageFailure, SweepAngleMode::Manual);

    CoveragePlanningProblem assessmentError =
        problem(rectangle(0.0, 0.0, 20.0, 20.0), rectangle(-5.0, -5.0, 25.0, 25.0), 1.0e150);
    const CoveragePlanningSolution assessmentFailure = AutoCoveragePlanner{}.plan(assessmentError);
    QCOMPARE(assessmentFailure.status, PlanningStatus::Failed);
    QCOMPARE(assessmentFailure.error, CoveragePlanningError::GeometryFailure);
    QCOMPARE(assessmentFailure.coverageQuality->status, CoverageQualityStatus::AssessmentError);
    verifySimpleSource(assessmentFailure, SweepAngleMode::Manual);

    CoveragePlanningProblem noTrack = problem(rectangle(0.0, 0.0, 20.0, 20.0), rectangle(0.0, 0.0, 20.0, 20.0));
    noTrack.executionSafety.executionMarginM = 20.0;
    const CoveragePlanningSolution safetyFailure = AutoCoveragePlanner{}.plan(noTrack);
    QCOMPARE(safetyFailure.status, PlanningStatus::Failed);
    verifySimpleSource(safetyFailure, SweepAngleMode::Manual);
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

UT_REGISTER_TEST_LIGHTWEIGHT(AutoCoveragePlannerTest, TestLabel::Unit)
