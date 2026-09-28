#include "CoverageTaskAdapterTest.h"

#include <limits>
#include <memory>
#include <optional>

#include "CoverageGeometry.h"
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
    task.region.coverageBoundary.vertices = {
        {38.0, 121.0, 0.0},
        {38.0, 121.002, 0.0},
        {38.002, 121.002, 0.0},
        {38.002, 121.0, 0.0},
    };
    task.region.navigationBoundary = task.region.coverageBoundary;
    task.coverage.swathWidthM = 8.0;
    task.safety.hardSafetyMarginM = 2.5;
    task.safety.preferredSafetyMarginM = 2.5;
    task.coverage.sweepAngleMode = SweepAngleMode::Manual;
    task.coverage.sweepAngleDeg = 35.0;
    task.planner.plannerId = "marine.coverage.mock";
    task.planner.executionSafety.executionMarginM = 0.25;
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
    QCOMPARE(problem.region.coverageBoundary.vertices.size(), task.region.coverageBoundary.vertices.size());
    QVERIFY(problem.region.noGoRegions.empty());
    QCOMPARE(problem.swathWidthM, 8.0);
    QCOMPARE(problem.safetyMarginM, 2.5);
    QCOMPARE(problem.executionSafety.executionMarginM, 0.25);
    QVERIFY(problem.sweepAngleMode == SweepAngleMode::Manual);
    QCOMPARE(problem.requestedSweepAngleDeg, 35.0);
    QVERIFY(problem.region.isFinite());

    for (std::size_t index = 0; index < problem.region.coverageBoundary.vertices.size(); ++index) {
        const std::optional<GeoPoint> roundTrip = reference->toGeo(problem.region.coverageBoundary.vertices[index]);
        QVERIFY(roundTrip.has_value());
        compareGeoPoint(*roundTrip, task.region.coverageBoundary.vertices[index]);
    }
}

void CoverageTaskAdapterTest::_testSeparateNavigationConversion()
{
    MarineTask task = createTask();
    task.region.navigationBoundary.vertices = {
        {37.999, 120.999, 0.0}, {37.999, 121.004, 0.0}, {38.004, 121.004, 0.0}, {38.004, 120.999, 0.0}};
    task.region.noGoRegions = {{{{38.001, 121.003, 0.0}, {38.001, 121.0035, 0.0}, {38.0015, 121.0035, 0.0}}}};
    CoveragePlanningProblem problem;
    std::optional<GeoReference> reference;
    CoveragePlanningError error;
    QVERIFY(CoverageTaskAdapter::buildProblem(task, problem, reference, error));
    QCOMPARE(problem.region.navigationBoundary.vertices.size(), task.region.navigationBoundary.vertices.size());
    for (std::size_t index = 0; index < problem.region.navigationBoundary.vertices.size(); ++index) {
        const auto roundTrip = reference->toGeo(problem.region.navigationBoundary.vertices[index]);
        QVERIFY(roundTrip.has_value());
        compareGeoPoint(*roundTrip, task.region.navigationBoundary.vertices[index]);
    }
    QCOMPARE(buildCoverageGeometry(problem.region).error, CoveragePlanningError::None);
    QCOMPARE(problem.region.noGoRegions.size(), std::size_t{1});
    task.region.navigationBoundary = {};
    QVERIFY(!CoverageTaskAdapter::buildProblem(task, problem, reference, error));
    QCOMPARE(error, CoveragePlanningError::InvalidNavigationBoundary);
    QVERIFY(!reference.has_value());
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
    invalidOuter.region.coverageBoundary.vertices.front().latitudeDeg = std::numeric_limits<double>::quiet_NaN();
    QVERIFY(!CoverageTaskAdapter::buildProblem(invalidOuter, problem, reference, error));
    QVERIFY(error == CoveragePlanningError::InvalidOuterBoundary);

    MarineTask invalidWgs84 = createTask();
    invalidWgs84.region.coverageBoundary.vertices.front().latitudeDeg = 91.0;
    QVERIFY(!CoverageTaskAdapter::buildProblem(invalidWgs84, problem, reference, error));
    QVERIFY(error == CoveragePlanningError::InvalidOuterBoundary);

    MarineTask selfIntersecting = createTask();
    selfIntersecting.region.coverageBoundary.vertices = {
        {38.0, 121.0, 0.0},
        {38.002, 121.002, 0.0},
        {38.002, 121.0, 0.0},
        {38.0, 121.002, 0.0},
    };
    QVERIFY(!CoverageTaskAdapter::buildProblem(selfIntersecting, problem, reference, error));
    QVERIFY(error == CoveragePlanningError::InvalidOuterBoundary);
}

void CoverageTaskAdapterTest::_testValidationAndNormalization()
{
    CoveragePlanningProblem problem;
    std::optional<GeoReference> reference;
    CoveragePlanningError error = CoveragePlanningError::None;

    MarineTask task = createTask();
    task.coverage.sweepAngleDeg = -45.0;
    QVERIFY(CoverageTaskAdapter::buildProblem(task, problem, reference, error));
    QCOMPARE(problem.requestedSweepAngleDeg, 135.0);

    task = createTask();
    task.coverage.swathWidthM = 0.0;
    QVERIFY(!CoverageTaskAdapter::buildProblem(task, problem, reference, error));
    QVERIFY(error == CoveragePlanningError::InvalidSwathWidth);

    task = createTask();
    task.safety.hardSafetyMarginM = 4.01;
    task.safety.preferredSafetyMarginM = 4.01;
    QVERIFY(CoverageTaskAdapter::buildProblem(task, problem, reference, error));
    QCOMPARE(problem.safetyMarginM, 4.01);

    task = createTask();
    task.planner.executionSafety.executionMarginM = -0.1;
    QVERIFY(!CoverageTaskAdapter::buildProblem(task, problem, reference, error));
    QCOMPARE(error, CoveragePlanningError::InvalidExecutionMargin);
}

