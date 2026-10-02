#include "SimpleMonotoneCoveragePlannerTest.h"

#include <algorithm>
#include <cmath>

#include "CoverageGeometry.h"
#include "CoverageSafety.h"
#include "Geometry/MarineGeometry.h"
#include "Planning/CoverageStrategySemantics.h"
#include "Planning/SimpleMonotoneCoveragePlanner.h"

using namespace Marine;

namespace {

Polygon2D rectangle(double left, double bottom, double right, double top)
{
    return {.vertices = {{left, bottom}, {right, bottom}, {right, top}, {left, top}}};
}

CoveragePlanningProblem simpleProblem(const Polygon2D& coverage, const Polygon2D& navigation, double swathWidthM = 5.0)
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

bool pathHasTransitOutsideTarget(const CoveragePlanningSolution& solution, const Polygon2D& target)
{
    const PolygonRegionSet2D targetRegion{{.outerBoundary = target}};
    for (std::size_t index = 0; index < solution.legRoles.size(); ++index) {
        if (solution.legRoles[index] != PathLegRole::Transit) {
            continue;
        }
        const Point2D midpoint{.xM = (solution.path[index].xM + solution.path[index + 1].xM) / 2.0,
                               .yM = (solution.path[index].yM + solution.path[index + 1].yM) / 2.0};
        if (!Geometry::pointInsidePolygonRegion(targetRegion, midpoint)) {
            return true;
        }
    }
    return false;
}

void verifyCoverageLegsStayInTarget(const CoveragePlanningSolution& solution, const PolygonRegionSet2D& target)
{
    QCOMPARE(solution.legRoles.size(), solution.path.size() - 1);
    for (std::size_t index = 0; index < solution.legRoles.size(); ++index) {
        if (solution.legRoles[index] == PathLegRole::Coverage) {
            QVERIFY(Geometry::segmentInsidePolygonRegion(target, solution.path[index], solution.path[index + 1]));
        }
    }
}

}  // namespace

void SimpleMonotoneCoveragePlannerTest::_testMetadataAndRectangle()
{
    const SimpleMonotoneCoveragePlanner planner;
    const CoveragePlanningSolution solution =
        planner.plan(simpleProblem(rectangle(0.0, 0.0, 20.0, 20.0), rectangle(-5.0, -5.0, 25.0, 25.0)));

    QCOMPARE(planner.id(), std::string(CoverageStrategySemantics::SimpleMonotoneId));
    QCOMPARE(planner.semanticVersion(), std::string(CoverageStrategySemantics::SimpleMonotoneVersion));
    QCOMPARE(solution.status, PlanningStatus::Success);
    QCOMPARE(solution.error, CoveragePlanningError::None);
    QVERIFY(solution.coverageQuality.has_value());
    QVERIFY(solution.coverageQuality->passesRequirement);
    QVERIFY(solution.plannerSource.has_value());
    QCOMPARE(solution.plannerSource->requestedPlannerId, std::string(CoverageStrategySemantics::SimpleMonotoneId));
    QCOMPARE(solution.plannerSource->resolvedStrategy.semanticVersion,
             std::string(CoverageStrategySemantics::SimpleMonotoneVersion));
    QCOMPARE(solution.plannerSource->resolutionStatus, PlannerResolutionStatus::Resolved);
    QCOMPARE(solution.plannerSource->resolutionReason, PlannerResolutionReason::None);
    QVERIFY(!solution.plannerSource->escalated);
    QCOMPARE(solution.plannerSource->requestedSweepMode, SweepAngleMode::Manual);
    QCOMPARE(solution.selectedSweepAngleDeg, 90.0);
}

void SimpleMonotoneCoveragePlannerTest::_testConcaveTargetAndCoverageRelativeLanes()
{
    const Polygon2D concave{
        .vertices = {{0.0, 0.0}, {20.0, 0.0}, {20.0, 10.0}, {10.0, 10.0}, {10.0, 20.0}, {0.0, 20.0}}};
    const CoveragePlanningProblem problem = simpleProblem(concave, rectangle(-10.0, -10.0, 30.0, 30.0), 4.0);
    const CoveragePlanningSolution solution = SimpleMonotoneCoveragePlanner{}.plan(problem);

    QCOMPARE(solution.status, PlanningStatus::Success);
    const CoverageGeometryResult geometry = buildCoverageGeometry(problem.region);
    QCOMPARE(geometry.error, CoveragePlanningError::None);
    verifyCoverageLegsStayInTarget(solution, geometry.geometry.coverageTarget);
    QVERIFY(std::ranges::any_of(solution.path, [](const Point2D& point) { return point.xM > 10.0; }));
}

