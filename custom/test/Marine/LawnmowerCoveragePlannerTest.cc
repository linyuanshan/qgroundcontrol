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
    QCOMPARE(solution.path.size(), std::size_t{10});
    comparePoint(solution.path[0], 18.0, 0.0);
    comparePoint(solution.path[1], 18.0, 10.0);
    comparePoint(solution.path[2], 14.0, 10.0);
    comparePoint(solution.path[3], 14.0, 0.0);
    comparePoint(solution.path[8], 2.0, 0.0);
    comparePoint(solution.path[9], 2.0, 10.0);
    compareWithinTolerance(solution.pathLengthM, 66.0);
    QCOMPARE(solution.selectedSweepAngleDeg, 0.0);
    QCOMPARE(solution.turnCount, 4);
}

void LawnmowerCoveragePlannerTest::_testRectangleManualNinetyDegrees()
{
    const LawnmowerCoveragePlanner planner;
    const CoveragePlanningSolution solution = planner.plan(rectangleProblem(20.0, 10.0, 5.0, 90.0));

    QCOMPARE(solution.status, PlanningStatus::Success);
    QCOMPARE(solution.path.size(), std::size_t{4});
    comparePoint(solution.path[0], 0.0, 2.5);
    comparePoint(solution.path[1], 20.0, 2.5);
    comparePoint(solution.path[2], 20.0, 7.5);
    comparePoint(solution.path[3], 0.0, 7.5);
    compareWithinTolerance(solution.pathLengthM, 45.0);
    QCOMPARE(solution.selectedSweepAngleDeg, 90.0);
    QCOMPARE(solution.turnCount, 1);
}

void LawnmowerCoveragePlannerTest::_testNarrowRegionUsesOneLane()
{
    const LawnmowerCoveragePlanner planner;
    const CoveragePlanningSolution solution = planner.plan(rectangleProblem(12.0, 3.0, 5.0, 90.0));

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
    constexpr double SwathWidthM = 4.0;
    const CoveragePlanningSolution solution = planner.plan(rectangleProblem(20.0, 10.0, SwathWidthM, 90.0, 1.0));

    QCOMPARE(solution.status, PlanningStatus::Success);
    QCOMPARE(solution.path.size(), std::size_t{6});
    comparePoint(solution.path[0], 1.0, 2.0);
    comparePoint(solution.path[1], 19.0, 2.0);
    comparePoint(solution.path[2], 19.0, 5.0);
    comparePoint(solution.path[3], 1.0, 5.0);
    comparePoint(solution.path[4], 1.0, 8.0);
    comparePoint(solution.path[5], 19.0, 8.0);
    compareWithinTolerance(solution.pathLengthM, 60.0);
    QCOMPARE(solution.turnCount, 2);

    const double halfSwathM = SwathWidthM / 2.0;
    QVERIFY(solution.path.front().yM - halfSwathM <= Geometry::LengthEpsilonM);
    QVERIFY(solution.path.back().yM + halfSwathM >= 10.0 - Geometry::LengthEpsilonM);
}

void LawnmowerCoveragePlannerTest::_testCoverageImpossibleWithSafetyMargin()
{
    const LawnmowerCoveragePlanner planner;
    const CoveragePlanningSolution solution = planner.plan(rectangleProblem(20.0, 10.0, 4.0, 90.0, 2.1));

    QCOMPARE(solution.status, PlanningStatus::Failed);
    QCOMPARE(solution.error, CoveragePlanningError::CoverageImpossibleWithSafetyMargin);
    QVERIFY(solution.path.empty());
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
    problem.requestedSweepAngleDeg = 0.0;

    const LawnmowerCoveragePlanner planner;
    const CoveragePlanningSolution solution = planner.plan(problem);

    QCOMPARE(solution.status, PlanningStatus::Failed);
    QCOMPARE(solution.error, CoveragePlanningError::NonMonotoneSweep);
    QVERIFY(solution.path.empty());
}

void LawnmowerCoveragePlannerTest::_testUnsafeConnectorFailure()
{
    CoveragePlanningProblem problem;
    problem.region.outerBoundary.vertices = {
        {0.0, 0.0}, {10.0, 0.0}, {10.0, 1.5}, {3.0, 3.0}, {10.0, 4.5}, {10.0, 10.0}, {0.0, 10.0},
    };
    problem.swathWidthM = 4.0;
    problem.sweepAngleMode = SweepAngleMode::Manual;
    problem.requestedSweepAngleDeg = 90.0;

    const LawnmowerCoveragePlanner planner;
    const CoveragePlanningSolution solution = planner.plan(problem);

    QCOMPARE(solution.status, PlanningStatus::Failed);
    QCOMPARE(solution.error, CoveragePlanningError::UnsafeConnector);
    QVERIFY(solution.path.empty());
}

void LawnmowerCoveragePlannerTest::_testSuccessfulPathInvariants()
{
    const CoveragePlanningProblem problem = rectangleProblem(20.0, 10.0, 4.0, 90.0, 1.0);
    const LawnmowerCoveragePlanner planner;
    const CoveragePlanningSolution solution = planner.plan(problem);
    const Geometry::PolygonInsetResult inset =
        Geometry::insetPolygon(problem.region.outerBoundary, problem.safetyMarginM);

    QCOMPARE(solution.status, PlanningStatus::Success);
    QCOMPARE(inset.status, Geometry::PolygonInsetStatus::Success);
    QVERIFY(solution.path.size() >= 2);
    for (const Point2D& point : solution.path) {
        QVERIFY(point.isFinite());
        QVERIFY(Geometry::containsPoint(inset.polygon, point));
    }
    for (std::size_t index = 1; index < solution.path.size(); ++index) {
        QVERIFY(Geometry::containsSegment(inset.polygon, solution.path[index - 1], solution.path[index]));
    }
    QVERIFY(std::isfinite(solution.pathLengthM));
    QVERIFY(solution.pathLengthM > 0.0);
    for (std::size_t index = 2; index < solution.path.size(); index += 2) {
        QVERIFY(std::abs(solution.path[index].yM - solution.path[index - 2].yM) <=
                problem.swathWidthM + Geometry::LengthEpsilonM);
    }
    QCOMPARE(solution.turnCount, static_cast<int>((solution.path.size() / 2) - 1));
}

