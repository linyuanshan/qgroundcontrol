#include "CellCoverageTest.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

#include "Geometry/MarineGeometry.h"
#include "Geometry/PolygonRegion.h"
#include "Planning/BoustrophedonDecomposition.h"
#include "Planning/CellCoverage.h"
#include "Planning/CoverageFreeSpace.h"
#include "Planning/PlanningPathMetrics.h"

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

bool pointsEqual(const Point2D& first, const Point2D& second)
{
    return std::abs(first.xM - second.xM) <= Geometry::LengthEpsilonM &&
           std::abs(first.yM - second.yM) <= Geometry::LengthEpsilonM;
}

std::vector<double> coverageLanePositions(const CellCoverage& coverage, double navigationAngleDeg)
{
    std::vector<double> positions;
    const double mathAngleDeg = Geometry::navigationAngleToMathAngle(navigationAngleDeg);
    for (std::size_t index = 0; index < coverage.legRoles.size(); ++index) {
        if (coverage.legRoles[index] != PathLegRole::Coverage) {
            continue;
        }
        const Point2D start = Geometry::toSweepFrame(coverage.path[index], mathAngleDeg);
        const Point2D end = Geometry::toSweepFrame(coverage.path[index + 1], mathAngleDeg);
        if (std::abs(start.yM - end.yM) <= Geometry::LengthEpsilonM) {
            positions.push_back((start.yM + end.yM) / 2.0);
        }
    }
    std::sort(positions.begin(), positions.end());
    return positions;
}

void verifyCoverage(const CellCoverage& coverage)
{
    QVERIFY(coverage.path.size() >= 2);
    QCOMPARE(coverage.legRoles.size(), coverage.path.size() - 1);
    QCOMPARE(coverage.laneCount, static_cast<int>((coverage.path.size() + 1) / 2));
    QCOMPARE(coverage.turnCount, coverage.laneCount - 1);
    QVERIFY(std::isfinite(coverage.pathLengthM));
    QVERIFY(coverage.pathLengthM > 0.0);
    QVERIFY(std::abs(coverage.pathLengthM - coverage.coverageLengthM - coverage.transitLengthM) <=
            Geometry::LengthEpsilonM);
    for (std::size_t index = 0; index < coverage.legRoles.size(); ++index) {
        QCOMPARE(coverage.legRoles[index], (index % 2) == 0 ? PathLegRole::Coverage : PathLegRole::Transit);
    }
}

void compareResults(const CellCoverageGenerationResult& first, const CellCoverageGenerationResult& second)
{
    QCOMPARE(first.status, second.status);
    QCOMPARE(first.error, second.error);
    QCOMPARE(first.message, second.message);
    QCOMPARE(first.cells.size(), second.cells.size());
    QCOMPARE(first.traversalStates.size(), second.traversalStates.size());
    for (std::size_t index = 0; index < first.cells.size(); ++index) {
        const CellCoverage& a = first.cells[index];
        const CellCoverage& b = second.cells[index];
        QCOMPARE(a.cellId, b.cellId);
        QCOMPARE(a.path.size(), b.path.size());
        QCOMPARE(a.legRoles, b.legRoles);
        QCOMPARE(a.coverageLengthM, b.coverageLengthM);
        QCOMPARE(a.transitLengthM, b.transitLengthM);
        QCOMPARE(a.pathLengthM, b.pathLengthM);
        QCOMPARE(a.laneCount, b.laneCount);
        QCOMPARE(a.turnCount, b.turnCount);
        for (std::size_t pointIndex = 0; pointIndex < a.path.size(); ++pointIndex) {
            QCOMPARE(a.path[pointIndex].xM, b.path[pointIndex].xM);
            QCOMPARE(a.path[pointIndex].yM, b.path[pointIndex].yM);
        }
    }
    for (std::size_t index = 0; index < first.traversalStates.size(); ++index) {
        const CellTraversalState& a = first.traversalStates[index];
        const CellTraversalState& b = second.traversalStates[index];
        QCOMPARE(a.cellId, b.cellId);
        QCOMPARE(a.orientation, b.orientation);
        QCOMPARE(a.entry.xM, b.entry.xM);
        QCOMPARE(a.entry.yM, b.entry.yM);
        QCOMPARE(a.exit.xM, b.exit.xM);
        QCOMPARE(a.exit.yM, b.exit.yM);
    }
}

}  // namespace