void SimpleMonotoneCoveragePlannerTest::_testNavigationOutsideTargetUsedOnlyForTransit()
{
    CoveragePlanningProblem problem =
        simpleProblem(rectangle(0.0, 0.0, 1.0, 30.0), rectangle(-10.0, -5.0, 11.0, 35.0), 10.0);
    problem.region.noGoRegions.push_back(rectangle(-5.0, 9.0, -0.5, 11.0));
    problem.executionSafety.executionMarginM = 2.0;
    const CoveragePlanningSolution solution = SimpleMonotoneCoveragePlanner{}.plan(problem);

    QCOMPARE(solution.status, PlanningStatus::Success);
    const auto geometry = buildCoverageGeometry(problem.region);
    QCOMPARE(geometry.error, CoveragePlanningError::None);
    verifyCoverageLegsStayInTarget(solution, geometry.geometry.coverageTarget);
    QVERIFY(pathHasTransitOutsideTarget(solution, problem.region.coverageBoundary));

    const auto safety = buildSafetyTrackRegions(problem.region, problem.safety, problem.executionSafety);
    QCOMPARE(safety.error, CoveragePlanningError::None);
    for (std::size_t index = 1; index < solution.path.size(); ++index) {
        QVERIFY(Geometry::segmentInsidePolygonRegionForValidatedGeometry(
            safety.regions.hardExecutionTrackRegion, solution.path[index - 1], solution.path[index]));
    }
}

void SimpleMonotoneCoveragePlannerTest::_testExecutionMarginAndPreferredTier()
{
    CoveragePlanningProblem problem =
        simpleProblem(rectangle(0.0, 0.0, 20.0, 20.0), rectangle(-10.0, -10.0, 30.0, 30.0));
    problem.safety.preferredSafetyMarginM = 0.5;
    problem.executionSafety.executionMarginM = 0.2;
    const CoveragePlanningSolution solution = SimpleMonotoneCoveragePlanner{}.plan(problem);

    QCOMPARE(solution.status, PlanningStatus::Success);
    QVERIFY(solution.coverageQuality.has_value());
    QVERIFY(solution.coverageQuality->passesRequirement);
    QCOMPARE(solution.path.size() >= 2, true);
}

void SimpleMonotoneCoveragePlannerTest::_testHardPassOutranksPreferredInsufficient()
{
    CoveragePlanningProblem problem =
        simpleProblem(rectangle(0.0, 0.0, 20.0, 20.0), rectangle(0.0, 0.0, 20.0, 20.0), 4.0);
    problem.safety.preferredSafetyMarginM = 1.0;
    const CoveragePlanningSolution solution = SimpleMonotoneCoveragePlanner{}.plan(problem);

    QCOMPARE(solution.status, PlanningStatus::Success);
    QVERIFY(solution.coverageQuality.has_value());
    QVERIFY(solution.coverageQuality->passesRequirement);
    const auto safety = buildSafetyTrackRegions(problem.region, problem.safety, problem.executionSafety);
    QCOMPARE(safety.error, CoveragePlanningError::None);
    bool preferred = true;
    for (std::size_t index = 1; index < solution.path.size(); ++index) {
        QVERIFY(Geometry::segmentInsidePolygonRegionForValidatedGeometry(
            safety.regions.hardExecutionTrackRegion, solution.path[index - 1], solution.path[index]));
        preferred &= Geometry::segmentInsidePolygonRegionForValidatedGeometry(
            safety.regions.preferredExecutionTrackRegion, solution.path[index - 1], solution.path[index]);
    }
    QVERIFY(!preferred);
}

