#include "CoverageTaskAdapterTest.h"

#include <limits>
#include <memory>
#include <optional>

#include "CoverageTaskAdapter.h"
#include "MockCoveragePlanner.h"
#include "PlannerRegistry.h"

using namespace Marine;

namespace {

constexpr double CoordinateToleranceDeg = 1e-9;

MarineTask createTask()
{
    MarineTask task;
    task.name = "Harbor inspection";
    task.region.outerBoundary.vertices = {
        {38.0, 121.0, 0.0},
        {38.0, 121.002, 0.0},
        {38.002, 121.002, 0.0},
        {38.002, 121.0, 0.0},
    };
    task.region.noGoRegions.push_back({{
        {38.0005, 121.0005, 0.0},
        {38.0005, 121.0008, 0.0},
        {38.0008, 121.0005, 0.0},
    }});
    task.coverage.swathWidthM = 8.0;
    task.coverage.safetyMarginM = 2.5;
    task.coverage.sweepAngleMode = SweepAngleMode::Manual;
    task.coverage.sweepAngleDeg = 35.0;
    task.planner.plannerId = "marine.coverage.mock";
    return task;
}

void compareGeoPoint(const GeoPoint& actual, const GeoPoint& expected)
{
    QVERIFY(qAbs(actual.latitudeDeg - expected.latitudeDeg) < CoordinateToleranceDeg);
    QVERIFY(qAbs(actual.longitudeDeg - expected.longitudeDeg) < CoordinateToleranceDeg);
    QCOMPARE(actual.altitudeM, 0.0);
}

}  // namespace

void CoverageTaskAdapterTest::_testBuildProblem()
{
    const MarineTask task = createTask();
    CoveragePlanningProblem problem;
    std::optional<GeoReference> reference;
    CoveragePlanningError error = CoveragePlanningError::GeometryFailure;

    QVERIFY(CoverageTaskAdapter::buildProblem(task, problem, reference, error));
    QVERIFY(reference.has_value());
    QVERIFY(error == CoveragePlanningError::None);
    QVERIFY(qAbs(reference->origin().latitudeDeg - 38.001) < CoordinateToleranceDeg);
    QVERIFY(qAbs(reference->origin().longitudeDeg - 121.001) < CoordinateToleranceDeg);
    QCOMPARE(problem.region.outerBoundary.vertices.size(), task.region.outerBoundary.vertices.size());
    QCOMPARE(problem.region.noGoRegions.size(), task.region.noGoRegions.size());
    QCOMPARE(problem.region.noGoRegions.front().vertices.size(), task.region.noGoRegions.front().vertices.size());
    QCOMPARE(problem.swathWidthM, 8.0);
    QCOMPARE(problem.safetyMarginM, 2.5);
    QVERIFY(problem.sweepAngleMode == SweepAngleMode::Manual);
    QCOMPARE(problem.requestedSweepAngleDeg, 35.0);
    QVERIFY(problem.region.isFinite());

    for (std::size_t index = 0; index < problem.region.outerBoundary.vertices.size(); ++index) {
        const std::optional<GeoPoint> roundTrip = reference->toGeo(problem.region.outerBoundary.vertices[index]);
        QVERIFY(roundTrip.has_value());
        compareGeoPoint(*roundTrip, task.region.outerBoundary.vertices[index]);
    }
}

void CoverageTaskAdapterTest::_testInvalidTaskGeometry()
{
    CoveragePlanningProblem problem;
    std::optional<GeoReference> reference;
    CoveragePlanningError error = CoveragePlanningError::None;

    QVERIFY(!CoverageTaskAdapter::buildProblem(MarineTask{}, problem, reference, error));
    QVERIFY(error == CoveragePlanningError::InvalidOuterBoundary);
    QVERIFY(!reference.has_value());

    MarineTask invalidOuter = createTask();
    invalidOuter.region.outerBoundary.vertices.front().latitudeDeg = std::numeric_limits<double>::quiet_NaN();
    QVERIFY(!CoverageTaskAdapter::buildProblem(invalidOuter, problem, reference, error));
    QVERIFY(error == CoveragePlanningError::InvalidOuterBoundary);

    MarineTask invalidNoGo = createTask();
    invalidNoGo.region.noGoRegions.front().vertices.front().longitudeDeg = std::numeric_limits<double>::infinity();
    QVERIFY(!CoverageTaskAdapter::buildProblem(invalidNoGo, problem, reference, error));
    QVERIFY(error == CoveragePlanningError::GeometryFailure);
}

void CoverageTaskAdapterTest::_testSolutionMapping()
{
    const std::optional<GeoReference> reference = GeoReference::create({38.0, 121.0, 0.0});
    QVERIFY(reference.has_value());

    CoveragePlanningSolution solution;
    solution.status = PlanningStatus::Success;
    solution.path = {{0.0, 0.0}, {100.0, 0.0}, {100.0, 100.0}};
    solution.pathLengthM = 200.0;
    solution.selectedSweepAngleDeg = 90.0;
    solution.turnCount = 1;
    solution.message = "Local plan";

    const PlanningResult result = CoverageTaskAdapter::toPlanningResult(solution, *reference);
    QVERIFY(result.status == PlanningStatus::Success);
    QCOMPARE(result.path.size(), solution.path.size());
    compareGeoPoint(result.path.front(), reference->origin());
    QCOMPARE(result.pathLengthM, 200.0);
    QCOMPARE(result.selectedSweepAngleDeg, 90.0);
    QCOMPARE(result.turnCount, 1);
    QVERIFY(result.message == solution.message);

    solution.path[1].xM = std::numeric_limits<double>::quiet_NaN();
    const PlanningResult invalidResult = CoverageTaskAdapter::toPlanningResult(solution, *reference);
    QVERIFY(invalidResult.status == PlanningStatus::Failed);
    QVERIFY(invalidResult.path.empty());
    QCOMPARE(invalidResult.pathLengthM, 0.0);
    QVERIFY(!invalidResult.message.empty());
}

void CoverageTaskAdapterTest::_testTaskPlannerRoundTrip()
{
    const MarineTask task = createTask();
    CoveragePlanningProblem problem;
    std::optional<GeoReference> reference;
    CoveragePlanningError error = CoveragePlanningError::None;
    QVERIFY(CoverageTaskAdapter::buildProblem(task, problem, reference, error));

    PlannerRegistry registry;
    QVERIFY(registry.registerPlanner(std::make_shared<MockCoveragePlanner>()));
    const std::shared_ptr<ICoveragePlanner> planner = registry.planner(task.planner.plannerId);
    QVERIFY(planner);

    const CoveragePlanningSolution solution = planner->plan(problem);
    const PlanningResult result = CoverageTaskAdapter::toPlanningResult(solution, *reference);
    QVERIFY(result.status == PlanningStatus::Success);
    QCOMPARE(result.path.size(), std::size_t{3});
    QVERIFY(result.pathLengthM > 0.0);
    QCOMPARE(result.selectedSweepAngleDeg, 35.0);
    QCOMPARE(result.turnCount, 1);
}

UT_REGISTER_TEST_LIGHTWEIGHT(CoverageTaskAdapterTest, TestLabel::Unit)