void CellCoverageTest::_testSingleCellCoverageAndTraversalStates()
{
    const std::vector<CoverageCell> cells = {{.id = 4, .polygon = rectangle(0.0, 0.0, 20.0, 10.0)}};
    const CellCoverageGenerationResult result = generateCellCoverage(cells, 4.0, 90.0);

    QVERIFY2(result.status == PlanningStatus::Success, result.message.c_str());
    QCOMPARE(result.error, CoveragePlanningError::None);
    QCOMPARE(result.cells.size(), std::size_t{1});
    QCOMPARE(result.traversalStates.size(), std::size_t{2});
    const CellCoverage& coverage = result.cells.front();
    QCOMPARE(coverage.cellId, CoverageCellId{4});
    verifyCoverage(coverage);

    const CellTraversalState& forward = result.traversalStates[0];
    const CellTraversalState& reverse = result.traversalStates[1];
    QCOMPARE(forward.cellId, coverage.cellId);
    QCOMPARE(reverse.cellId, coverage.cellId);
    QCOMPARE(forward.orientation, CellTraversalOrientation::Forward);
    QCOMPARE(reverse.orientation, CellTraversalOrientation::Reverse);
    QVERIFY(pointsEqual(forward.entry, coverage.path.front()));
    QVERIFY(pointsEqual(forward.exit, coverage.path.back()));
    QVERIFY(pointsEqual(reverse.entry, coverage.path.back()));
    QVERIFY(pointsEqual(reverse.exit, coverage.path.front()));
}

void CellCoverageTest::_testSharedLaneLatticeAcrossArtificialBoundary()
{
    const std::vector<CoverageCell> cells = {
        {.id = 1, .polygon = rectangle(0.0, 6.0, 10.0, 12.0)},
        {.id = 0, .polygon = rectangle(0.0, 0.0, 10.0, 6.0)},
    };
    const std::vector<CoverageCell> sortedCells = {cells[1], cells[0]};
    const CellCoverageGenerationResult first = generateCellCoverage(cells, 5.0, 90.0);
    const CellCoverageGenerationResult second = generateCellCoverage(sortedCells, 5.0, 90.0);

    QVERIFY2(first.status == PlanningStatus::Success, first.message.c_str());
    compareResults(first, second);
    QCOMPARE(first.cells.size(), std::size_t{2});
    QCOMPARE(first.cells[0].cellId, CoverageCellId{0});
    QCOMPARE(first.cells[1].cellId, CoverageCellId{1});
    const std::vector<double> lowerLanes = coverageLanePositions(first.cells[0], 90.0);
    const std::vector<double> upperLanes = coverageLanePositions(first.cells[1], 90.0);
    QVERIFY(!lowerLanes.empty());
    QVERIFY(!upperLanes.empty());
    QVERIFY(std::abs(lowerLanes.front() - 2.5) <= Geometry::LengthEpsilonM);
    QVERIFY(std::abs(upperLanes.front() - 7.5) <= Geometry::LengthEpsilonM);

    const double independentCenterGapM = 9.0 - 3.0;
    QVERIFY(independentCenterGapM > 5.0);
    const double sharedScheduleBoundaryGapM = upperLanes.front() - lowerLanes.back();
    QVERIFY(sharedScheduleBoundaryGapM <= 5.0 + Geometry::LengthEpsilonM);
    QVERIFY((6.0 - lowerLanes.back()) <= 2.5 + Geometry::LengthEpsilonM);
    QVERIFY((upperLanes.front() - 6.0) <= 2.5 + Geometry::LengthEpsilonM);
}