void LawnmowerCoveragePlannerTest::_testAutoRectangleRanksTurnCount()
{
    CoveragePlanningProblem problem = rectangleProblem(20.0, 10.0, 4.0, 0.0);
    problem.sweepAngleMode = SweepAngleMode::Auto;

    const LawnmowerCoveragePlanner planner;
    const CoveragePlanningSolution solution = planner.plan(problem);

    QCOMPARE(solution.status, PlanningStatus::Success);
    QCOMPARE(solution.error, CoveragePlanningError::None);
    QCOMPARE(solution.selectedSweepAngleDeg, 90.0);
    QCOMPARE(solution.turnCount, 2);
}

void LawnmowerCoveragePlannerTest::_testAutoRanksPathLengthBeforeAngle()
{
    CoveragePlanningProblem problem = rectangleProblem(20.0, 10.0, 25.0, 0.0);
    problem.sweepAngleMode = SweepAngleMode::Auto;

    const LawnmowerCoveragePlanner planner;
    const CoveragePlanningSolution solution = planner.plan(problem);

    QCOMPARE(solution.status, PlanningStatus::Success);
    QCOMPARE(solution.turnCount, 0);
    QCOMPARE(solution.selectedSweepAngleDeg, 0.0);
    compareWithinTolerance(solution.pathLengthM, 10.0);
}

void LawnmowerCoveragePlannerTest::_testAutoAngleTieBreak()
{
    CoveragePlanningProblem problem = rectangleProblem(10.0, 10.0, 20.0, 0.0);
    problem.sweepAngleMode = SweepAngleMode::Auto;

    const LawnmowerCoveragePlanner planner;
    const CoveragePlanningSolution solution = planner.plan(problem);

    QCOMPARE(solution.status, PlanningStatus::Success);
    QCOMPARE(solution.turnCount, 0);
    QCOMPARE(solution.selectedSweepAngleDeg, 0.0);
    compareWithinTolerance(solution.pathLengthM, 10.0);
}

void LawnmowerCoveragePlannerTest::_testAutoRotatedRectangle()
{
    CoveragePlanningProblem problem;
    problem.region.outerBoundary.vertices = {
        {0.0, 0.0},
        {17.3205080757, 10.0},
        {12.3205080757, 18.6602540378},
        {-5.0, 8.6602540378},
    };
    problem.swathWidthM = 4.0;
    problem.sweepAngleMode = SweepAngleMode::Auto;

    const LawnmowerCoveragePlanner planner;
    const CoveragePlanningSolution solution = planner.plan(problem);

    QCOMPARE(solution.status, PlanningStatus::Success);
    compareWithinTolerance(solution.selectedSweepAngleDeg, 60.0);
    QCOMPARE(solution.turnCount, 2);
}

void LawnmowerCoveragePlannerTest::_testAutoFiltersNonMonotoneCandidates()
{
    CoveragePlanningProblem problem;
    problem.region.outerBoundary.vertices = {
        {0.0, 0.0}, {10.0, 0.0}, {10.0, 3.0}, {3.0, 3.0}, {3.0, 7.0}, {10.0, 7.0}, {10.0, 10.0}, {0.0, 10.0},
    };
    problem.swathWidthM = 20.0;
    problem.sweepAngleMode = SweepAngleMode::Auto;

    const LawnmowerCoveragePlanner planner;
    const CoveragePlanningSolution solution = planner.plan(problem);

    QCOMPARE(solution.status, PlanningStatus::Success);
    QCOMPARE(solution.selectedSweepAngleDeg, 90.0);
    QCOMPARE(solution.turnCount, 0);
}

void LawnmowerCoveragePlannerTest::_testAutoIrregularPolygonDeterminism()
{
    CoveragePlanningProblem problem;
    problem.region.outerBoundary.vertices = {
        {-2.0, 4.0}, {0.0, 0.0}, {12.0, -1.0}, {18.0, 5.0}, {13.0, 10.0}, {2.0, 9.0},
    };
    problem.swathWidthM = 3.0;
    problem.sweepAngleMode = SweepAngleMode::Auto;

    const LawnmowerCoveragePlanner planner;
    const CoveragePlanningSolution first = planner.plan(problem);
    const CoveragePlanningSolution second = planner.plan(problem);

    QCOMPARE(first.status, PlanningStatus::Success);
    QCOMPARE(first.error, CoveragePlanningError::None);
    QCOMPARE(first.selectedSweepAngleDeg, second.selectedSweepAngleDeg);
    QCOMPARE(first.turnCount, second.turnCount);
    QCOMPARE(first.pathLengthM, second.pathLengthM);
    QCOMPARE(first.path.size(), second.path.size());
    for (std::size_t index = 0; index < first.path.size(); ++index) {
        QCOMPARE(first.path[index].xM, second.path[index].xM);
        QCOMPARE(first.path[index].yM, second.path[index].yM);
    }
}

UT_REGISTER_TEST_LIGHTWEIGHT(LawnmowerCoveragePlannerTest, TestLabel::Unit)
