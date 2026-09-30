#include "CoverageGeometryTest.h"

#include <limits>
#include <utility>

#include "BoustrophedonCoveragePlanner.h"
#include "CoverageGeometry.h"
#include "CoverageProblemValidator.h"
#include "Geometry/MarineGeometry.h"
#include "Geometry/PolygonRegion.h"
#include "LawnmowerCoveragePlanner.h"
#include "MockCoveragePlanner.h"

using namespace Marine;

Q_DECLARE_METATYPE(Region2D)

namespace {

Polygon2D rectangle(double left, double bottom, double right, double top)
{
    return {{{left, bottom}, {right, bottom}, {right, top}, {left, top}}};
}

Region2D nestedRegion(std::vector<Polygon2D> obstacles = {})
{
    return {.coverageBoundary = rectangle(0, 0, 10, 10),
            .navigationBoundary = rectangle(-10, -10, 20, 20),
            .noGoRegions = std::move(obstacles)};
}

}  // namespace

void CoverageGeometryTest::_testRegionSets_data()
{
    QTest::addColumn<Region2D>("input");
    QTest::addColumn<double>("targetArea");
    QTest::addColumn<double>("navigationArea");
    QTest::addColumn<int>("components");
    QTest::addColumn<int>("targetHoles");
    QTest::addColumn<int>("navigationHoles");
    Region2D equal = nestedRegion();
    equal.navigationBoundary = equal.coverageBoundary;
    QTest::newRow("C=N") << equal << 100.0 << 100.0 << 1 << 0 << 0;
    QTest::newRow("C-inside-N") << nestedRegion() << 100.0 << 900.0 << 1 << 0 << 0;
    Region2D touching = nestedRegion();
    touching.navigationBoundary = rectangle(0, 0, 20, 20);
    QTest::newRow("C-touches-N") << touching << 100.0 << 400.0 << 1 << 0 << 0;
    QTest::newRow("O-inside-C") << nestedRegion({rectangle(2, 2, 4, 4)}) << 96.0 << 896.0 << 1 << 1 << 1;
    QTest::newRow("O-outside-C") << nestedRegion({rectangle(12, 2, 14, 4)}) << 100.0 << 896.0 << 1 << 0 << 1;
    QTest::newRow("O-crosses-C") << nestedRegion({rectangle(8, 2, 12, 4)}) << 96.0 << 892.0 << 1 << 0 << 1;
    QTest::newRow("O-touches-C-outside") << nestedRegion({rectangle(10, 2, 12, 4)}) << 100.0 << 896.0 << 1 << 0 << 1;
    QTest::newRow("O-touches-C-inside") << nestedRegion({rectangle(8, 2, 10, 4)}) << 96.0 << 896.0 << 1 << 0 << 1;
    QTest::newRow("O-point-touches-C") << nestedRegion({rectangle(10, 10, 12, 12)}) << 100.0 << 896.0 << 1 << 0 << 1;
    QTest::newRow("split-target") << nestedRegion({rectangle(4, -2, 6, 12)}) << 80.0 << 872.0 << 2 << 0 << 1;
    QTest::newRow("split-target-with-hole")
        << nestedRegion({rectangle(4, -2, 6, 12), rectangle(1, 2, 2, 4)}) << 78.0 << 870.0 << 2 << 1 << 2;
}

void CoverageGeometryTest::_testRegionSets()
{
    QFETCH(Region2D, input);
    QFETCH(double, targetArea);
    QFETCH(double, navigationArea);
    QFETCH(int, components);
    QFETCH(int, targetHoles);
    QFETCH(int, navigationHoles);
    const auto result = buildCoverageGeometry(input);
    QCOMPARE(result.error, CoveragePlanningError::None);
    QCOMPARE(result.geometry.coverageTarget.size(), static_cast<std::size_t>(components));
    QCOMPARE(result.geometry.rawNavigationFreeSpace.size(), std::size_t{1});
    int holes = 0;
    for (const auto& component : result.geometry.coverageTarget) {
        QVERIFY(Geometry::isValidPolygonRegion(component));
        holes += static_cast<int>(component.holes.size());
    }
    QCOMPARE(holes, targetHoles);
    QCOMPARE(result.geometry.rawNavigationFreeSpace.front().holes.size(), static_cast<std::size_t>(navigationHoles));
    const auto target = Geometry::polygonRegionArea(result.geometry.coverageTarget);
    const auto navigation = Geometry::polygonRegionArea(result.geometry.rawNavigationFreeSpace);
    QCOMPARE(target.status, Geometry::PolygonRegionOperationStatus::Success);
    QCOMPARE(navigation.status, Geometry::PolygonRegionOperationStatus::Success);
    QCOMPARE(target.areaM2, targetArea);
    QCOMPARE(navigation.areaM2, navigationArea);
}

