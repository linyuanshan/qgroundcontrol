#include "BoustrophedonDecompositionTest.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "Geometry/MarineGeometry.h"
#include "Geometry/PolygonRegion.h"
#include "Planning/BoustrophedonDecomposition.h"
#include "Planning/CoverageFreeSpace.h"

using namespace Marine;

namespace {

Polygon2D polygon(std::initializer_list<Point2D> vertices)
{
    return {.vertices = vertices};
}

Polygon2D rectangle(double x0, double y0, double x1, double y1)
{
    return polygon({{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}});
}

PolygonRegion2D region(Polygon2D outer, std::vector<Polygon2D> holes = {})
{
    return {.outerBoundary = std::move(outer), .holes = std::move(holes)};
}

double area(const Polygon2D& value)
{
    double result = 0.0;
    for (std::size_t i = 0; i < value.vertices.size(); ++i) {
        const Point2D& a = value.vertices[i];
        const Point2D& b = value.vertices[(i + 1) % value.vertices.size()];
        result += a.xM * b.yM - b.xM * a.yM;
    }
    return std::abs(result) * 0.5;
}

double area(const PolygonRegion2D& value)
{
    double result = area(value.outerBoundary);
    for (const Polygon2D& hole : value.holes) {
        result -= area(hole);
    }
    return result;
}

double area(const PolygonRegionSet2D& value)
{
    double result = 0.0;
    for (const PolygonRegion2D& regionValue : value) {
        result += area(regionValue);
    }
    return result;
}

CoverageDecompositionResult decompose(const PolygonRegion2D& value, double angle = 90.0)
{
    return decomposeBoustrophedon({value}, angle);
}

void compareResults(const CoverageDecompositionResult& first, const CoverageDecompositionResult& second)
{
    QCOMPARE(first.status, second.status);
    QCOMPARE(first.error, second.error);
    QCOMPARE(first.cells.size(), second.cells.size());
    QCOMPARE(first.adjacency, second.adjacency);
    for (std::size_t i = 0; i < first.cells.size(); ++i) {
        QCOMPARE(first.cells[i].id, second.cells[i].id);
        QCOMPARE(first.cells[i].polygon.vertices.size(), second.cells[i].polygon.vertices.size());
        for (std::size_t j = 0; j < first.cells[i].polygon.vertices.size(); ++j) {
            QCOMPARE(first.cells[i].polygon.vertices[j].xM, second.cells[i].polygon.vertices[j].xM);
            QCOMPARE(first.cells[i].polygon.vertices[j].yM, second.cells[i].polygon.vertices[j].yM);
        }
    }
}

void verifyCellInvariants(const CoverageDecompositionResult& result, const PolygonRegion2D& input, double angle)
{
    QVERIFY2(result.status == PlanningStatus::Success, result.message.c_str());
    QCOMPARE(result.error, CoveragePlanningError::None);
    for (std::size_t i = 0; i < result.cells.size(); ++i) {
        QCOMPARE(result.cells[i].id, static_cast<CoverageCellId>(i));
        QVERIFY(result.cells[i].polygon.isFinite());
        QVERIFY(result.cells[i].polygon.vertices.size() >= 3);
        QVERIFY(Geometry::isValidPolygonRegion(region(result.cells[i].polygon)));
        QVERIFY(Geometry::isMonotoneCellPolygon(result.cells[i].polygon, Geometry::navigationAngleToMathAngle(angle)));
        QVERIFY(area(result.cells[i].polygon) > 1e-6);
    }
    const auto unionResult = Geometry::unionPolygonRegions([&result] {
        PolygonRegionSet2D regions;
        for (const CoverageCell& cell : result.cells) {
            regions.push_back(region(cell.polygon));
        }
        return regions;
    }());
    QCOMPARE(unionResult.status, Geometry::PolygonRegionOperationStatus::Success);
    double perimeter = 0.0;
    const auto addPerimeter = [&perimeter](const Polygon2D& boundary) {
        for (std::size_t index = 0; index < boundary.vertices.size(); ++index) {
            const auto& a = boundary.vertices[index];
            const auto& b = boundary.vertices[(index + 1) % boundary.vertices.size()];
            perimeter += std::hypot(a.xM - b.xM, a.yM - b.yM);
        }
    };
    addPerimeter(input.outerBoundary);
    for (const auto& hole : input.holes) {
        addPerimeter(hole);
    }
    const double areaToleranceM2 = perimeter * Geometry::LengthEpsilonM * 2.0;
    QVERIFY(std::abs(area(unionResult.regions) - area(input)) <= areaToleranceM2);
    const auto inputAsSet = PolygonRegionSet2D{input};
    const auto unionBuffer = Geometry::bufferPolygonRegions(unionResult.regions, 2.0 * Geometry::LengthEpsilonM);
    const auto inputBuffer = Geometry::bufferPolygonRegions(inputAsSet, 2.0 * Geometry::LengthEpsilonM);
    QCOMPARE(unionBuffer.status, Geometry::PolygonRegionOperationStatus::Success);
    QCOMPARE(inputBuffer.status, Geometry::PolygonRegionOperationStatus::Success);
    QVERIFY(Geometry::isRegionSetContained(inputAsSet, unionBuffer.regions).contained);
    QVERIFY(Geometry::isRegionSetContained(unionResult.regions, inputBuffer.regions).contained);
    QVERIFY(std::abs(area(unionResult.regions) - [&result] {
                double sum = 0.0;
                for (const CoverageCell& cell : result.cells) {
                    sum += area(cell.polygon);
                }
                return sum;
            }()) <= areaToleranceM2);

    // Round-trip to the backend sweep lattice: an overlap cannot hide in ENU rotation tolerance.
    PolygonRegionSet2D sweepCells;
    double sumSweepArea = 0.0;
    for (const auto& cell : result.cells) {
        Polygon2D sweep = Geometry::toSweepFrame(cell.polygon, Geometry::navigationAngleToMathAngle(angle));
        for (auto& vertex : sweep.vertices) {
            vertex.xM = std::round(vertex.xM * Geometry::CoordinateScalePerM) / Geometry::CoordinateScalePerM;
            vertex.yM = std::round(vertex.yM * Geometry::CoordinateScalePerM) / Geometry::CoordinateScalePerM;
        }
        std::vector<double> eventY;
        for (const auto& vertex : sweep.vertices) {
            eventY.push_back(vertex.yM);
        }
        std::sort(eventY.begin(), eventY.end());
        eventY.erase(std::unique(eventY.begin(), eventY.end()), eventY.end());
        for (std::size_t index = 0; index + 1 < eventY.size(); ++index) {
            const double probeY = (eventY[index] + eventY[index + 1]) / 2.0;
            std::size_t crossings = 0;
            for (std::size_t edge = 0; edge < sweep.vertices.size(); ++edge) {
                const auto& a = sweep.vertices[edge];
                const auto& b = sweep.vertices[(edge + 1) % sweep.vertices.size()];
                if ((a.yM > probeY) != (b.yM > probeY)) {
                    ++crossings;
                }
            }
            QCOMPARE(crossings, std::size_t{2});
        }
        sumSweepArea += area(sweep);
        sweepCells.push_back(region(std::move(sweep)));
    }
    const auto sweepUnion = Geometry::unionPolygonRegions(sweepCells);
    QCOMPARE(sweepUnion.status, Geometry::PolygonRegionOperationStatus::Success);
    QVERIFY(std::abs(sumSweepArea - area(sweepUnion.regions)) < 1e-7);
}

}  // namespace