void CellCoverageTest::_testNoGoDecompositionCoversEveryCell()
{
    const PolygonRegionSet2D feasibleRegion = {
        {.outerBoundary = rectangle(0.0, 0.0, 20.0, 20.0), .holes = {rectangle(8.0, 8.0, 12.0, 12.0)}}};
    const CoverageDecompositionResult decomposition = decomposeBoustrophedon(feasibleRegion, 90.0);
    QVERIFY2(decomposition.status == PlanningStatus::Success, decomposition.message.c_str());
    QCOMPARE(decomposition.cells.size(), std::size_t{4});

    const CellCoverageGenerationResult result = generateCellCoverage(decomposition.cells, 4.0, 90.0);
    QVERIFY2(result.status == PlanningStatus::Success, result.message.c_str());
    QCOMPARE(result.cells.size(), decomposition.cells.size());
    QCOMPARE(result.traversalStates.size(), decomposition.cells.size() * 2);
    int totalLaneCount = 0;
    for (std::size_t index = 0; index < result.cells.size(); ++index) {
        QCOMPARE(result.cells[index].cellId, decomposition.cells[index].id);
        verifyCoverage(result.cells[index]);
        totalLaneCount += result.cells[index].laneCount;
    }
    QCOMPARE(totalLaneCount, 6);
}

void CellCoverageTest::_testBackendDerivedRoundedCells()
{
    CoveragePlanningProblem problem;
    problem.executionSafety.executionMarginM = 0.0;
    problem.region.coverageBoundary = rectangle(0.0, 0.0, 20.0, 20.0);
    problem.region.navigationBoundary = problem.region.coverageBoundary;
    problem.region.noGoRegions = {rectangle(8.0, 8.0, 12.0, 12.0)};
    problem.swathWidthM = 4.0;
    problem.safety.hardSafetyMarginM = 1.0;
    problem.safety.preferredSafetyMarginM = problem.safety.hardSafetyMarginM;
    problem.sweepAngleMode = SweepAngleMode::Manual;
    problem.requestedSweepAngleDeg = 90.0;

    const CoverageFreeSpaceResult freeSpace = buildCoverageFreeSpace(problem);
    QVERIFY2(freeSpace.status == PlanningStatus::Success, freeSpace.message.c_str());
    const CoverageDecompositionResult decomposition =
        decomposeBoustrophedon(freeSpace.freeSpace.nominalTrackFeasibleRegion, problem.requestedSweepAngleDeg);
    QVERIFY2(decomposition.status == PlanningStatus::Success, decomposition.message.c_str());

    const double mathAngleDeg = Geometry::navigationAngleToMathAngle(problem.requestedSweepAngleDeg);
    bool containsDenseBackendCell = false;
    for (const CoverageCell& cell : decomposition.cells) {
        const PolygonRegion2D cellRegion{.outerBoundary = cell.polygon};
        QVERIFY(Geometry::isValidPolygonRegion(cellRegion));
        QVERIFY(Geometry::isMonotoneCellPolygon(cell.polygon, mathAngleDeg));
        containsDenseBackendCell |=
            (cell.polygon.vertices.size() > 4) && !Geometry::isSimpleNonDegeneratePolygon(cell.polygon);
    }
    QVERIFY(containsDenseBackendCell);

    const CellCoverageGenerationResult first =
        generateCellCoverage(decomposition.cells, problem.swathWidthM, problem.requestedSweepAngleDeg);
    QVERIFY2(first.status == PlanningStatus::Success, first.message.c_str());
    QCOMPARE(first.cells.size(), decomposition.cells.size());
    QCOMPARE(first.traversalStates.size(), decomposition.cells.size() * 2);

    for (const CellCoverage& coverage : first.cells) {
        const auto cellIterator = std::ranges::find_if(
            decomposition.cells, [&coverage](const CoverageCell& cell) { return cell.id == coverage.cellId; });
        QVERIFY(cellIterator != decomposition.cells.end());
        const PolygonRegionSet2D cellRegions{{.outerBoundary = cellIterator->polygon}};
        for (const Point2D& pathPoint : coverage.path) {
            QVERIFY(Geometry::pointInsidePolygonRegion(cellRegions, pathPoint));
        }
        QCOMPARE(coverage.legRoles.size(), coverage.path.size() - 1);
        for (std::size_t index = 1; index < coverage.path.size(); ++index) {
            QVERIFY(Geometry::segmentInsidePolygonRegion(cellRegions, coverage.path[index - 1], coverage.path[index]));
        }
    }

    std::vector<CoverageCell> reversedCells = decomposition.cells;
    std::ranges::reverse(reversedCells);
    const CellCoverageGenerationResult reversed =
        generateCellCoverage(reversedCells, problem.swathWidthM, problem.requestedSweepAngleDeg);
    compareResults(first, reversed);
}

