#include "MonotoneCoverageTest.h"

#include <cmath>

#include "Geometry/MarineGeometry.h"
#include "Planning/MonotoneCoverage.h"

using namespace Marine;

namespace {

Polygon2D rectangle(double widthM, double heightM)
{
    return {.vertices = {{0.0, 0.0}, {widthM, 0.0}, {widthM, heightM}, {0.0, heightM}}};
}

Polygon2D insetRectangle(double widthM, double heightM, double marginM)
{
    return {.vertices = {{marginM, marginM},
                         {widthM - marginM, marginM},
                         {widthM - marginM, heightM - marginM},
                         {marginM, heightM - marginM}}};
}

void comparePoint(const Point2D& actual, double expectedXM, double expectedYM)
{
    QVERIFY(std::abs(actual.xM - expectedXM) <= Geometry::LengthEpsilonM);
    QVERIFY(std::abs(actual.yM - expectedYM) <= Geometry::LengthEpsilonM);
}

}  // namespace

void MonotoneCoverageTest::_testFixedAngleCoverageAndRoles()
{
    const Polygon2D target = rectangle(20.0, 10.0);
    const Polygon2D navigable = insetRectangle(20.0, 10.0, 1.0);
    const std::vector<double> lanePositionsYM = {2.0, 5.0, 8.0};
    const MonotoneCoverageResult result = generateMonotoneCoverage(target, navigable, 4.0, 90.0, lanePositionsYM);

    QCOMPARE(result.status, PlanningStatus::Success);
    QCOMPARE(result.error, MonotoneCoverageError::None);
    QCOMPARE(result.path.size(), std::size_t{6});
    QCOMPARE(result.legRoles.size(), result.path.size() - 1);
    QCOMPARE(result.legRoles[0], PathLegRole::Coverage);
    QCOMPARE(result.legRoles[1], PathLegRole::Transit);
    QCOMPARE(result.legRoles[2], PathLegRole::Coverage);
    QCOMPARE(result.legRoles[3], PathLegRole::Transit);
    QCOMPARE(result.legRoles[4], PathLegRole::Coverage);
    comparePoint(result.path[0], 1.0, 2.0);
    comparePoint(result.path[1], 19.0, 2.0);
    comparePoint(result.path[2], 19.0, 5.0);
    comparePoint(result.path[3], 1.0, 5.0);
    comparePoint(result.path[4], 1.0, 8.0);
    comparePoint(result.path[5], 19.0, 8.0);
    QCOMPARE(result.lanes.size(), std::size_t{3});
    QCOMPARE(result.lanes[0].startIndex, std::size_t{0});
    QCOMPARE(result.lanes[0].endIndex, std::size_t{1});
    QCOMPARE(result.lanes[1].startIndex, std::size_t{2});
    QCOMPARE(result.lanes[1].endIndex, std::size_t{3});
    QCOMPARE(result.lanes[2].startIndex, std::size_t{4});
    QCOMPARE(result.lanes[2].endIndex, std::size_t{5});
    QCOMPARE(result.laneCount, 3);
    QCOMPARE(result.turnCount, 2);
    QCOMPARE(result.laneSpacingM, 3.0);
    QVERIFY(std::abs(result.coverageLengthM - 54.0) <= Geometry::LengthEpsilonM);
    QVERIFY(std::abs(result.transitLengthM - 6.0) <= Geometry::LengthEpsilonM);
    QVERIFY(std::abs(result.pathLengthM - 60.0) <= Geometry::LengthEpsilonM);
}

void MonotoneCoverageTest::_testSingleLaneCoverage()
{
    const Polygon2D polygon = rectangle(12.0, 3.0);
    const std::vector<double> lanePositionsYM = {1.5};
    const MonotoneCoverageResult result = generateMonotoneCoverage(polygon, polygon, 5.0, 90.0, lanePositionsYM);

    QCOMPARE(result.status, PlanningStatus::Success);
    QCOMPARE(result.path.size(), std::size_t{2});
    QCOMPARE(result.legRoles, std::vector<PathLegRole>{PathLegRole::Coverage});
    QCOMPARE(result.laneCount, 1);
    QCOMPARE(result.turnCount, 0);
    QCOMPARE(result.laneSpacingM, 0.0);
    QCOMPARE(result.coverageLengthM, 12.0);
    QCOMPARE(result.transitLengthM, 0.0);
}

void MonotoneCoverageTest::_testConvenienceSchedule()
{
    const Polygon2D polygon = rectangle(20.0, 10.0);
    const MonotoneCoverageResult result = generateMonotoneCoverage(polygon, polygon, 4.0, 90.0);

    QCOMPARE(result.status, PlanningStatus::Success);
    QCOMPARE(result.path.size(), std::size_t{6});
    QCOMPARE(result.legRoles.size(), std::size_t{5});
    QCOMPARE(result.laneCount, 3);
    QCOMPARE(result.turnCount, 2);
}

void MonotoneCoverageTest::_testUnsafeConnector()
{
    const Polygon2D polygon = {
        .vertices = {{0.0, 0.0}, {10.0, 0.0}, {10.0, 1.5}, {3.0, 3.0}, {10.0, 4.5}, {10.0, 10.0}, {0.0, 10.0}},
    };
    const std::vector<double> lanePositionsYM = {2.0, 5.0, 8.0};
    const MonotoneCoverageResult result = generateMonotoneCoverage(polygon, polygon, 4.0, 90.0, lanePositionsYM);

    QCOMPARE(result.status, PlanningStatus::Failed);
    QCOMPARE(result.error, MonotoneCoverageError::UnsafeConnector);
    QVERIFY(result.path.empty());
    QVERIFY(result.legRoles.empty());
}

void MonotoneCoverageTest::_testInvalidLaneSchedule()
{
    const Polygon2D polygon = rectangle(20.0, 10.0);
    const std::vector<double> lanePositionsYM = {2.0, 7.0, 6.0};
    const MonotoneCoverageResult result = generateMonotoneCoverage(polygon, polygon, 4.0, 90.0, lanePositionsYM);

    QCOMPARE(result.status, PlanningStatus::InvalidInput);
    QCOMPARE(result.error, MonotoneCoverageError::InvalidLaneSchedule);
    QVERIFY(result.path.empty());
    QVERIFY(result.legRoles.empty());
}

void MonotoneCoverageTest::_testInvalidTarget()
{
    const Polygon2D navigable = rectangle(20.0, 10.0);
    const MonotoneCoverageResult result = generateMonotoneCoverage({}, navigable, 4.0, 90.0);

    QCOMPARE(result.status, PlanningStatus::InvalidInput);
    QCOMPARE(result.error, MonotoneCoverageError::InvalidTargetPolygon);
    QVERIFY(result.path.empty());
    QVERIFY(result.legRoles.empty());
}

UT_REGISTER_TEST_LIGHTWEIGHT(MonotoneCoverageTest, TestLabel::Unit)