void BoustrophedonDecompositionTest::_testRectAndSimpleConcaveRegions()
{
    QCOMPARE(decompose(region(rectangle(0, 0, 10, 10))).cells.size(), std::size_t{1});
    const Polygon2D lShape = polygon({{0, 0}, {10, 0}, {10, 4}, {4, 4}, {4, 10}, {0, 10}});
    const auto result = decompose(region(lShape));
    QCOMPARE(result.cells.size(), std::size_t{1});
    QVERIFY(result.adjacency.empty());
    verifyCellInvariants(result, region(lShape), 90.0);
}

void BoustrophedonDecompositionTest::_testCShapeSplitsAndMerges()
{
    const Polygon2D cShape = polygon({{0, 0}, {10, 0}, {10, 3}, {3, 3}, {3, 7}, {10, 7}, {10, 10}, {0, 10}});
    QCOMPARE(decompose(region(cShape), 90.0).cells.size(), std::size_t{1});
    QCOMPARE(decompose(region(cShape), 0.0).cells.size(), std::size_t{3});
    const auto reverse = decompose(region(cShape), 180.0);
    QCOMPARE(reverse.cells.size(), std::size_t{3});
    verifyCellInvariants(reverse, region(cShape), 180.0);
}

void BoustrophedonDecompositionTest::_testHoleAndTwoHoleTopology()
{
    const Polygon2D outer = rectangle(0, 0, 10, 10);
    const Polygon2D hole = rectangle(4, 3, 6, 7);
    const auto oneHole = decompose(region(outer, {hole}));
    QCOMPARE(oneHole.cells.size(), std::size_t{4});
    QCOMPARE(oneHole.adjacency, (std::vector<CellAdjacency>{{0, 1}, {0, 2}, {1, 3}, {2, 3}}));
    verifyCellInvariants(oneHole, region(outer, {hole}), 90.0);

    const auto twoHoles = decompose(region(outer, {rectangle(2, 3, 3, 7), rectangle(7, 3, 8, 7)}));
    QCOMPARE(twoHoles.cells.size(), std::size_t{5});
    QCOMPARE(twoHoles.adjacency, (std::vector<CellAdjacency>{{0, 1}, {0, 2}, {0, 3}, {1, 4}, {2, 4}, {3, 4}}));
    verifyCellInvariants(twoHoles, region(outer, {rectangle(2, 3, 3, 7), rectangle(7, 3, 8, 7)}), 90.0);

    const Polygon2D diamond = polygon({{5, 2}, {7, 5}, {5, 8}, {3, 5}});
    const auto pointedHole = decompose(region(outer, {diamond}));
    QVERIFY2(pointedHole.status == PlanningStatus::Success, pointedHole.message.c_str());
    QCOMPARE(pointedHole.cells.size(), std::size_t{4});
    QCOMPARE(pointedHole.adjacency, oneHole.adjacency);
    verifyCellInvariants(pointedHole, region(outer, {diamond}), 90.0);
}