void CellCoverageTest::_testNonCardinalAndNarrowCells()
{
    constexpr double NavigationAngleDeg = 37.0;
    const double mathAngleDeg = Geometry::navigationAngleToMathAngle(NavigationAngleDeg);
    Polygon2D aligned = rectangle(-5.0, 10.0, 15.0, 12.0);
    for (Point2D& vertex : aligned.vertices) {
        vertex = Geometry::fromSweepFrame(vertex, mathAngleDeg);
    }
    const std::vector<CoverageCell> cells = {{.id = 0, .polygon = aligned}};

    const CellCoverageGenerationResult first = generateCellCoverage(cells, 5.0, NavigationAngleDeg);
    const CellCoverageGenerationResult second = generateCellCoverage(cells, 5.0, NavigationAngleDeg);
    QVERIFY2(first.status == PlanningStatus::Success, first.message.c_str());
    compareResults(first, second);
    QCOMPARE(first.cells.front().laneCount, 1);
    verifyCoverage(first.cells.front());
}

void CellCoverageTest::_testAlternateLaneParity()
{
    const Polygon2D unsafe =
        polygon({{0.0, 0.0}, {10.0, 0.0}, {10.0, 1.5}, {3.0, 3.0}, {10.0, 4.5}, {10.0, 10.0}, {0.0, 10.0}});
    const std::vector<CoverageCell> cells = {
        {.id = 0, .polygon = rectangle(20.0, 0.0, 30.0, 10.0)},
        {.id = 1, .polygon = unsafe},
    };
    const CellCoverageGenerationResult result = generateCellCoverage(cells, 4.0, 90.0);

    QVERIFY2(result.status == PlanningStatus::Success, result.message.c_str());
    QCOMPARE(result.cells.size(), cells.size());
    for (const CellCoverage& coverage : result.cells) {
        verifyCoverage(coverage);
    }
    compareResults(result, generateCellCoverage(cells, 4.0, 90.0));
}

void CellCoverageTest::_testFailureIsAtomic()
{
    const Polygon2D invalid = polygon({{0.0, 0.0}, {10.0, 10.0}, {0.0, 10.0}, {10.0, 0.0}});
    const std::vector<CoverageCell> cells = {
        {.id = 0, .polygon = rectangle(20.0, 0.0, 30.0, 10.0)},
        {.id = 1, .polygon = invalid},
    };
    const CellCoverageGenerationResult result = generateCellCoverage(cells, 4.0, 90.0);

    QCOMPARE(result.status, PlanningStatus::Failed);
    QCOMPARE(result.error, CoveragePlanningError::CellCoverageFailed);
    QVERIFY(result.cells.empty());
    QVERIFY(result.traversalStates.empty());
}

