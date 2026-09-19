#include "GreedyCellOrderingTest.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <set>
#include <utility>

#include "Geometry/MarineGeometry.h"
#include "Planning/GreedyCellOrdering.h"

using namespace Marine;

namespace {

Point2D point(double xM, double yM)
{
    return {.xM = xM, .yM = yM};
}

Polygon2D rectangle(double x0, double y0, double x1, double y1)
{
    return {.vertices = {point(x0, y0), point(x1, y0), point(x1, y1), point(x0, y1)}};
}

PolygonRegionSet2D regionSet(Polygon2D outer, std::vector<Polygon2D> holes = {})
{
    return {{.outerBoundary = std::move(outer), .holes = std::move(holes)}};
}

CellCoverage cell(CoverageCellId cellId)
{
    return {.cellId = cellId};
}

void appendStates(std::vector<CellTraversalState>& states, CoverageCellId cellId, const Point2D& forwardEntry,
                  const Point2D& forwardExit)
{
    states.push_back({.cellId = cellId,
                      .orientation = CellTraversalOrientation::Forward,
                      .entry = forwardEntry,
                      .exit = forwardExit});
    states.push_back({.cellId = cellId,
                      .orientation = CellTraversalOrientation::Reverse,
                      .entry = forwardExit,
                      .exit = forwardEntry});
}

bool pointsEqual(const Point2D& first, const Point2D& second)
{
    return (first.xM == second.xM) && (first.yM == second.yM);
}

void verifySuccessfulOrdering(const PolygonRegionSet2D& regions, std::span<const CellCoverage> cells,
                              const CellOrderingResult& result)
{
    QVERIFY2(result.status == PlanningStatus::Success, result.message.c_str());
    QCOMPARE(result.error, CoveragePlanningError::None);
    QCOMPARE(result.visits.size(), cells.size());
    QVERIFY(std::isfinite(result.totalTransitLengthM));
    QVERIFY(result.totalTransitLengthM >= 0.0);

    std::set<CoverageCellId> expected;
    for (const CellCoverage& coverage : cells) {
        expected.insert(coverage.cellId);
    }
    std::set<CoverageCellId> visited;
    double transitLengthM = 0.0;
    for (std::size_t index = 0; index < result.visits.size(); ++index) {
        const OrderedCellTraversal& visit = result.visits.at(index);
        QVERIFY(visited.insert(visit.state.cellId).second);
        if (index == 0) {
            QVERIFY(!visit.transitFromPrevious.has_value());
            continue;
        }

        QVERIFY(visit.transitFromPrevious.has_value());
        const StaticRoute& transit = *visit.transitFromPrevious;
        QCOMPARE(transit.status, PlanningStatus::Success);
        QCOMPARE(transit.error, CoveragePlanningError::None);
        QVERIFY(transit.path.size() >= 2);
        QVERIFY(pointsEqual(transit.path.front(), result.visits.at(index - 1).state.exit));
        QVERIFY(pointsEqual(transit.path.back(), visit.state.entry));
        for (std::size_t pointIndex = 1; pointIndex < transit.path.size(); ++pointIndex) {
            QVERIFY(Geometry::segmentInsidePolygonRegion(regions, transit.path.at(pointIndex - 1),
                                                         transit.path.at(pointIndex)));
        }
        transitLengthM += transit.lengthM;
    }
    QVERIFY(visited == expected);
    QCOMPARE(result.totalTransitLengthM, transitLengthM);
}

void compareOrderings(const CellOrderingResult& first, const CellOrderingResult& second)
{
    QCOMPARE(first.status, second.status);
    QCOMPARE(first.error, second.error);
    QCOMPARE(first.totalTransitLengthM, second.totalTransitLengthM);
    QCOMPARE(first.visits.size(), second.visits.size());
    for (std::size_t index = 0; index < first.visits.size(); ++index) {
        const OrderedCellTraversal& firstVisit = first.visits.at(index);
        const OrderedCellTraversal& secondVisit = second.visits.at(index);
        QCOMPARE(firstVisit.state.cellId, secondVisit.state.cellId);
        QCOMPARE(firstVisit.state.orientation, secondVisit.state.orientation);
        QCOMPARE(firstVisit.state.entry.xM, secondVisit.state.entry.xM);
        QCOMPARE(firstVisit.state.entry.yM, secondVisit.state.entry.yM);
        QCOMPARE(firstVisit.state.exit.xM, secondVisit.state.exit.xM);
        QCOMPARE(firstVisit.state.exit.yM, secondVisit.state.exit.yM);
        QCOMPARE(firstVisit.transitFromPrevious.has_value(), secondVisit.transitFromPrevious.has_value());
        if (!firstVisit.transitFromPrevious.has_value()) {
            continue;
        }
        const StaticRoute& firstTransit = *firstVisit.transitFromPrevious;
        const StaticRoute& secondTransit = *secondVisit.transitFromPrevious;
        QCOMPARE(firstTransit.lengthM, secondTransit.lengthM);
        QCOMPARE(firstTransit.path.size(), secondTransit.path.size());
        for (std::size_t pointIndex = 0; pointIndex < firstTransit.path.size(); ++pointIndex) {
            QCOMPARE(firstTransit.path.at(pointIndex).xM, secondTransit.path.at(pointIndex).xM);
            QCOMPARE(firstTransit.path.at(pointIndex).yM, secondTransit.path.at(pointIndex).yM);
        }
    }
}

}  // namespace

