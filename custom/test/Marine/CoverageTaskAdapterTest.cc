#include "CoverageTaskAdapterTest.h"

#include <limits>
#include <memory>
#include <optional>

#include "CoverageGeometry.h"
#include "CoverageTaskAdapter.h"
#include "Geometry/MarineGeometry.h"
#include "MockCoveragePlanner.h"
#include "PlannerRegistry.h"
#include "PlanningInputIdentity.h"
#include "PlanningPathMetrics.h"

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
    task.safety.preferredSafetyMarginM = 3.5;
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
    QCOMPARE(problem.safety.hardSafetyMarginM, 2.5);
    QCOMPARE(problem.safety.preferredSafetyMarginM, 3.5);
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

    MarineTask missingOuter;
    missingOuter.planner.executionSafety.executionMarginM = 0.0;
    QVERIFY(!CoverageTaskAdapter::buildProblem(missingOuter, problem, reference, error));
    QVERIFY(error == CoveragePlanningError::InvalidSafetyMargin);
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
    QCOMPARE(problem.safety.hardSafetyMarginM, 4.01);

    task = createTask();
    task.safety.preferredSafetyMarginM = task.safety.hardSafetyMarginM - 0.1;
    QVERIFY(!CoverageTaskAdapter::buildProblem(task, problem, reference, error));
    QCOMPARE(error, CoveragePlanningError::InvalidPreferredSafetyMargin);
    QVERIFY(!reference.has_value());
    QVERIFY(problem.region.navigationBoundary.vertices.empty());

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

void CoverageTaskAdapterTest::_testOptionalClosingVertex()
{
    MarineTask task = createTask();
    task.region.noGoRegions.push_back({{{38.0005, 121.0005, 0.0}, {38.0005, 121.0008, 0.0}, {38.0008, 121.0005, 0.0}}});
    const auto baseline = PlanningInputIdentity::fromTask(task);
    QVERIFY(baseline.has_value());
    CoveragePlanningProblem openProblem;
    std::optional<GeoReference> openReference;
    CoveragePlanningError error;
    QVERIFY(CoverageTaskAdapter::buildProblem(task, openProblem, openReference, error));

    for (GeoPolygon* polygon :
         {&task.region.coverageBoundary, &task.region.navigationBoundary, &task.region.noGoRegions.front()}) {
        polygon->vertices.push_back(polygon->vertices.front());
    }
    QVERIFY(task.schemaValid());
    QVERIFY(baseline->matches(task));
    CoveragePlanningProblem closedProblem;
    std::optional<GeoReference> closedReference;
    QVERIFY(CoverageTaskAdapter::buildProblem(task, closedProblem, closedReference, error));
    QCOMPARE(closedReference->origin().latitudeDeg, openReference->origin().latitudeDeg);
    QCOMPARE(closedReference->origin().longitudeDeg, openReference->origin().longitudeDeg);
    QCOMPARE(closedProblem.region.coverageBoundary.vertices.size(),
             openProblem.region.coverageBoundary.vertices.size());
    QCOMPARE(closedProblem.region.navigationBoundary.vertices.size(),
             openProblem.region.navigationBoundary.vertices.size());
    QCOMPARE(closedProblem.region.noGoRegions.front().vertices.size(),
             openProblem.region.noGoRegions.front().vertices.size());
    for (std::size_t index = 0; index < openProblem.region.coverageBoundary.vertices.size(); ++index) {
        QCOMPARE(closedProblem.region.coverageBoundary.vertices[index].xM,
                 openProblem.region.coverageBoundary.vertices[index].xM);
        QCOMPARE(closedProblem.region.coverageBoundary.vertices[index].yM,
                 openProblem.region.coverageBoundary.vertices[index].yM);
    }
    QCOMPARE(buildCoverageGeometry(closedProblem.region).error, CoveragePlanningError::None);

    task.region.coverageBoundary.vertices.push_back(task.region.coverageBoundary.vertices.front());
    QVERIFY(!task.schemaValid());
    QVERIFY(!PlanningInputIdentity::fromTask(task).has_value());
}

void CoverageTaskAdapterTest::_testRejectInvalidSuccessPath()
{
    const auto reference = GeoReference::create(GeoPoint{38.0, 121.0, 0.0});
    QVERIFY(reference.has_value());
    CoveragePlanningSolution valid;
    valid.status = PlanningStatus::Success;
    valid.path = {{0, 0}, {10, 0}, {10, 5}};
    valid.legRoles = {PathLegRole::Coverage, PathLegRole::Transit};
    valid.coverageLengthM = 10;
    valid.transitLengthM = 5;
    valid.pathLengthM = 15;
    QCOMPARE(CoverageTaskAdapter::toPlanningResult(valid, *reference).status, PlanningStatus::Success);
    const auto reject = [&](const CoveragePlanningSolution& solution) {
        const auto result = CoverageTaskAdapter::toPlanningResult(solution, *reference);
        QCOMPARE(result.status, PlanningStatus::Failed);
        QVERIFY(result.path.empty());
        QVERIFY(result.legRoles.empty());
    };
    auto bad = valid;
    bad.path.resize(1);
    bad.legRoles.clear();
    reject(bad);
    bad = valid;
    bad.path[1] = bad.path[0];
    reject(bad);
    bad = valid;
    bad.legRoles.pop_back();
    reject(bad);
    bad = valid;
    bad.legRoles[0] = static_cast<PathLegRole>(999);
    reject(bad);
    bad = valid;
    bad.path[1].xM = std::numeric_limits<double>::infinity();
    reject(bad);
    bad = valid;
    bad.coverageLengthM += 2 * PathMetricsConsistencyToleranceM;
    bad.transitLengthM -= 2 * PathMetricsConsistencyToleranceM;
    reject(bad);
    bad = valid;
    bad.path[1].xM += 1;
    reject(bad);
    bad = valid;
    bad.coverageLengthM += PathMetricsConsistencyToleranceM / 2;
    QCOMPARE(CoverageTaskAdapter::toPlanningResult(bad, *reference).status, PlanningStatus::Success);
}

void CoverageTaskAdapterTest::_testPathMetricsConsistencyTolerance()
{
    // This centimeter-scale threshold checks bookkeeping consistency, not safe path clearance.
    QVERIFY(PathMetricsConsistencyToleranceM >= 0.01);
    const PlanningPathMetrics actual{};
    const double inside = std::nextafter(PathMetricsConsistencyToleranceM, 0.0);
    const double outside = std::nextafter(PathMetricsConsistencyToleranceM, std::numeric_limits<double>::infinity());
    for (int metric = 0; metric < 3; ++metric) {
        for (const double accepted : {inside, PathMetricsConsistencyToleranceM}) {
            double reported[3]{};
            reported[metric] = accepted;
            QVERIFY(planningPathMetricsMatch(actual, reported[0], reported[1], reported[2]));
        }
        double reported[3]{};
        reported[metric] = outside;
        QVERIFY(!planningPathMetricsMatch(actual, reported[0], reported[1], reported[2]));
    }
}

UT_REGISTER_TEST_LIGHTWEIGHT(CoverageTaskAdapterTest, TestLabel::Unit)