void BoustrophedonDecompositionTest::_testExplicitSplitAndMergeTopology()
{
    const Polygon2D splitU = polygon({{0, 0}, {10, 0}, {10, 10}, {7, 10}, {7, 3}, {3, 3}, {3, 10}, {0, 10}});
    const auto split = decompose(region(splitU), 90.0);
    QCOMPARE(split.cells.size(), std::size_t{3});
    QCOMPARE(split.adjacency, (std::vector<CellAdjacency>{{0, 1}, {0, 2}}));
    verifyCellInvariants(split, region(splitU), 90.0);

    const Polygon2D invertedU = polygon({{0, 0}, {3, 0}, {3, 7}, {7, 7}, {7, 0}, {10, 0}, {10, 10}, {0, 10}});
    const auto merge = decompose(region(invertedU), 90.0);
    QCOMPARE(merge.cells.size(), std::size_t{3});
    QCOMPARE(merge.adjacency, (std::vector<CellAdjacency>{{0, 2}, {1, 2}}));
    verifyCellInvariants(merge, region(invertedU), 90.0);
}

void BoustrophedonDecompositionTest::_testCollinearAndNearEqualEvents()
{
    const Polygon2D collinear = polygon({{0, 0}, {5, 0}, {10, 0}, {10, 5}, {5, 5}, {0, 5}});
    const auto result = decompose(region(collinear));
    QVERIFY(result.status == PlanningStatus::Success);
    QCOMPARE(result.cells.size(), std::size_t{1});
    const Polygon2D nearEqual = polygon({{0, 0}, {10, 0.0004}, {10, 5.0004}, {0, 5}});
    const auto merged = decompose(region(nearEqual));
    QVERIFY(merged.status == PlanningStatus::Success);
    QCOMPARE(merged.cells.size(), std::size_t{1});
    verifyCellInvariants(merged, region(nearEqual), 90.0);
    const auto nearHoleEvents =
        region(rectangle(0, 0, 10, 10), {rectangle(2, 3, 3, 7), rectangle(7, 3.0004, 8, 7.0004)});
    const auto mergedHoles = decompose(nearHoleEvents);
    QCOMPARE(mergedHoles.cells.size(), std::size_t{5});
    QCOMPARE(mergedHoles.adjacency.size(), std::size_t{6});
    verifyCellInvariants(mergedHoles, nearHoleEvents, 90.0);
}

void BoustrophedonDecompositionTest::_testDeterminismAndInputOrdering()
{
    const Polygon2D outer = rectangle(0.123, 0.456, 30.123, 20.456);
    const Polygon2D firstHole = rectangle(8.123, 8.456, 12.123, 12.456);
    const Polygon2D secondHole = rectangle(18.123, 8.456, 22.123, 12.456);
    auto first = decompose(region(outer, {firstHole, secondHole}));
    QCOMPARE(first.status, PlanningStatus::Success);
    auto reversedOuter = outer;
    std::reverse(reversedOuter.vertices.begin(), reversedOuter.vertices.end());
    auto reversedHole = secondHole;
    std::reverse(reversedHole.vertices.begin(), reversedHole.vertices.end());
    const auto second = decompose(region(reversedOuter, {reversedHole, firstHole}));
    compareResults(first, second);
    const auto repeat = decompose(region(outer, {firstHole, secondHole}));
    compareResults(first, repeat);
    auto cyclicOuter = outer;
    std::rotate(cyclicOuter.vertices.begin(), cyclicOuter.vertices.begin() + 1, cyclicOuter.vertices.end());
    compareResults(first, decompose(region(cyclicOuter, {firstHole, secondHole})));
}