void GreedyCellOrderingTest::_testSingleCell()
{
    const PolygonRegionSet2D regions = regionSet(rectangle(0.0, 0.0, 20.0, 20.0));
    const std::vector<CellCoverage> cells{cell(7)};
    std::vector<CellTraversalState> states;
    appendStates(states, 7, point(2.0, 2.0), point(4.0, 2.0));
    std::ranges::reverse(states);

    const CellOrderingResult result = orderCellTraversals(regions, cells, states);

    verifySuccessfulOrdering(regions, cells, result);
    QCOMPARE(result.visits.front().state.cellId, CoverageCellId{7});
    QCOMPARE(result.visits.front().state.orientation, CellTraversalOrientation::Forward);
    QCOMPARE(result.totalTransitLengthM, 0.0);
}

void GreedyCellOrderingTest::_testTwoCellsDirectTransit()
{
    const PolygonRegionSet2D regions = regionSet(rectangle(0.0, 0.0, 20.0, 20.0));
    const std::vector<CellCoverage> cells{cell(1), cell(0)};
    std::vector<CellTraversalState> states;
    appendStates(states, 1, point(8.0, 2.0), point(9.0, 2.0));
    appendStates(states, 0, point(1.0, 2.0), point(2.0, 2.0));

    const CellOrderingResult result = orderCellTraversals(regions, cells, states);

    verifySuccessfulOrdering(regions, cells, result);
    QCOMPARE(result.visits.at(0).state.cellId, CoverageCellId{0});
    QCOMPARE(result.visits.at(1).state.cellId, CoverageCellId{1});
    QCOMPARE(result.visits.at(1).state.orientation, CellTraversalOrientation::Forward);
    QCOMPARE(result.visits.at(1).transitFromPrevious.value().path.size(), std::size_t{2});
    QCOMPARE(result.totalTransitLengthM, 6.0);
}

void GreedyCellOrderingTest::_testOrientationAffectsTransitDistance()
{
    const PolygonRegionSet2D regions = regionSet(rectangle(0.0, 0.0, 20.0, 20.0));
    const std::vector<CellCoverage> cells{cell(0), cell(1)};
    std::vector<CellTraversalState> states;
    appendStates(states, 0, point(1.0, 2.0), point(2.0, 2.0));
    appendStates(states, 1, point(18.0, 2.0), point(6.0, 2.0));

    const CellOrderingResult result = orderCellTraversals(regions, cells, states);

    verifySuccessfulOrdering(regions, cells, result);
    QCOMPARE(result.visits.at(1).state.orientation, CellTraversalOrientation::Reverse);
    QCOMPARE(result.totalTransitLengthM, 4.0);
}

void GreedyCellOrderingTest::_testGreedyNearestSelection()
{
    const PolygonRegionSet2D regions = regionSet(rectangle(0.0, 0.0, 20.0, 20.0));
    const std::vector<CellCoverage> cells{cell(3), cell(1), cell(0), cell(2)};
    std::vector<CellTraversalState> states;
    appendStates(states, 3, point(9.0, 2.0), point(9.0, 2.0));
    appendStates(states, 1, point(18.0, 2.0), point(18.0, 2.0));
    appendStates(states, 0, point(1.0, 2.0), point(1.0, 2.0));
    appendStates(states, 2, point(4.0, 2.0), point(4.0, 2.0));

    const CellOrderingResult result = orderCellTraversals(regions, cells, states);

    verifySuccessfulOrdering(regions, cells, result);
    const std::vector<CoverageCellId> expected{0, 2, 3, 1};
    for (std::size_t index = 0; index < expected.size(); ++index) {
        QCOMPARE(result.visits.at(index).state.cellId, expected.at(index));
        QCOMPARE(result.visits.at(index).state.orientation, CellTraversalOrientation::Forward);
    }
}

void GreedyCellOrderingTest::_testNoGoDetour()
{
    const PolygonRegionSet2D regions = regionSet(rectangle(0.0, 0.0, 20.0, 20.0), {rectangle(8.0, 8.0, 12.0, 12.0)});
    const std::vector<CellCoverage> cells{cell(0), cell(1)};
    std::vector<CellTraversalState> states;
    appendStates(states, 0, point(1.0, 10.0), point(2.0, 10.0));
    appendStates(states, 1, point(18.0, 10.0), point(19.0, 10.0));

    const CellOrderingResult result = orderCellTraversals(regions, cells, states);

    verifySuccessfulOrdering(regions, cells, result);
    QCOMPARE(result.visits.at(1).state.orientation, CellTraversalOrientation::Forward);
    QVERIFY(result.visits.at(1).transitFromPrevious.value().path.size() > 2);
    QVERIFY(result.totalTransitLengthM > 16.0);
}

