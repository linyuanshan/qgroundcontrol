#include "LawnmowerCoveragePlannerTest.h"

#include <cmath>

#include "Geometry/MarineGeometry.h"
#include "Planning/LawnmowerCoveragePlanner.h"

using namespace Marine;

namespace {

CoveragePlanningProblem rectangleProblem(double widthM, double heightM, double swathWidthM, double angleDeg,
                                         double safetyMarginM = 0.0)
{
    CoveragePlanningProblem problem;
    problem.region.outerBoundary.vertices = {
        {0.0, 0.0},
        {widthM, 0.0},
        {widthM, heightM},
        {0.0, heightM},
    };
    problem.swathWidthM = swathWidthM;
    problem.safetyMarginM = safetyMarginM;
    problem.sweepAngleMode = SweepAngleMode::Manual;
    problem.requestedSweepAngleDeg = angleDeg;
    return problem;
}

void compareWithinTolerance(double actual, double expected)
{
    QVERIFY(std::abs(actual - expected) <= Geometry::LengthEpsilonM);
}

void comparePoint(const Point2D& actual, double expectedX, double expectedY)
{
    compareWithinTolerance(actual.xM, expectedX);
    compareWithinTolerance(actual.yM, expectedY);
}

}  // namespace

void LawnmowerCoveragePlannerTest::_testMetadata()
{
    const LawnmowerCoveragePlanner planner;

    QCOMPARE(planner.id(), std::string("marine.coverage.lawnmower"));
    QVERIFY(!planner.displayName().empty());
}

void LawnmowerCoveragePlannerTest::_testRectangleManualZeroDegrees()
{
    const LawnmowerCoveragePlanner planner;
    const CoveragePlanningSolution solution = planner.plan(rectangleProblem(20.0, 10.0, 4.0, 0.0));

    QCOMPARE(solution.status, PlanningStatus::Success);
    QCOMPARE(solution.error, CoveragePlanningError::None);
    QCOMPARE(solution.path.size(), std::size_t{6});
    comparePoint(solution.path[0], 0.0, 5.0 / 3.0);
    comparePoint(solution.path[1], 20.0, 5.0 / 3.0);
    comparePoint(solution.path[2], 20.0, 5.0);
    comparePoint(solution.path[3], 0.0, 5.0);
    comparePoint(solution.path[4], 0.0, 25.0 / 3.0);
    comparePoint(solution.path[5], 20.0, 25.0 / 3.0);
    compareWithinTolerance(solution.pathLengthM, 200.0 / 3.0);
    QCOMPARE(solution.selectedSweepAngleDeg, 0.0);
    QCOMPARE(solution.turnCount, 2);
}

void LawnmowerCoveragePlannerTest::_testRectangleManualNinetyDegrees()
{
    const LawnmowerCoveragePlanner planner;
    const CoveragePlanningSolution solution = planner.plan(rectangleProblem(20.0, 10.0, 5.0, 90.0));

    QCOMPARE(solution.status, PlanningStatus::Success);
    QCOMPARE(solution.path.size(), std::size_t{8});
    comparePoint(solution.path[0], 17.5, 0.0);
    comparePoint(solution.path[1], 17.5, 10.0);
    comparePoint(solution.path[2], 12.5, 10.0);
    comparePoint(solution.path[3], 12.5, 0.0);
    comparePoint(solution.path[6], 2.5, 10.0);
    comparePoint(solution.path[7], 2.5, 0.0);
    compareWithinTolerance(solution.pathLengthM, 55.0);
    QCOMPARE(solution.selectedSweepAngleDeg, 90.0);
    QCOMPARE(solution.turnCount, 3);
}

void LawnmowerCoveragePlannerTest::_testNarrowRegionUsesOneLane()
{
    const LawnmowerCoveragePlanner planner;
    const CoveragePlanningSolution solution = planner.plan(rectangleProblem(12.0, 3.0, 5.0, 0.0));

    QCOMPARE(solution.status, PlanningStatus::Success);
    QCOMPARE(solution.path.size(), std::size_t{2});
    comparePoint(solution.path[0], 0.0, 1.5);
    comparePoint(solution.path[1], 12.0, 1.5);
    compareWithinTolerance(solution.pathLengthM, 12.0);
    QCOMPARE(solution.turnCount, 0);
}