void CoverageTaskAdapterTest::_testNoGoConversion()
{
    MarineTask task = createTask();
    task.region.noGoRegions.push_back({{
        {38.0005, 121.0005, 0.0},
        {38.0005, 121.0008, 0.0},
        {38.0008, 121.0005, 0.0},
    }});

    CoveragePlanningProblem problem;
    std::optional<GeoReference> reference;
    CoveragePlanningError error = CoveragePlanningError::None;
    QVERIFY(CoverageTaskAdapter::buildProblem(task, problem, reference, error));
    QCOMPARE(error, CoveragePlanningError::None);
    QVERIFY(reference.has_value());
    QCOMPARE(problem.region.noGoRegions.size(), std::size_t{1});
    QCOMPARE(problem.region.noGoRegions.front().vertices.size(), task.region.noGoRegions.front().vertices.size());
    for (std::size_t index = 0; index < problem.region.noGoRegions.front().vertices.size(); ++index) {
        const std::optional<GeoPoint> roundTrip = reference->toGeo(problem.region.noGoRegions.front().vertices[index]);
        QVERIFY(roundTrip.has_value());
        compareGeoPoint(*roundTrip, task.region.noGoRegions.front().vertices[index]);
    }

    task.region.noGoRegions.front().vertices.front().latitudeDeg = 91.0;
    QVERIFY(!CoverageTaskAdapter::buildProblem(task, problem, reference, error));
    QCOMPARE(error, CoveragePlanningError::InvalidNoGoRegion);
    QVERIFY(!reference.has_value());
}

void CoverageTaskAdapterTest::_testSolutionMapping()
{
    const std::optional<GeoReference> reference = GeoReference::create({38.0, 121.0, 0.0});
    QVERIFY(reference.has_value());

    CoveragePlanningSolution solution;
    solution.status = PlanningStatus::Success;
    solution.path = {{0.0, 0.0}, {100.0, 0.0}, {100.0, 100.0}};
    solution.legRoles = {PathLegRole::Coverage, PathLegRole::Transit};
    solution.coverageLengthM = 100.0;
    solution.transitLengthM = 100.0;
    solution.pathLengthM = 200.0;
    solution.selectedSweepAngleDeg = 90.0;
    solution.cellCount = 3;
    solution.turnCount = 1;
    solution.message = "Local plan";

    const PlanningResult result = CoverageTaskAdapter::toPlanningResult(solution, *reference);
    QVERIFY(result.status == PlanningStatus::Success);
    QCOMPARE(result.path.size(), solution.path.size());
    QCOMPARE(result.legRoles.size(), solution.legRoles.size());
    QCOMPARE(result.legRoles[0], PathLegRole::Coverage);
    QCOMPARE(result.legRoles[1], PathLegRole::Transit);
    compareGeoPoint(result.path.front(), reference->origin());
    QCOMPARE(result.pathLengthM, 200.0);
    QCOMPARE(result.coverageLengthM, 100.0);
    QCOMPARE(result.transitLengthM, 100.0);
    QCOMPARE(result.selectedSweepAngleDeg, 90.0);
    QCOMPARE(result.cellCount, 3);
    QCOMPARE(result.turnCount, 1);
    QVERIFY(result.message == solution.message);

    solution.path[1].xM = std::numeric_limits<double>::quiet_NaN();
    const PlanningResult invalidResult = CoverageTaskAdapter::toPlanningResult(solution, *reference);
    QVERIFY(invalidResult.status == PlanningStatus::Failed);
    QVERIFY(invalidResult.path.empty());
    QCOMPARE(invalidResult.pathLengthM, 0.0);
    QCOMPARE(invalidResult.coverageLengthM, 0.0);
    QCOMPARE(invalidResult.transitLengthM, 0.0);
    QCOMPARE(invalidResult.cellCount, 0);
    QVERIFY(!invalidResult.message.empty());

    solution = {};
    solution.status = PlanningStatus::Success;
    solution.path = {{0.0, 0.0}, {1.0, 0.0}};
    const PlanningResult invalidRolesResult = CoverageTaskAdapter::toPlanningResult(solution, *reference);
    QVERIFY(invalidRolesResult.status == PlanningStatus::Failed);
    QVERIFY(invalidRolesResult.path.empty());
    QVERIFY(invalidRolesResult.legRoles.empty());
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
    QCOMPARE(result.legRoles.size(), std::size_t{2});
    QCOMPARE(result.legRoles[0], PathLegRole::Coverage);
    QCOMPARE(result.legRoles[1], PathLegRole::Transit);
    QVERIFY(result.pathLengthM > 0.0);
    QVERIFY(result.coverageLengthM > 0.0);
    QVERIFY(result.transitLengthM > 0.0);
    QCOMPARE(result.pathLengthM, result.coverageLengthM + result.transitLengthM);
    QCOMPARE(result.cellCount, 1);
    QCOMPARE(result.selectedSweepAngleDeg, 35.0);
    QCOMPARE(result.turnCount, 1);
}

UT_REGISTER_TEST_LIGHTWEIGHT(CoverageTaskAdapterTest, TestLabel::Unit)
