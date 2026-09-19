#include "ComplexCoverageAssemblyTest.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <utility>

#include "Geometry/MarineGeometry.h"
#include "Planning/BoustrophedonDecomposition.h"
#include "Planning/CellCoverage.h"
#include "Planning/ComplexCoverageAssembly.h"
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

double distance(const Point2D& first, const Point2D& second)
{
    return std::hypot(second.xM - first.xM, second.yM - first.yM);
}

bool pointsEqual(const Point2D& first, const Point2D& second)
{
    return (first.xM == second.xM) && (first.yM == second.yM);
}

CellCoverage coverage(CoverageCellId cellId, std::vector<Point2D> path, std::vector<PathLegRole> roles)
{
    CellCoverage result;
    result.cellId = cellId;
    result.path = std::move(path);
    result.legRoles = std::move(roles);
    for (std::size_t index = 1; index < result.path.size(); ++index) {
        const double lengthM = distance(result.path.at(index - 1), result.path.at(index));
        result.pathLengthM += lengthM;
        if (result.legRoles.at(index - 1) == PathLegRole::Coverage) {
            result.coverageLengthM += lengthM;
        } else {
            result.transitLengthM += lengthM;
        }
    }
    return result;
}

CellTraversalState traversal(const CellCoverage& cell, CellTraversalOrientation orientation)
{
    const bool forward = orientation == CellTraversalOrientation::Forward;
    return {.cellId = cell.cellId,
            .orientation = orientation,
            .entry = forward ? cell.path.front() : cell.path.back(),
            .exit = forward ? cell.path.back() : cell.path.front()};
}

OrderedCellTraversal visit(const CellCoverage& cell, CellTraversalOrientation orientation,
                           std::optional<StaticRoute> transit = std::nullopt)
{
    return {.state = traversal(cell, orientation), .transitFromPrevious = std::move(transit)};
}

void verifySuccessfulAssembly(const PolygonRegionSet2D& regions, const ComplexCoverageAssemblyResult& result)
{
    QVERIFY2(result.status == PlanningStatus::Success, result.message.c_str());
    QCOMPARE(result.error, CoveragePlanningError::None);
    QVERIFY(result.path.size() >= 2);
    QCOMPARE(result.legRoles.size(), result.path.size() - 1);
    QVERIFY(result.cellCount > 0);
    QVERIFY(result.turnCount >= 0);

    double coverageLengthM = 0.0;
    double transitLengthM = 0.0;
    for (std::size_t index = 1; index < result.path.size(); ++index) {
        const Point2D& first = result.path.at(index - 1);
        const Point2D& second = result.path.at(index);
        QVERIFY(first.isFinite());
        QVERIFY(second.isFinite());
        QVERIFY(!pointsEqual(first, second));
        const double lengthM = distance(first, second);
        QVERIFY(lengthM > Geometry::LengthEpsilonM);
        QVERIFY(Geometry::segmentInsidePolygonRegion(regions, first, second));
        if (result.legRoles.at(index - 1) == PathLegRole::Coverage) {
            coverageLengthM += lengthM;
        } else {
            QCOMPARE(result.legRoles.at(index - 1), PathLegRole::Transit);
            transitLengthM += lengthM;
        }
    }
    QVERIFY(std::abs(result.coverageLengthM - coverageLengthM) <= Geometry::LengthEpsilonM);
    QVERIFY(std::abs(result.transitLengthM - transitLengthM) <= Geometry::LengthEpsilonM);
    QVERIFY(std::abs(result.pathLengthM - coverageLengthM - transitLengthM) <= Geometry::LengthEpsilonM);
}

void verifyAtomicFailure(const ComplexCoverageAssemblyResult& result)
{
    QCOMPARE(result.status, PlanningStatus::Failed);
    QCOMPARE(result.error, CoveragePlanningError::InvalidGeneratedPath);
    QVERIFY(result.path.empty());
    QVERIFY(result.legRoles.empty());
    QCOMPARE(result.coverageLengthM, 0.0);
    QCOMPARE(result.transitLengthM, 0.0);
    QCOMPARE(result.pathLengthM, 0.0);
    QCOMPARE(result.cellCount, 0);
    QCOMPARE(result.turnCount, 0);
}

void compareAssemblies(const ComplexCoverageAssemblyResult& first, const ComplexCoverageAssemblyResult& second)
{
    QCOMPARE(first.status, second.status);
    QCOMPARE(first.error, second.error);
    QCOMPARE(first.path.size(), second.path.size());
    QCOMPARE(first.legRoles, second.legRoles);
    QCOMPARE(first.coverageLengthM, second.coverageLengthM);
    QCOMPARE(first.transitLengthM, second.transitLengthM);
    QCOMPARE(first.pathLengthM, second.pathLengthM);
    QCOMPARE(first.cellCount, second.cellCount);
    QCOMPARE(first.turnCount, second.turnCount);
    for (std::size_t index = 0; index < first.path.size(); ++index) {
        QCOMPARE(first.path.at(index).xM, second.path.at(index).xM);
        QCOMPARE(first.path.at(index).yM, second.path.at(index).yM);
    }
}