void LawnmowerCoveragePlannerTest::_testPositiveSafetyMargin()
{
    const LawnmowerCoveragePlanner planner;
    const CoveragePlanningSolution solution = planner.plan(rectangleProblem(20.0, 10.0, 4.0, 0.0, 1.0));

    QCOMPARE(solution.status, PlanningStatus::Success);
    QCOMPARE(solution.path.size(), std::size_t{4});
    comparePoint(solution.path[0], 1.0, 3.0);
    comparePoint(solution.path[1], 19.0, 3.0);
    comparePoint(solution.path[2], 19.0, 7.0);
    comparePoint(solution.path[3], 1.0, 7.0);
    compareWithinTolerance(solution.pathLengthM, 40.0);
    QCOMPARE(solution.turnCount, 1);
}

void LawnmowerCoveragePlannerTest::_testGeneralConvexDeterminism()
{
    CoveragePlanningProblem problem;
    problem.region.outerBoundary.vertices = {
        {-2.0, 4.0}, {0.0, 0.0}, {12.0, -1.0}, {18.0, 5.0}, {13.0, 10.0}, {2.0, 9.0},
    };
    problem.swathWidthM = 3.0;
    problem.sweepAngleMode = SweepAngleMode::Manual;
    problem.requestedSweepAngleDeg = 27.0;

    const LawnmowerCoveragePlanner planner;
    const CoveragePlanningSolution first = planner.plan(problem);
    const CoveragePlanningSolution second = planner.plan(problem);

    QCOMPARE(first.status, PlanningStatus::Success);
    QCOMPARE(first.error, CoveragePlanningError::None);
    QVERIFY(first.path.size() >= 2);
    QCOMPARE(first.path.size(), second.path.size());
    for (std::size_t index = 0; index < first.path.size(); ++index) {
        QVERIFY(first.path[index].isFinite());
        QCOMPARE(first.path[index].xM, second.path[index].xM);
        QCOMPARE(first.path[index].yM, second.path[index].yM);
    }
    QCOMPARE(first.pathLengthM, second.pathLengthM);
    QCOMPARE(first.turnCount, second.turnCount);
}

void LawnmowerCoveragePlannerTest::_testSafetyInsetFailure()
{
    const LawnmowerCoveragePlanner planner;
    const CoveragePlanningSolution solution = planner.plan(rectangleProblem(4.0, 4.0, 10.0, 0.0, 2.0));

    QCOMPARE(solution.status, PlanningStatus::Failed);
    QCOMPARE(solution.error, CoveragePlanningError::SafetyInsetEmpty);
    QVERIFY(solution.path.empty());
}

void LawnmowerCoveragePlannerTest::_testNonMonotoneSweepFailure()
{
    CoveragePlanningProblem problem;
    problem.region.outerBoundary.vertices = {
        {0.0, 0.0}, {10.0, 0.0}, {10.0, 3.0}, {3.0, 3.0}, {3.0, 7.0}, {10.0, 7.0}, {10.0, 10.0}, {0.0, 10.0},
    };
    problem.swathWidthM = 2.0;
    problem.sweepAngleMode = SweepAngleMode::Manual;
    problem.requestedSweepAngleDeg = 90.0;

    const LawnmowerCoveragePlanner planner;
    const CoveragePlanningSolution solution = planner.plan(problem);

    QCOMPARE(solution.status, PlanningStatus::Failed);
    QCOMPARE(solution.error, CoveragePlanningError::NonMonotoneSweep);
    QVERIFY(solution.path.empty());
}

void LawnmowerCoveragePlannerTest::_testAutoModeIsDeferred()
{
    CoveragePlanningProblem problem = rectangleProblem(20.0, 10.0, 4.0, 0.0);
    problem.sweepAngleMode = SweepAngleMode::Auto;

    const LawnmowerCoveragePlanner planner;
    const CoveragePlanningSolution solution = planner.plan(problem);

    QCOMPARE(solution.status, PlanningStatus::Failed);
    QCOMPARE(solution.error, CoveragePlanningError::GeometryFailure);
    QVERIFY(solution.path.empty());
    QVERIFY(solution.message.find("P1-08") != std::string::npos);
}

UT_REGISTER_TEST_LIGHTWEIGHT(LawnmowerCoveragePlannerTest, TestLabel::Unit)