void CellCoverageTest::_testTargetTrackClipping()
{
    const std::vector<CoverageCell> cells = {{.id = 3, .polygon = rectangle(0.0, 0.0, 10.0, 10.0)}};
    const PolygonRegionSet2D target{{.outerBoundary = rectangle(0.0, 0.0, 10.0, 10.0)}};
    const PolygonRegionSet2D activeTrack{{.outerBoundary = rectangle(2.0, 0.0, 8.0, 10.0)}};
    const CellCoverageGenerationResult result = generateCellCoverage(cells, activeTrack, 4.0, 90.0);

    QVERIFY2(result.status == PlanningStatus::Success, result.message.c_str());
    QCOMPARE(result.error, CoveragePlanningError::None);
    QCOMPARE(result.cells.size(), std::size_t{1});
    QCOMPARE(result.traversalStates.size(), std::size_t{2});
    const CellCoverage& coverage = result.cells.front();
    int coverageLaneCount = 0;
    const double mathAngleDeg = Geometry::navigationAngleToMathAngle(90.0);
    for (std::size_t index = 0; index < coverage.legRoles.size(); ++index) {
        const Point2D& first = coverage.path[index];
        const Point2D& second = coverage.path[index + 1];
        QVERIFY(Geometry::segmentInsidePolygonRegion(activeTrack, first, second));
        if (coverage.legRoles[index] != PathLegRole::Coverage) {
            continue;
        }
        ++coverageLaneCount;
        QVERIFY(Geometry::segmentInsidePolygonRegion(target, first, second));
        const Point2D sweepFirst = Geometry::toSweepFrame(first, mathAngleDeg);
        const Point2D sweepSecond = Geometry::toSweepFrame(second, mathAngleDeg);
        QVERIFY(std::abs(std::min(sweepFirst.xM, sweepSecond.xM) - 2.0) <= Geometry::LengthEpsilonM);
        QVERIFY(std::abs(std::max(sweepFirst.xM, sweepSecond.xM) - 8.0) <= Geometry::LengthEpsilonM);
    }
    QVERIFY(coverageLaneCount > 0);
    QCOMPARE(coverage.laneCount, coverageLaneCount);
}

void CellCoverageTest::_testTransitCanLeaveTargetAndUsesPathMetrics()
{
    const Polygon2D targetPolygon =
        polygon({{0.0, 0.0}, {10.0, 0.0}, {10.0, 1.5}, {3.0, 3.0}, {10.0, 4.5}, {10.0, 10.0}, {0.0, 10.0}});
    const std::vector<CoverageCell> cells = {{.id = 5, .polygon = targetPolygon}};
    const PolygonRegionSet2D target{{.outerBoundary = targetPolygon}};
    const PolygonRegionSet2D activeTrack{{.outerBoundary = rectangle(-5.0, -5.0, 15.0, 15.0)}};
    const CellCoverageGenerationResult result = generateCellCoverage(cells, activeTrack, 4.0, 90.0);

    QVERIFY2(result.status == PlanningStatus::Success, result.message.c_str());
    const CellCoverage& coverage = result.cells.front();
    bool transitLeavesTarget = false;
    int coverageLaneCount = 0;
    for (std::size_t index = 0; index < coverage.legRoles.size(); ++index) {
        const Point2D& first = coverage.path[index];
        const Point2D& second = coverage.path[index + 1];
        QVERIFY(Geometry::segmentInsidePolygonRegion(activeTrack, first, second));
        if (coverage.legRoles[index] == PathLegRole::Coverage) {
            ++coverageLaneCount;
            QVERIFY(Geometry::segmentInsidePolygonRegion(target, first, second));
        } else {
            transitLeavesTarget |= !Geometry::segmentInsidePolygonRegion(target, first, second);
        }
    }
    QVERIFY(transitLeavesTarget);
    QCOMPARE(coverage.laneCount, coverageLaneCount);

    const std::optional<PlanningPathMetrics> metrics = calculatePlanningPathMetrics(coverage.path, coverage.legRoles);
    QVERIFY(metrics.has_value());
    QCOMPARE(coverage.coverageLengthM, metrics->coverageLengthM);
    QCOMPARE(coverage.transitLengthM, metrics->transitLengthM);
    QCOMPARE(coverage.pathLengthM, metrics->pathLengthM);
    QCOMPARE(coverage.turnCount, metrics->turnCount);
}