void CoverageGeometryTest::_testInvalidTopology_data()
{
    QTest::addColumn<Region2D>("input");
    QTest::addColumn<int>("error");
    auto add = [](const char* name, const Region2D& input, CoveragePlanningError error) {
        QTest::newRow(name) << input << static_cast<int>(error);
    };
    auto input = nestedRegion();
    input.coverageBoundary = rectangle(0, 0, 21, 10);
    add("C-outside-N", input, CoveragePlanningError::CoverageOutsideNavigationBoundary);
    input.coverageBoundary = {{{1, 1}, {9, 1}, {1, 9}}};
    input.navigationBoundary = {{{0, 0}, {10, 0}, {10, 2}, {2, 2}, {2, 10}, {0, 10}}};
    add("C-edges-leave-concave-N", input, CoveragePlanningError::CoverageOutsideNavigationBoundary);
    input = nestedRegion();
    input.coverageBoundary = {{{0, 0}, {5, 0}, {10, 0}}};
    add("degenerate-C", input, CoveragePlanningError::InvalidOuterBoundary);
    input = nestedRegion();
    input.coverageBoundary = {{{0, 0}, {10, 10}, {0, 10}, {10, 0}}};
    add("self-crossing-C", input, CoveragePlanningError::InvalidOuterBoundary);
    input = nestedRegion();
    input.coverageBoundary.vertices.front().xM = std::numeric_limits<double>::quiet_NaN();
    add("nonfinite-C", input, CoveragePlanningError::InvalidOuterBoundary);
    input = nestedRegion();
    input.navigationBoundary = {};
    add("missing-N", input, CoveragePlanningError::InvalidNavigationBoundary);
    input.navigationBoundary = {{{0, 0}, {10, 0}, {20, 0}}};
    add("degenerate-N", input, CoveragePlanningError::InvalidNavigationBoundary);
    input.navigationBoundary = {{{-10, -10}, {20, 20}, {-10, 20}, {20, -10}}};
    add("self-crossing-N", input, CoveragePlanningError::InvalidNavigationBoundary);
    input = nestedRegion();
    input.navigationBoundary.vertices.front().xM = std::numeric_limits<double>::infinity();
    add("nonfinite-N", input, CoveragePlanningError::InvalidNavigationBoundary);
    add("O-outside-N", nestedRegion({rectangle(21, 0, 23, 2)}), CoveragePlanningError::NoGoOutsideBoundary);
    add("O-touches-N", nestedRegion({rectangle(18, 0, 20, 2)}), CoveragePlanningError::NoGoBoundaryConflict);
    add("O-crosses-N", nestedRegion({rectangle(18, 0, 22, 2)}), CoveragePlanningError::NoGoBoundaryConflict);
    add("O-overlap", nestedRegion({rectangle(1, 1, 4, 4), rectangle(3, 3, 5, 5)}),
        CoveragePlanningError::NoGoOverlapOrTouch);
    add("O-touch", nestedRegion({rectangle(1, 1, 4, 4), rectangle(4, 1, 6, 4)}),
        CoveragePlanningError::NoGoOverlapOrTouch);
    add("O-nesting", nestedRegion({rectangle(1, 1, 8, 8), rectangle(2, 2, 4, 4)}),
        CoveragePlanningError::NoGoOverlapOrTouch);
    add("O-point-touch", nestedRegion({rectangle(1, 1, 4, 4), rectangle(4, 4, 6, 6)}),
        CoveragePlanningError::NoGoOverlapOrTouch);
    add("invalid-O", nestedRegion({{{{0, 0}, {5, 5}, {0, 5}, {5, 0}}}}), CoveragePlanningError::InvalidNoGoRegion);
    add("empty-T", nestedRegion({rectangle(0, 0, 10, 10)}), CoveragePlanningError::EmptyCoverageTarget);
    add("zero-area-T", nestedRegion({rectangle(-1, -1, 10, 11)}), CoveragePlanningError::EmptyCoverageTarget);
}

void CoverageGeometryTest::_testInvalidTopology()
{
    QFETCH(Region2D, input);
    QFETCH(int, error);
    const auto result = buildCoverageGeometry(input);
    QCOMPARE(result.error, static_cast<CoveragePlanningError>(error));
    QCOMPARE(CoverageProblemValidator::statusForError(result.error), PlanningStatus::InvalidInput);
    QVERIFY(result.geometry.coverageTarget.empty());
    QVERIFY(result.geometry.rawNavigationFreeSpace.empty());
}

void CoverageGeometryTest::_testNavigationOutsideCoverage()
{
    const auto result = buildCoverageGeometry(nestedRegion({rectangle(12, 2, 14, 4)}));
    QCOMPARE(result.error, CoveragePlanningError::None);
    QVERIFY(Geometry::pointInsidePolygonRegion(result.geometry.rawNavigationFreeSpace, {15, 3}));
    QVERIFY(!Geometry::pointInsidePolygonRegion(result.geometry.coverageTarget, {15, 3}));
    QVERIFY(!Geometry::pointInsidePolygonRegion(result.geometry.rawNavigationFreeSpace, {13, 3}));
    QVERIFY(Geometry::pointInsidePolygonRegion(result.geometry.coverageTarget, {5, 5}));
}

void CoverageGeometryTest::_testHistoricalPlannerBoundary()
{
    CoveragePlanningProblem problem;
    problem.executionSafety.executionMarginM = 0.0;
    problem.region = nestedRegion();
    problem.swathWidthM = 4.0;
    problem.safety.hardSafetyMarginM = 100.0;
    problem.safety.preferredSafetyMarginM = problem.safety.hardSafetyMarginM;
    problem.executionSafety.executionMarginM = 100.0;
    QCOMPARE(CoverageProblemValidator::validateAndNormalize(problem), CoveragePlanningError::None);
    QCOMPARE(buildCoverageGeometry(problem.region).error, CoveragePlanningError::None);
    for (const auto result : {MockCoveragePlanner{}.plan(problem), LawnmowerCoveragePlanner{}.plan(problem),
                              BoustrophedonCoveragePlanner{}.plan(problem)}) {
        QCOMPARE(result.error, CoveragePlanningError::UnsupportedSeparateBoundaries);
        QCOMPARE(result.status, PlanningStatus::Failed);
        QVERIFY(result.path.empty());
    }
}

UT_REGISTER_TEST_LIGHTWEIGHT(CoverageGeometryTest, TestLabel::Unit)