void comparePaths(std::span<const Point2D> first, std::span<const Point2D> second)
{
    QCOMPARE(first.size(), second.size());
    auto secondIterator = second.begin();
    for (const Point2D& firstPoint : first) {
        QCOMPARE(firstPoint.xM, secondIterator->xM);
        QCOMPARE(firstPoint.yM, secondIterator->yM);
        ++secondIterator;
    }
}

}  // namespace

void ComplexCoverageAssemblyTest::_testSingleCellForwardAndReverse()
{
    const PolygonRegionSet2D regions = regionSet(rectangle(0.0, 0.0, 20.0, 20.0));
    const CellCoverage cell =
        coverage(4, {point(2.0, 2.0), point(8.0, 2.0), point(8.0, 5.0)}, {PathLegRole::Coverage, PathLegRole::Transit});
    const std::vector<CellCoverage> cells{cell};
    const std::vector<OrderedCellTraversal> forwardVisits{visit(cell, CellTraversalOrientation::Forward)};
    const std::vector<OrderedCellTraversal> reverseVisits{visit(cell, CellTraversalOrientation::Reverse)};

    const ComplexCoverageAssemblyResult forward = assembleComplexCoverage(regions, cells, forwardVisits);
    const ComplexCoverageAssemblyResult reverse = assembleComplexCoverage(regions, cells, reverseVisits);

    verifySuccessfulAssembly(regions, forward);
    verifySuccessfulAssembly(regions, reverse);
    comparePaths(forward.path, cell.path);
    QCOMPARE(forward.legRoles, cell.legRoles);
    QCOMPARE(reverse.path.front().xM, cell.path.back().xM);
    QCOMPARE(reverse.path.back().xM, cell.path.front().xM);
    QCOMPARE(reverse.legRoles.at(0), PathLegRole::Transit);
    QCOMPARE(reverse.legRoles.at(1), PathLegRole::Coverage);
    QCOMPARE(forward.coverageLengthM, reverse.coverageLengthM);
    QCOMPARE(forward.transitLengthM, reverse.transitLengthM);
}

void ComplexCoverageAssemblyTest::_testDirectAndReverseTransit()
{
    const PolygonRegionSet2D regions = regionSet(rectangle(0.0, 0.0, 20.0, 20.0));
    const CellCoverage first = coverage(0, {point(1.0, 1.0), point(3.0, 1.0)}, {PathLegRole::Coverage});
    const CellCoverage second = coverage(1, {point(7.0, 3.0), point(9.0, 3.0)}, {PathLegRole::Coverage});
    const std::vector<CellCoverage> cells{first, second};

    const StaticRoute direct = routeStatic(regions, first.path.back(), second.path.front());
    const std::vector<OrderedCellTraversal> forwardVisits = {
        visit(first, CellTraversalOrientation::Forward),
        visit(second, CellTraversalOrientation::Forward, direct),
    };
    const ComplexCoverageAssemblyResult forward = assembleComplexCoverage(regions, cells, forwardVisits);
    verifySuccessfulAssembly(regions, forward);
    QCOMPARE(forward.legRoles,
             std::vector<PathLegRole>({PathLegRole::Coverage, PathLegRole::Transit, PathLegRole::Coverage}));

    const StaticRoute toReverse = routeStatic(regions, first.path.back(), second.path.back());
    const std::vector<OrderedCellTraversal> reverseVisits = {
        visit(first, CellTraversalOrientation::Forward),
        visit(second, CellTraversalOrientation::Reverse, toReverse),
    };
    const ComplexCoverageAssemblyResult reverse = assembleComplexCoverage(regions, cells, reverseVisits);
    verifySuccessfulAssembly(regions, reverse);
    QVERIFY(pointsEqual(reverse.path.back(), second.path.front()));
}