void CellCoverageTest::_testMultiIntervalSelectionIsDeterministic()
{
    const std::vector<CoverageCell> cells = {{.id = 0, .polygon = rectangle(0.0, 0.0, 20.0, 10.0)}};
    const PolygonRegionSet2D activeTrack = {{.outerBoundary = rectangle(0.0, 0.0, 8.0, 10.0)},
                                             {.outerBoundary = rectangle(12.0, 0.0, 20.0, 10.0)}};
    PolygonRegionSet2D reversedActiveTrack = activeTrack;
    std::ranges::reverse(reversedActiveTrack);
    const CellCoverageGenerationResult first = generateCellCoverage(cells, activeTrack, 4.0, 90.0);

    QVERIFY2(first.status == PlanningStatus::Success, first.message.c_str());
    compareResults(first, generateCellCoverage(cells, reversedActiveTrack, 4.0, 90.0));
    compareResults(first, generateCellCoverage(cells, activeTrack, 4.0, 90.0));
    const CellCoverage& coverage = first.cells.front();
    int coverageLaneCount = 0;
    const double mathAngleDeg = Geometry::navigationAngleToMathAngle(90.0);
    for (std::size_t index = 0; index < coverage.legRoles.size(); ++index) {
        if (coverage.legRoles[index] != PathLegRole::Coverage) {
            continue;
        }
        ++coverageLaneCount;
        const Point2D firstPoint = Geometry::toSweepFrame(coverage.path[index], mathAngleDeg);
        const Point2D secondPoint = Geometry::toSweepFrame(coverage.path[index + 1], mathAngleDeg);
        QVERIFY(std::abs(std::min(firstPoint.xM, secondPoint.xM)) <= Geometry::LengthEpsilonM);
        QVERIFY(std::abs(std::max(firstPoint.xM, secondPoint.xM) - 8.0) <= Geometry::LengthEpsilonM);
    }
    QVERIFY(coverageLaneCount > 0);
    QCOMPARE(coverage.laneCount, coverageLaneCount);
}

void CellCoverageTest::_testActiveTrackHoleIsRespected()
{
    const std::vector<CoverageCell> cells = {{.id = 0, .polygon = rectangle(0.0, 0.0, 10.0, 10.0)}};
    const PolygonRegionSet2D activeTrack = {
        {.outerBoundary = rectangle(0.0, 0.0, 10.0, 10.0), .holes = {rectangle(4.0, 3.0, 6.0, 6.0)}}};
    const CellCoverageGenerationResult result = generateCellCoverage(cells, activeTrack, 3.0, 90.0);

    QVERIFY2(result.status == PlanningStatus::Success, result.message.c_str());
    const CellCoverage& coverage = result.cells.front();
    const double mathAngleDeg = Geometry::navigationAngleToMathAngle(90.0);
    bool selectedHoleLane = false;
    for (std::size_t index = 0; index < coverage.legRoles.size(); ++index) {
        const Point2D& first = coverage.path[index];
        const Point2D& second = coverage.path[index + 1];
        QVERIFY(Geometry::segmentInsidePolygonRegion(activeTrack, first, second));
        if (coverage.legRoles[index] != PathLegRole::Coverage) {
            continue;
        }
        const Point2D sweepFirst = Geometry::toSweepFrame(first, mathAngleDeg);
        const Point2D sweepSecond = Geometry::toSweepFrame(second, mathAngleDeg);
        if (std::abs(sweepFirst.yM - 4.5) <= Geometry::LengthEpsilonM &&
            std::abs(sweepSecond.yM - 4.5) <= Geometry::LengthEpsilonM) {
            selectedHoleLane = true;
            QVERIFY(std::max(sweepFirst.xM, sweepSecond.xM) <= 4.0 + Geometry::LengthEpsilonM);
        }
    }
    QVERIFY(selectedHoleLane);
}