void BoustrophedonDecompositionTest::_testNonCardinalSweep()
{
    const Polygon2D outer = rectangle(-10, -10, 10, 10);
    const auto result = decompose(region(outer), 37.0);
    QCOMPARE(result.cells.size(), std::size_t{1});
    verifyCellInvariants(result, region(outer), 37.0);
}

void BoustrophedonDecompositionTest::_testInvalidInputs()
{
    QVERIFY(decomposeBoustrophedon({}, 90.0).status == PlanningStatus::Failed);
    QVERIFY(decomposeBoustrophedon({region(rectangle(0, 0, 1, 1)), region(rectangle(2, 0, 3, 1))}, 90.0).error ==
            CoveragePlanningError::DisconnectedFeasibleRegion);
    QVERIFY(decompose(region(rectangle(0, 0, 10, 10)), std::numeric_limits<double>::quiet_NaN()).status ==
            PlanningStatus::InvalidInput);
    const Polygon2D nanPolygon = polygon({{0, 0}, {10, 0}, {10, 10}, {std::numeric_limits<double>::quiet_NaN(), 10}});
    QVERIFY(decompose(region(nanPolygon)).status == PlanningStatus::InvalidInput);
    const auto sliver = decompose(region(rectangle(0, 0, 10, 0.0005)));
    QVERIFY(sliver.status != PlanningStatus::Success);
    QVERIFY(sliver.cells.empty());
    QVERIFY(sliver.adjacency.empty());
    QVERIFY(!Geometry::isValidPolygonRegion(region(polygon({{0, 0}, {10, 10}, {0, 10}, {10, 0}}))));
    QVERIFY(!Geometry::isValidPolygonRegion(region(rectangle(0, 0, 10, 10), {rectangle(0, 2, 3, 4)})));
    QVERIFY(!Geometry::isValidPolygonRegion(
        region(rectangle(0, 0, 10, 10), {rectangle(2, 2, 8, 8), rectangle(3, 3, 4, 4)})));
    QVERIFY(!Geometry::shareSlabBoundary(rectangle(0, 0, 5, 5), rectangle(5, 5, 10, 10), 5.0));
    QVERIFY(Geometry::shareSlabBoundary(rectangle(0, 0, 6, 5), rectangle(5, 5, 10, 10), 5.0));
    QVERIFY(Geometry::shareSlabBoundary(rectangle(0, 0, 5.001, 5), rectangle(5, 5, 10, 10), 5.0));
}

void BoustrophedonDecompositionTest::_testOutputInvariants()
{
    const Polygon2D outer = rectangle(0, 0, 20, 20);
    const auto result = decompose(region(outer, {rectangle(8, 8, 12, 12)}));
    verifyCellInvariants(result, region(outer, {rectangle(8, 8, 12, 12)}), 90.0);
    for (const CellAdjacency& edge : result.adjacency) {
        QVERIFY(edge.first < result.cells.size());
        QVERIFY(edge.second < result.cells.size());
        QVERIFY(edge.first != edge.second);
    }
}

void BoustrophedonDecompositionTest::_testRoundedHoleEndToEnd()
{
    CoveragePlanningProblem problem;
    problem.region.outerBoundary = rectangle(0, 0, 30, 20);
    problem.region.noGoRegions = {rectangle(12, 7, 18, 13)};
    problem.swathWidthM = 4.0;
    problem.safetyMarginM = 1.0;
    problem.sweepAngleMode = SweepAngleMode::Manual;
    problem.requestedSweepAngleDeg = 90.0;
    const CoverageFreeSpaceResult freeSpace = buildCoverageFreeSpace(problem);
    QCOMPARE(freeSpace.status, PlanningStatus::Success);
    QVERIFY(!freeSpace.freeSpace.trackFeasibleRegion.empty());
    const auto result = decompose(freeSpace.freeSpace.trackFeasibleRegion.front(), 90.0);
    verifyCellInvariants(result, freeSpace.freeSpace.trackFeasibleRegion.front(), 90.0);
}

UT_REGISTER_TEST_LIGHTWEIGHT(BoustrophedonDecompositionTest, TestLabel::Unit)
