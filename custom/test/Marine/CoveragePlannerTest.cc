#include "CoveragePlannerTest.h"

#include <memory>

#include "MockCoveragePlanner.h"
#include "PlannerRegistry.h"

using namespace Marine;

namespace {

MarineTask createValidTask()
{
    MarineTask task;
    task.region.outerBoundary.vertices = {
        {38.0, 121.0, 0.0},
        {38.0, 121.2, 0.0},
        {38.2, 121.2, 0.0},
        {38.2, 121.0, 0.0},
    };
    return task;
}

void comparePoints(const GeoPoint& actual, const GeoPoint& expected)
{
    QCOMPARE(actual.latitudeDeg, expected.latitudeDeg);
    QCOMPARE(actual.longitudeDeg, expected.longitudeDeg);
    QCOMPARE(actual.altitudeM, expected.altitudeM);
}

}  // namespace

void CoveragePlannerTest::_testRegisterAndLookup()
{
    PlannerRegistry registry;
    const auto planner = std::make_shared<MockCoveragePlanner>();

    QVERIFY(registry.registerPlanner(planner));
    QCOMPARE(registry.planner(planner->id()), planner);
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

void CoveragePlannerTest::_testValidPolygon()
{
    const MockCoveragePlanner planner;
    const PlanningResult result = planner.plan(createValidTask());

    QVERIFY(planner.id() == "marine.coverage.mock");
    QVERIFY(planner.displayName() == "Architecture Test Planner");
    QVERIFY(result.status == PlanningStatus::Success);
    QCOMPARE(result.path.size(), std::size_t{3});
    comparePoints(result.path[0], {38.0, 121.0, 0.0});
    comparePoints(result.path[1], {38.1, 121.1, 0.0});
    comparePoints(result.path[2], {38.2, 121.2, 0.0});
    QVERIFY(result.pathLengthM > 0.0);
    QVERIFY(result.message.find("Not for field operation") != std::string::npos);
}

void CoveragePlannerTest::_testInvalidPolygon()
{
    const MockCoveragePlanner planner;
    const PlanningResult result = planner.plan(MarineTask{});

    QVERIFY(result.status == PlanningStatus::InvalidInput);
    QVERIFY(result.path.empty());
    QCOMPARE(result.pathLengthM, 0.0);
    QVERIFY(!result.message.empty());
}

void CoveragePlannerTest::_testDeterministicPath()
{
    const MockCoveragePlanner planner;
    const MarineTask task = createValidTask();
    const PlanningResult first = planner.plan(task);
    const PlanningResult second = planner.plan(task);

    QVERIFY(first.status == second.status);
    QCOMPARE(first.path.size(), second.path.size());
    for (std::size_t index = 0; index < first.path.size(); ++index) {
        comparePoints(first.path[index], second.path[index]);
    }
    QCOMPARE(first.pathLengthM, second.pathLengthM);
    QVERIFY(first.message == second.message);
}

UT_REGISTER_TEST_LIGHTWEIGHT(CoveragePlannerTest, TestLabel::Unit)