void CellCoverageTest::_testNoTrackOverlap()
{
    const std::vector<CoverageCell> cells = {{.id = 0, .polygon = rectangle(0.0, 0.0, 10.0, 10.0)}};
    const PolygonRegionSet2D activeTrack{{.outerBoundary = rectangle(12.0, 0.0, 20.0, 10.0)}};
    const CellCoverageGenerationResult result = generateCellCoverage(cells, activeTrack, 4.0, 90.0);

    QCOMPARE(result.status, PlanningStatus::Failed);
    QCOMPARE(result.error, CoveragePlanningError::CoverageImpossibleWithExecutionMargin);
    QVERIFY(result.cells.empty());
    QVERIFY(result.traversalStates.empty());
}

void CellCoverageTest::_testTrackOverloadFailureIsAtomic()
{
    const std::vector<CoverageCell> cells = {
        {.id = 0, .polygon = rectangle(0.0, 0.0, 10.0, 10.0)},
        {.id = 1, .polygon = rectangle(20.0, 0.0, 30.0, 10.0)},
    };
    const PolygonRegionSet2D activeTrack{{.outerBoundary = rectangle(0.0, 0.0, 10.0, 10.0)}};
    const CellCoverageGenerationResult result = generateCellCoverage(cells, activeTrack, 4.0, 90.0);

    QCOMPARE(result.status, PlanningStatus::Failed);
    QCOMPARE(result.error, CoveragePlanningError::CoverageImpossibleWithExecutionMargin);
    QVERIFY(result.cells.empty());
    QVERIFY(result.traversalStates.empty());
}