void ComplexCoverageAssemblyTest::_testVisibilityGraphTransitAndRoles()
{
    const PolygonRegionSet2D regions = regionSet(rectangle(0.0, 0.0, 20.0, 20.0), {rectangle(8.0, 8.0, 12.0, 12.0)});
    const CellCoverage first = coverage(0, {point(1.0, 10.0), point(2.0, 10.0)}, {PathLegRole::Coverage});
    const CellCoverage second = coverage(1, {point(18.0, 10.0), point(19.0, 10.0)}, {PathLegRole::Coverage});
    const StaticRoute transit = routeStatic(regions, first.path.back(), second.path.front());
    QVERIFY(transit.path.size() > 2);
    const std::vector<CellCoverage> cells{first, second};
    const std::vector<OrderedCellTraversal> visits = {
        visit(first, CellTraversalOrientation::Forward),
        visit(second, CellTraversalOrientation::Forward, transit),
    };

    const ComplexCoverageAssemblyResult result = assembleComplexCoverage(regions, cells, visits);

    verifySuccessfulAssembly(regions, result);
    QCOMPARE(result.legRoles.front(), PathLegRole::Coverage);
    QCOMPARE(result.legRoles.back(), PathLegRole::Coverage);
    for (std::size_t index = 1; index + 1 < result.legRoles.size(); ++index) {
        QCOMPARE(result.legRoles.at(index), PathLegRole::Transit);
    }
    QCOMPARE(result.transitLengthM, transit.lengthM);
}

void ComplexCoverageAssemblyTest::_testSharedEndpointsMetricsAndTurns()
{
    const PolygonRegionSet2D regions = regionSet(rectangle(0.0, 0.0, 20.0, 20.0));
    const CellCoverage first = coverage(0, {point(1.0, 1.0), point(3.0, 1.0)}, {PathLegRole::Coverage});
    const CellCoverage second =
        coverage(1, {point(3.0, 1.0), point(3.0, 3.0), point(5.0, 3.0)}, {PathLegRole::Transit, PathLegRole::Coverage});
    const StaticRoute zeroTransit = routeStatic(regions, first.path.back(), second.path.front());
    QCOMPARE(zeroTransit.lengthM, 0.0);
    const std::vector<CellCoverage> cells{first, second};
    const std::vector<OrderedCellTraversal> visits = {
        visit(first, CellTraversalOrientation::Forward),
        visit(second, CellTraversalOrientation::Forward, zeroTransit),
    };

    const ComplexCoverageAssemblyResult result = assembleComplexCoverage(regions, cells, visits);

    verifySuccessfulAssembly(regions, result);
    QCOMPARE(result.path.size(), std::size_t{4});
    QCOMPARE(result.legRoles,
             std::vector<PathLegRole>({PathLegRole::Coverage, PathLegRole::Transit, PathLegRole::Coverage}));
    QCOMPARE(result.coverageLengthM, 4.0);
    QCOMPARE(result.transitLengthM, 2.0);
    QCOMPARE(result.pathLengthM, 6.0);
    QCOMPARE(result.cellCount, 2);
    QCOMPARE(result.turnCount, 2);
}

void ComplexCoverageAssemblyTest::_testDeterminismAndCoverageInputOrder()
{
    const PolygonRegionSet2D regions = regionSet(rectangle(0.0, 0.0, 20.0, 20.0));
    const CellCoverage first = coverage(0, {point(1.0, 1.0), point(3.0, 1.0)}, {PathLegRole::Coverage});
    const CellCoverage second = coverage(1, {point(7.0, 3.0), point(9.0, 3.0)}, {PathLegRole::Coverage});
    std::vector<CellCoverage> cells{first, second};
    const std::vector<OrderedCellTraversal> visits = {
        visit(first, CellTraversalOrientation::Forward),
        visit(second, CellTraversalOrientation::Forward, routeStatic(regions, first.path.back(), second.path.front())),
    };
    const ComplexCoverageAssemblyResult firstResult = assembleComplexCoverage(regions, cells, visits);
    std::ranges::reverse(cells);
    const ComplexCoverageAssemblyResult reordered = assembleComplexCoverage(regions, cells, visits);

    verifySuccessfulAssembly(regions, firstResult);
    compareAssemblies(firstResult, reordered);
    for (int repetition = 0; repetition < 10; ++repetition) {
        compareAssemblies(firstResult, assembleComplexCoverage(regions, cells, visits));
    }
}

