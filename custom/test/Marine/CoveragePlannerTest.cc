#include "CoveragePlannerTest.h"

#include <memory>

#include "LawnmowerCoveragePlanner.h"
#include "MockCoveragePlanner.h"
#include "PlannerRegistry.h"

using namespace Marine;

namespace {

CoveragePlanningProblem createValidProblem()
{
    CoveragePlanningProblem problem;
    problem.region.outerBoundary.vertices = {
        {0.0, 0.0},
        {20.0, 0.0},
        {20.0, 10.0},
        {0.0, 10.0},
    };
    problem.swathWidthM = 5.0;
    problem.safetyMarginM = 1.0;
    problem.sweepAngleMode = SweepAngleMode::Manual;
    problem.requestedSweepAngleDeg = 90.0;
    return problem;
}

void comparePoints(const Point2D& actual, const Point2D& expected)
{
    QCOMPARE(actual.xM, expected.xM);
    QCOMPARE(actual.yM, expected.yM);
}

}  // namespace

void CoveragePlannerTest::_testRegisterAndLookup()
{
    PlannerRegistry registry;
    const auto mockPlanner = std::make_shared<MockCoveragePlanner>();
    const auto lawnmowerPlanner = std::make_shared<LawnmowerCoveragePlanner>();

    QVERIFY(registry.registerPlanner(mockPlanner));
    QVERIFY(registry.registerPlanner(lawnmowerPlanner));
    QCOMPARE(registry.planner(mockPlanner->id()), mockPlanner);
    QCOMPARE(registry.planner(lawnmowerPlanner->id()), lawnmowerPlanner);
    QVERIFY(!registry.registerPlanner(nullptr));
}

void CoveragePlannerTest::_testDuplicateId()
{
    PlannerRegistry registry;
    const auto first = std::make_shared<MockCoveragePlanner>();
    const auto duplicate = std::make_shared<MockCoveragePlanner>();

    QVERIFY(registry.registerPlanner(first));
    QVERIFY(!registry.registerPlanner(duplicate));
    QCOMPARE(registry.planner(first->id()), first);
}

void CoveragePlannerTest::_testMissingPlanner()
{
    const PlannerRegistry registry;

    QVERIFY(!registry.planner("marine.coverage.missing"));
}

void CoveragePlannerTest::_testValidProblem()
{
    const MockCoveragePlanner planner;
    const CoveragePlanningSolution solution = planner.plan(createValidProblem());

    QVERIFY(planner.id() == "marine.coverage.mock");
    QVERIFY(planner.displayName() == "Architecture Test Planner");
    QVERIFY(solution.status == PlanningStatus::Success);
    QVERIFY(solution.error == CoveragePlanningError::None);
    QCOMPARE(solution.path.size(), std::size_t{3});
    comparePoints(solution.path[0], {0.0, 0.0});
    comparePoints(solution.path[1], {10.0, 5.0});
    comparePoints(solution.path[2], {20.0, 10.0});
    QVERIFY(solution.pathLengthM > 0.0);
    QCOMPARE(solution.selectedSweepAngleDeg, 90.0);
    QCOMPARE(solution.turnCount, 1);
    QVERIFY(solution.message.find("Not for field operation") != std::string::npos);
}

void CoveragePlannerTest::_testInvalidProblem()
{
    const MockCoveragePlanner planner;
    const CoveragePlanningSolution solution = planner.plan(CoveragePlanningProblem{});

    QVERIFY(solution.status == PlanningStatus::InvalidInput);
    QVERIFY(solution.error == CoveragePlanningError::InvalidOuterBoundary);
    QVERIFY(solution.path.empty());
    QCOMPARE(solution.pathLengthM, 0.0);
    QVERIFY(!solution.message.empty());
}

void CoveragePlannerTest::_testCapabilityGate()
{
    const MockCoveragePlanner planner;
    CoveragePlanningProblem problem = createValidProblem();
    problem.region.noGoRegions.push_back({{{2.0, 2.0}, {3.0, 2.0}, {2.0, 3.0}}});

    const CoveragePlanningSolution solution = planner.plan(problem);
    QVERIFY(solution.status == PlanningStatus::Failed);
    QVERIFY(solution.error == CoveragePlanningError::UnsupportedNoGoRegion);
    QVERIFY(solution.path.empty());
    QVERIFY(solution.message.find("P2") != std::string::npos);
}

void CoveragePlannerTest::_testDeterministicPath()
{
    const MockCoveragePlanner planner;
    const CoveragePlanningProblem problem = createValidProblem();
    const CoveragePlanningSolution first = planner.plan(problem);
    const CoveragePlanningSolution second = planner.plan(problem);

    QVERIFY(first.status == second.status);
    QVERIFY(first.error == second.error);
    QCOMPARE(first.path.size(), second.path.size());
    for (std::size_t index = 0; index < first.path.size(); ++index) {
        comparePoints(first.path[index], second.path[index]);
    }
    QCOMPARE(first.pathLengthM, second.pathLengthM);
    QCOMPARE(first.selectedSweepAngleDeg, second.selectedSweepAngleDeg);
    QCOMPARE(first.turnCount, second.turnCount);
    QVERIFY(first.message == second.message);
}

UT_REGISTER_TEST_LIGHTWEIGHT(CoveragePlannerTest, TestLabel::Unit)