void CellCoverageTest::_testTrackOverloadInputValidation()
{
    const std::vector<CoverageCell> validCells = {{.id = 0, .polygon = rectangle(0.0, 0.0, 10.0, 10.0)}};
    const PolygonRegionSet2D validTrack{{.outerBoundary = rectangle(0.0, 0.0, 10.0, 10.0)}};

    const CellCoverageGenerationResult invalidSwath = generateCellCoverage(validCells, validTrack, 0.0, 90.0);
    QCOMPARE(invalidSwath.status, PlanningStatus::InvalidInput);
    QCOMPARE(invalidSwath.error, CoveragePlanningError::InvalidSwathWidth);
    QVERIFY(invalidSwath.cells.empty());
    QVERIFY(invalidSwath.traversalStates.empty());

    const CellCoverageGenerationResult nonFiniteSwath =
        generateCellCoverage(validCells, validTrack, std::numeric_limits<double>::infinity(), 90.0);
    QCOMPARE(nonFiniteSwath.status, PlanningStatus::InvalidInput);
    QCOMPARE(nonFiniteSwath.error, CoveragePlanningError::InvalidSwathWidth);
    QVERIFY(nonFiniteSwath.cells.empty());
    QVERIFY(nonFiniteSwath.traversalStates.empty());

    const CellCoverageGenerationResult invalidAngle =
        generateCellCoverage(validCells, validTrack, 4.0, std::numeric_limits<double>::infinity());
    QCOMPARE(invalidAngle.status, PlanningStatus::InvalidInput);
    QCOMPARE(invalidAngle.error, CoveragePlanningError::InvalidSweepAngle);
    QVERIFY(invalidAngle.cells.empty());
    QVERIFY(invalidAngle.traversalStates.empty());

    const CellCoverageGenerationResult emptyCells =
        generateCellCoverage(std::span<const CoverageCell>{}, validTrack, 4.0, 90.0);
    QCOMPARE(emptyCells.status, PlanningStatus::Failed);
    QCOMPARE(emptyCells.error, CoveragePlanningError::CellCoverageFailed);
    QVERIFY(emptyCells.cells.empty());
    QVERIFY(emptyCells.traversalStates.empty());

    const CellCoverageGenerationResult emptyTrack = generateCellCoverage(validCells, PolygonRegionSet2D{}, 4.0, 90.0);
    QCOMPARE(emptyTrack.status, PlanningStatus::Failed);
    QCOMPARE(emptyTrack.error, CoveragePlanningError::NoNavigableArea);
    QVERIFY(emptyTrack.cells.empty());
    QVERIFY(emptyTrack.traversalStates.empty());

    const Polygon2D invalidTrackPolygon = polygon({{0.0, 0.0}, {10.0, 10.0}, {0.0, 10.0}, {10.0, 0.0}});
    const PolygonRegionSet2D invalidTrack{{.outerBoundary = invalidTrackPolygon}};
    const CellCoverageGenerationResult badTrack = generateCellCoverage(validCells, invalidTrack, 4.0, 90.0);
    QCOMPARE(badTrack.status, PlanningStatus::Failed);
    QCOMPARE(badTrack.error, CoveragePlanningError::GeometryFailure);
    QVERIFY(badTrack.cells.empty());
    QVERIFY(badTrack.traversalStates.empty());

    const std::vector<CoverageCell> invalidCells = {
        {.id = 0, .polygon = polygon({{0.0, 0.0}, {10.0, 10.0}, {0.0, 10.0}, {10.0, 0.0}})}};
    const CellCoverageGenerationResult badCell = generateCellCoverage(invalidCells, validTrack, 4.0, 90.0);
    QCOMPARE(badCell.status, PlanningStatus::Failed);
    QCOMPARE(badCell.error, CoveragePlanningError::CellCoverageFailed);
    QVERIFY(badCell.cells.empty());
    QVERIFY(badCell.traversalStates.empty());

    const Polygon2D nonMonotonePolygon =
        polygon({{0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {7.0, 10.0}, {7.0, 3.0}, {3.0, 3.0}, {3.0, 10.0},
                 {0.0, 10.0}});
    QVERIFY(Geometry::isValidPolygonRegion({.outerBoundary = nonMonotonePolygon}));
    QVERIFY(!Geometry::isMonotoneCellPolygon(nonMonotonePolygon, Geometry::navigationAngleToMathAngle(90.0)));
    const std::vector<CoverageCell> nonMonotoneCells = {{.id = 2, .polygon = nonMonotonePolygon}};
    const CellCoverageGenerationResult nonMonotone = generateCellCoverage(nonMonotoneCells, validTrack, 4.0, 90.0);
    QCOMPARE(nonMonotone.status, PlanningStatus::Failed);
    QCOMPARE(nonMonotone.error, CoveragePlanningError::CellCoverageFailed);
    QVERIFY(nonMonotone.cells.empty());
    QVERIFY(nonMonotone.traversalStates.empty());

    const std::vector<CoverageCell> duplicateIds = {
        {.id = 1, .polygon = rectangle(0.0, 0.0, 10.0, 10.0)},
        {.id = 1, .polygon = rectangle(20.0, 0.0, 30.0, 10.0)},
    };
    const CellCoverageGenerationResult duplicateId = generateCellCoverage(duplicateIds, validTrack, 4.0, 90.0);
    QCOMPARE(duplicateId.status, PlanningStatus::Failed);
    QCOMPARE(duplicateId.error, CoveragePlanningError::CellCoverageFailed);
    QVERIFY(duplicateId.cells.empty());
    QVERIFY(duplicateId.traversalStates.empty());
}

UT_REGISTER_TEST_LIGHTWEIGHT(CellCoverageTest, TestLabel::Unit)