void SimpleMonotoneCoveragePlannerTest::_testIncompleteAndAssessmentErrorDoNotReturnPath()
{
    CoveragePlanningProblem incomplete =
        simpleProblem(rectangle(0.0, 0.0, 20.0, 20.0), rectangle(0.0, 0.0, 20.0, 20.0));
    incomplete.safety.hardSafetyMarginM = 2.0;
    incomplete.safety.preferredSafetyMarginM = 2.0;
    // Boundary support may now satisfy Standard; Strict retains the irreducible corner deficit.
    incomplete.coverageRequirement = CoverageRequirement::Strict;
    const CoveragePlanningSolution failedCoverage = SimpleMonotoneCoveragePlanner{}.plan(incomplete);
    QCOMPARE(failedCoverage.status, PlanningStatus::Failed);
    QCOMPARE(failedCoverage.error, CoveragePlanningError::CoverageIncomplete);
    QVERIFY(failedCoverage.path.empty());
    QCOMPARE(failedCoverage.coverageQuality->status, CoverageQualityStatus::Insufficient);

    CoveragePlanningProblem assessmentError =
        simpleProblem(rectangle(0.0, 0.0, 20.0, 20.0), rectangle(-5.0, -5.0, 25.0, 25.0), 1.0e150);
    const CoveragePlanningSolution failedAssessment = SimpleMonotoneCoveragePlanner{}.plan(assessmentError);
    QCOMPARE(failedAssessment.status, PlanningStatus::Failed);
    QCOMPARE(failedAssessment.error, CoveragePlanningError::GeometryFailure);
    QVERIFY(failedAssessment.path.empty());
    QVERIFY(failedAssessment.coverageQuality.has_value());
    QCOMPARE(failedAssessment.coverageQuality->status, CoverageQualityStatus::AssessmentError);
}

void SimpleMonotoneCoveragePlannerTest::_testDirectCapabilityFailureDoesNotEscalate()
{
    CoveragePlanningProblem problem = simpleProblem(rectangle(0.0, 0.0, 20.0, 20.0), rectangle(-5.0, -5.0, 25.0, 25.0));
    problem.region.noGoRegions.push_back(rectangle(8.0, 8.0, 12.0, 12.0));
    const CoveragePlanningSolution solution = SimpleMonotoneCoveragePlanner{}.plan(problem);

    QCOMPARE(solution.status, PlanningStatus::Failed);
    QCOMPARE(solution.error, CoveragePlanningError::UnsupportedStrategyCapability);
    QVERIFY(solution.path.empty());
    QVERIFY(solution.plannerSource.has_value());
    QCOMPARE(solution.plannerSource->resolutionReason, PlannerResolutionReason::None);
    QVERIFY(!solution.plannerSource->escalated);
}

void SimpleMonotoneCoveragePlannerTest::_testDeterministicPathAndSafety()
{
    const CoveragePlanningProblem problem =
        simpleProblem(rectangle(0.0, 0.0, 20.0, 20.0), rectangle(-5.0, -5.0, 25.0, 25.0));
    const CoveragePlanningSolution first = SimpleMonotoneCoveragePlanner{}.plan(problem);
    const CoveragePlanningSolution second = SimpleMonotoneCoveragePlanner{}.plan(problem);

    QCOMPARE(first.status, PlanningStatus::Success);
    QCOMPARE(first.path.size(), second.path.size());
    QCOMPARE(first.legRoles, second.legRoles);
    QCOMPARE(first.pathLengthM, second.pathLengthM);
    for (std::size_t index = 0; index < first.path.size(); ++index) {
        QCOMPARE(first.path[index].xM, second.path[index].xM);
        QCOMPARE(first.path[index].yM, second.path[index].yM);
    }
    QCOMPARE(first.plannerSource->requestedPlannerId, second.plannerSource->requestedPlannerId);
    QCOMPARE(first.plannerSource->resolvedStrategy, second.plannerSource->resolvedStrategy);
    QCOMPARE(first.plannerSource->selectedSweepAngleDeg, second.plannerSource->selectedSweepAngleDeg);
    const auto safety = buildSafetyTrackRegions(problem.region, problem.safety, problem.executionSafety);
    for (std::size_t index = 1; index < first.path.size(); ++index) {
        QVERIFY(Geometry::segmentInsidePolygonRegionForValidatedGeometry(safety.regions.hardExecutionTrackRegion,
                                                                         first.path[index - 1], first.path[index]));
    }
}

UT_REGISTER_TEST_LIGHTWEIGHT(SimpleMonotoneCoveragePlannerTest, TestLabel::Unit)