void GreedyCellOrderingTest::_testForwardUnreachableReverseReachable()
{
    const PolygonRegionSet2D regions = {
        {.outerBoundary = rectangle(0.0, 0.0, 4.0, 4.0)},
        {.outerBoundary = rectangle(10.0, 0.0, 14.0, 4.0)},
    };
    const std::vector<CellCoverage> cells{cell(0), cell(1)};
    std::vector<CellTraversalState> states;
    appendStates(states, 0, point(1.0, 2.0), point(2.0, 2.0));
    appendStates(states, 1, point(12.0, 2.0), point(3.0, 2.0));

    const CellOrderingResult result = orderCellTraversals(regions, cells, states);

    verifySuccessfulOrdering(regions, cells, result);
    QCOMPARE(result.visits.at(1).state.orientation, CellTraversalOrientation::Reverse);
    QCOMPARE(result.totalTransitLengthM, 1.0);
}

void GreedyCellOrderingTest::_testRemainingCellUnreachable()
{
    const PolygonRegionSet2D regions = {
        {.outerBoundary = rectangle(0.0, 0.0, 4.0, 4.0)},
        {.outerBoundary = rectangle(10.0, 0.0, 14.0, 4.0)},
    };
    const std::vector<CellCoverage> cells{cell(0), cell(1)};
    std::vector<CellTraversalState> states;
    appendStates(states, 0, point(1.0, 2.0), point(2.0, 2.0));
    appendStates(states, 1, point(12.0, 2.0), point(13.0, 2.0));

    const CellOrderingResult result = orderCellTraversals(regions, cells, states);

    QCOMPARE(result.status, PlanningStatus::Failed);
    QCOMPARE(result.error, CoveragePlanningError::SafeTransitNotFound);
    QVERIFY(result.visits.empty());
    QCOMPARE(result.totalTransitLengthM, 0.0);
}

void GreedyCellOrderingTest::_testEqualDistanceTieBreak()
{
    const PolygonRegionSet2D regions = regionSet(rectangle(0.0, 0.0, 20.0, 20.0));
    const std::vector<CellCoverage> cells{cell(2), cell(0), cell(1)};
    std::vector<CellTraversalState> states;
    appendStates(states, 2, point(15.0, 10.0), point(15.0, 10.0));
    appendStates(states, 0, point(10.0, 10.0), point(10.0, 10.0));
    appendStates(states, 1, point(5.0, 10.0), point(5.0, 10.0));

    const CellOrderingResult result = orderCellTraversals(regions, cells, states);

    verifySuccessfulOrdering(regions, cells, result);
    QCOMPARE(result.visits.at(1).state.cellId, CoverageCellId{1});
    QCOMPARE(result.visits.at(1).state.orientation, CellTraversalOrientation::Forward);
}

void GreedyCellOrderingTest::_testInputOrderingIndependence()
{
    Polygon2D outer = rectangle(0.0, 0.0, 20.0, 20.0);
    Polygon2D hole = rectangle(8.0, 8.0, 12.0, 12.0);
    const PolygonRegionSet2D canonical = regionSet(outer, {hole});
    std::vector<CellCoverage> cells{cell(0), cell(1), cell(2)};
    std::vector<CellTraversalState> states;
    appendStates(states, 0, point(1.0, 10.0), point(2.0, 10.0));
    appendStates(states, 1, point(18.0, 10.0), point(19.0, 10.0));
    appendStates(states, 2, point(3.0, 18.0), point(4.0, 18.0));
    const CellOrderingResult first = orderCellTraversals(canonical, cells, states);

    std::ranges::reverse(cells);
    std::ranges::reverse(states);
    std::ranges::reverse(outer.vertices);
    std::ranges::reverse(hole.vertices);
    std::ranges::rotate(hole.vertices, std::next(hole.vertices.begin()));
    const PolygonRegionSet2D reorderedRegions = regionSet(outer, {hole});
    const CellOrderingResult reordered = orderCellTraversals(reorderedRegions, cells, states);

    verifySuccessfulOrdering(canonical, cells, first);
    verifySuccessfulOrdering(reorderedRegions, cells, reordered);
    compareOrderings(first, reordered);
}

void GreedyCellOrderingTest::_testRepeatedRunsDeterministic()
{
    const PolygonRegionSet2D regions = regionSet(rectangle(0.0, 0.0, 20.0, 20.0), {rectangle(8.0, 8.0, 12.0, 12.0)});
    const std::vector<CellCoverage> cells{cell(2), cell(0), cell(1)};
    std::vector<CellTraversalState> states;
    appendStates(states, 2, point(4.0, 18.0), point(3.0, 18.0));
    appendStates(states, 0, point(1.0, 10.0), point(2.0, 10.0));
    appendStates(states, 1, point(18.0, 10.0), point(19.0, 10.0));
    const CellOrderingResult first = orderCellTraversals(regions, cells, states);

    verifySuccessfulOrdering(regions, cells, first);
    for (int repetition = 0; repetition < 10; ++repetition) {
        compareOrderings(first, orderCellTraversals(regions, cells, states));
    }
}

UT_REGISTER_TEST_LIGHTWEIGHT(GreedyCellOrderingTest, TestLabel::Unit)