void ComplexCoverageAssemblyTest::_testInvalidInputsAreAtomic()
{
    const PolygonRegionSet2D regions = regionSet(rectangle(0.0, 0.0, 20.0, 20.0));
    const CellCoverage first = coverage(0, {point(1.0, 1.0), point(3.0, 1.0)}, {PathLegRole::Coverage});
    const CellCoverage second = coverage(1, {point(7.0, 1.0), point(9.0, 1.0)}, {PathLegRole::Coverage});
    const StaticRoute transit = routeStatic(regions, first.path.back(), second.path.front());
    const std::vector<CellCoverage> validCells{first, second};
    const std::vector<OrderedCellTraversal> validVisits = {
        visit(first, CellTraversalOrientation::Forward),
        visit(second, CellTraversalOrientation::Forward, transit),
    };

    std::vector<OrderedCellTraversal> mismatched = validVisits;
    mismatched.at(1).state.entry = point(8.0, 1.0);
    verifyAtomicFailure(assembleComplexCoverage(regions, validCells, mismatched));

    const std::vector<CellCoverage> missingCell{first};
    const std::vector<CellCoverage> duplicateCells{first, first};
    verifyAtomicFailure(assembleComplexCoverage(regions, missingCell, validVisits));
    verifyAtomicFailure(assembleComplexCoverage(regions, duplicateCells, validVisits));

    std::vector<OrderedCellTraversal> duplicateVisits = validVisits;
    duplicateVisits.at(1).state.cellId = 0;
    verifyAtomicFailure(assembleComplexCoverage(regions, validCells, duplicateVisits));

    CellCoverage malformedRoles = first;
    malformedRoles.legRoles.clear();
    const std::vector<CellCoverage> malformedCells{malformedRoles};
    const std::vector<OrderedCellTraversal> malformedVisits{visit(malformedRoles, CellTraversalOrientation::Forward)};
    verifyAtomicFailure(assembleComplexCoverage(regions, malformedCells, malformedVisits));

    std::vector<OrderedCellTraversal> missingTransit = validVisits;
    missingTransit.at(1).transitFromPrevious.reset();
    verifyAtomicFailure(assembleComplexCoverage(regions, validCells, missingTransit));

    std::vector<OrderedCellTraversal> badTransit = validVisits;
    badTransit.at(1).transitFromPrevious.value().path.front() = point(4.0, 1.0);
    verifyAtomicFailure(assembleComplexCoverage(regions, validCells, badTransit));

    CellCoverage zeroLeg =
        coverage(2, {point(1.0, 1.0), point(1.0, 1.0), point(2.0, 1.0)}, {PathLegRole::Transit, PathLegRole::Coverage});
    const std::vector<CellCoverage> zeroLegCells{zeroLeg};
    const std::vector<OrderedCellTraversal> zeroLegVisits{visit(zeroLeg, CellTraversalOrientation::Forward)};
    verifyAtomicFailure(assembleComplexCoverage(regions, zeroLegCells, zeroLegVisits));

    CellCoverage nonFiniteMetric = first;
    nonFiniteMetric.pathLengthM = std::numeric_limits<double>::infinity();
    const std::vector<CellCoverage> nonFiniteCells{nonFiniteMetric};
    const std::vector<OrderedCellTraversal> nonFiniteVisits{visit(nonFiniteMetric, CellTraversalOrientation::Forward)};
    verifyAtomicFailure(assembleComplexCoverage(regions, nonFiniteCells, nonFiniteVisits));

    const PolygonRegionSet2D withHole = regionSet(rectangle(0.0, 0.0, 20.0, 20.0), {rectangle(8.0, 8.0, 12.0, 12.0)});
    const CellCoverage unsafe = coverage(3, {point(2.0, 10.0), point(18.0, 10.0)}, {PathLegRole::Coverage});
    const std::vector<CellCoverage> unsafeCells{unsafe};
    const std::vector<OrderedCellTraversal> unsafeVisits{visit(unsafe, CellTraversalOrientation::Forward)};
    verifyAtomicFailure(assembleComplexCoverage(withHole, unsafeCells, unsafeVisits));
}

void ComplexCoverageAssemblyTest::_testNoGoVerticalSlice()
{
    const PolygonRegionSet2D regions = regionSet(rectangle(0.0, 0.0, 20.0, 20.0), {rectangle(8.0, 8.0, 12.0, 12.0)});
    const CoverageDecompositionResult decomposition = decomposeBoustrophedon(regions, 90.0);
    QVERIFY2(decomposition.status == PlanningStatus::Success, decomposition.message.c_str());

    const CellCoverageGenerationResult generated = generateCellCoverage(decomposition.cells, 4.0, 90.0);
    QVERIFY2(generated.status == PlanningStatus::Success, generated.message.c_str());
    const CellOrderingResult ordered = orderCellTraversals(regions, generated.cells, generated.traversalStates);
    QVERIFY2(ordered.status == PlanningStatus::Success, ordered.message.c_str());

    const ComplexCoverageAssemblyResult assembled = assembleComplexCoverage(regions, generated.cells, ordered.visits);

    verifySuccessfulAssembly(regions, assembled);
    QCOMPARE(assembled.cellCount, static_cast<int>(decomposition.cells.size()));
    QCOMPARE(ordered.visits.size(), decomposition.cells.size());
    std::set<CoverageCellId> visited;
    for (const OrderedCellTraversal& orderedVisit : ordered.visits) {
        QVERIFY(visited.insert(orderedVisit.state.cellId).second);
    }
    QCOMPARE(visited.size(), decomposition.cells.size());
}

UT_REGISTER_TEST_LIGHTWEIGHT(ComplexCoverageAssemblyTest, TestLabel::Unit)
