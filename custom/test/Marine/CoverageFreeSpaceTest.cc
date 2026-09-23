#include "CoverageFreeSpaceTest.h"

#include "Geometry/PolygonRegion.h"
#include "Planning/CoverageFreeSpace.h"

using namespace Marine;

namespace {

Polygon2D rectangle(double minimumX, double minimumY, double maximumX, double maximumY)
{
    return {{{minimumX, minimumY}, {maximumX, minimumY}, {maximumX, maximumY}, {minimumX, maximumY}}};
}

CoveragePlanningProblem problemWithOuter(const Polygon2D& outer)
{
    CoveragePlanningProblem problem;
    problem.region.outerBoundary = outer;
    problem.swathWidthM = 4.0;
    problem.safetyMarginM = 1.0;
    problem.sweepAngleMode = SweepAngleMode::Manual;
    problem.requestedSweepAngleDeg = 0.0;
    return problem;
}

void comparePolygon(const Polygon2D& first, const Polygon2D& second)
{
    QCOMPARE(first.vertices.size(), second.vertices.size());
    for (std::size_t index = 0; index < first.vertices.size(); ++index) {
        QCOMPARE(first.vertices[index].xM, second.vertices[index].xM);
        QCOMPARE(first.vertices[index].yM, second.vertices[index].yM);
    }
}

void compareRegionSet(const PolygonRegionSet2D& first, const PolygonRegionSet2D& second)
{
    QCOMPARE(first.size(), second.size());
    for (std::size_t regionIndex = 0; regionIndex < first.size(); ++regionIndex) {
        comparePolygon(first[regionIndex].outerBoundary, second[regionIndex].outerBoundary);
        QCOMPARE(first[regionIndex].holes.size(), second[regionIndex].holes.size());
        for (std::size_t holeIndex = 0; holeIndex < first[regionIndex].holes.size(); ++holeIndex) {
            comparePolygon(first[regionIndex].holes[holeIndex], second[regionIndex].holes[holeIndex]);
        }
    }
}

}  // namespace

void CoverageFreeSpaceTest::_testValidNoGoValidation()
{
    const Polygon2D outer = rectangle(0.0, 0.0, 20.0, 20.0);
    const std::vector<Polygon2D> oneNoGo{rectangle(3.0, 3.0, 6.0, 6.0)};
    const std::vector<Polygon2D> multipleNoGo{oneNoGo.front(), rectangle(12.0, 12.0, 15.0, 15.0)};

    QCOMPARE(Geometry::validateNoGoRegions(outer, oneNoGo), Geometry::NoGoValidationStatus::Success);
    QCOMPARE(Geometry::validateNoGoRegions(outer, multipleNoGo), Geometry::NoGoValidationStatus::Success);

    const Polygon2D selfIntersecting{{{2.0, 2.0}, {6.0, 6.0}, {2.0, 6.0}, {6.0, 2.0}}};
    QCOMPARE(Geometry::validateNoGoRegions(outer, {selfIntersecting}),
             Geometry::NoGoValidationStatus::InvalidNoGoRegion);
}

void CoverageFreeSpaceTest::_testOuterBoundaryConflicts()
{
    const Polygon2D outer = rectangle(0.0, 0.0, 20.0, 20.0);

    QCOMPARE(Geometry::validateNoGoRegions(outer, {rectangle(21.0, 2.0, 23.0, 4.0)}),
             Geometry::NoGoValidationStatus::OutsideOuterBoundary);
    QCOMPARE(Geometry::validateNoGoRegions(outer, {rectangle(0.0, 2.0, 3.0, 4.0)}),
             Geometry::NoGoValidationStatus::BoundaryConflict);
    QCOMPARE(Geometry::validateNoGoRegions(outer, {rectangle(-1.0, 2.0, 3.0, 4.0)}),
             Geometry::NoGoValidationStatus::BoundaryConflict);
}

void CoverageFreeSpaceTest::_testNoGoPairConflicts()
{
    const Polygon2D outer = rectangle(0.0, 0.0, 20.0, 20.0);
    const Polygon2D first = rectangle(3.0, 3.0, 8.0, 8.0);

    QCOMPARE(Geometry::validateNoGoRegions(outer, {first, rectangle(7.0, 7.0, 10.0, 10.0)}),
             Geometry::NoGoValidationStatus::OverlapOrTouch);
    QCOMPARE(Geometry::validateNoGoRegions(outer, {first, rectangle(8.0, 4.0, 10.0, 6.0)}),
             Geometry::NoGoValidationStatus::OverlapOrTouch);
    QCOMPARE(Geometry::validateNoGoRegions(outer, {first, rectangle(8.0005, 4.0, 10.0, 6.0)}),
             Geometry::NoGoValidationStatus::OverlapOrTouch);
    QCOMPARE(Geometry::validateNoGoRegions(outer, {first, rectangle(4.0, 4.0, 6.0, 6.0)}),
             Geometry::NoGoValidationStatus::OverlapOrTouch);
}

void CoverageFreeSpaceTest::_testValidationErrorMapping()
{
    const Polygon2D outer = rectangle(0.0, 0.0, 20.0, 20.0);
    CoveragePlanningProblem problem = problemWithOuter(outer);

    problem.region.noGoRegions = {{{{2.0, 2.0}, {6.0, 6.0}, {2.0, 6.0}, {6.0, 2.0}}}};
    QCOMPARE(buildCoverageFreeSpace(problem).error, CoveragePlanningError::InvalidNoGoRegion);

    problem.region.noGoRegions = {rectangle(21.0, 2.0, 23.0, 4.0)};
    QCOMPARE(buildCoverageFreeSpace(problem).error, CoveragePlanningError::NoGoOutsideBoundary);

    problem.region.noGoRegions = {rectangle(0.0, 2.0, 3.0, 4.0)};
    QCOMPARE(buildCoverageFreeSpace(problem).error, CoveragePlanningError::NoGoBoundaryConflict);

    problem.region.noGoRegions = {rectangle(3.0, 3.0, 8.0, 8.0), rectangle(7.0, 7.0, 10.0, 10.0)};
    QCOMPARE(buildCoverageFreeSpace(problem).error, CoveragePlanningError::NoGoOverlapOrTouch);
}

void CoverageFreeSpaceTest::_testCoverageTargetAndTrackFeasibleRegion()
{
    CoveragePlanningProblem problem = problemWithOuter(rectangle(0.0, 0.0, 20.0, 20.0));
    problem.region.noGoRegions = {rectangle(3.0, 3.0, 6.0, 6.0), rectangle(12.0, 12.0, 15.0, 15.0)};

    const Geometry::PolygonRegionOperationResult target =
        Geometry::buildCoverageTarget(problem.region.outerBoundary, problem.region.noGoRegions);
    QCOMPARE(target.status, Geometry::PolygonRegionOperationStatus::Success);
    const Geometry::PolygonRegionOperationResult track = Geometry::buildTrackFeasibleRegion(
        problem.region.outerBoundary, problem.region.noGoRegions, problem.safetyMarginM);
    QCOMPARE(track.status, Geometry::PolygonRegionOperationStatus::Success);
    const Geometry::PolygonRegionOperationResult reachable =
        Geometry::bufferPolygonRegions(track.regions, problem.swathWidthM / 2.0);
    QCOMPARE(reachable.status, Geometry::PolygonRegionOperationStatus::Success);
    const Geometry::PolygonRegionContainmentResult containment =
        Geometry::isRegionSetContained(target.regions, reachable.regions);
    QCOMPARE(containment.status, Geometry::PolygonRegionOperationStatus::Success);
    QVERIFY(containment.contained);

    const CoverageFreeSpaceResult result = buildCoverageFreeSpace(problem);

    QCOMPARE(result.status, PlanningStatus::Success);
    QCOMPARE(result.error, CoveragePlanningError::None);
    QCOMPARE(result.freeSpace.coverageTarget.holes.size(), std::size_t{2});
    QCOMPARE(result.freeSpace.nominalTrackFeasibleRegion.size(), std::size_t{1});
    QCOMPARE(result.freeSpace.nominalTrackFeasibleRegion.front().holes.size(), std::size_t{2});
    QVERIFY(result.freeSpace.coverageTarget.isFinite());
    QVERIFY(result.freeSpace.nominalTrackFeasibleRegion.front().isFinite());
}

void CoverageFreeSpaceTest::_testInflationMergesNoGo()
{
    CoveragePlanningProblem problem = problemWithOuter(rectangle(0.0, 0.0, 30.0, 20.0));
    problem.swathWidthM = 4.2;
    problem.region.noGoRegions = {rectangle(8.0, 8.0, 12.0, 12.0), rectangle(14.0, 8.0, 18.0, 12.0)};

    const Geometry::PolygonRegionOperationResult target =
        Geometry::buildCoverageTarget(problem.region.outerBoundary, problem.region.noGoRegions);
    const Geometry::PolygonRegionOperationResult track = Geometry::buildTrackFeasibleRegion(
        problem.region.outerBoundary, problem.region.noGoRegions, problem.safetyMarginM);

    QCOMPARE(target.status, Geometry::PolygonRegionOperationStatus::Success);
    QCOMPARE(target.regions.front().holes.size(), std::size_t{2});
    QCOMPARE(track.status, Geometry::PolygonRegionOperationStatus::Success);
    QCOMPARE(track.regions.size(), std::size_t{1});
    QCOMPARE(track.regions.front().holes.size(), std::size_t{1});
    QCOMPARE(buildCoverageFreeSpace(problem).error, CoveragePlanningError::CoverageImpossibleWithSafetyMargin);
}

void CoverageFreeSpaceTest::_testDisconnectedFeasibleRegion()
{
    CoveragePlanningProblem problem = problemWithOuter(rectangle(0.0, 0.0, 20.0, 10.0));
    problem.region.noGoRegions = {rectangle(9.0, 1.5, 11.0, 8.5)};

    const CoverageFreeSpaceResult result = buildCoverageFreeSpace(problem);

    QCOMPARE(result.status, PlanningStatus::Failed);
    QCOMPARE(result.error, CoveragePlanningError::DisconnectedFeasibleRegion);
}

void CoverageFreeSpaceTest::_testNoNavigableArea()
{
    CoveragePlanningProblem problem = problemWithOuter(rectangle(0.0, 0.0, 4.0, 4.0));
    problem.swathWidthM = 4.0;
    problem.safetyMarginM = 2.0;

    const CoverageFreeSpaceResult result = buildCoverageFreeSpace(problem);

    QCOMPARE(result.status, PlanningStatus::Failed);
    QCOMPARE(result.error, CoveragePlanningError::NoNavigableArea);
}

void CoverageFreeSpaceTest::_testSafetyExceedsHalfSwath()
{
    CoveragePlanningProblem problem = problemWithOuter(rectangle(0.0, 0.0, 20.0, 20.0));
    problem.swathWidthM = 1.9;
    problem.safetyMarginM = 1.0;

    const CoverageFreeSpaceResult result = buildCoverageFreeSpace(problem);

    QCOMPARE(result.status, PlanningStatus::Failed);
    QCOMPARE(result.error, CoveragePlanningError::CoverageImpossibleWithSafetyMargin);
}

void CoverageFreeSpaceTest::_testUnreachableCoverage()
{
    CoveragePlanningProblem problem;
    problem.region.outerBoundary.vertices = {
        {0.0, 0.0}, {20.0, 0.0}, {20.0, 9.25}, {30.0, 9.25}, {30.0, 10.75}, {20.0, 10.75}, {20.0, 20.0}, {0.0, 20.0},
    };
    problem.swathWidthM = 2.0;
    problem.safetyMarginM = 1.0;
    problem.sweepAngleMode = SweepAngleMode::Manual;

    const CoverageFreeSpaceResult result = buildCoverageFreeSpace(problem);

    QCOMPARE(result.status, PlanningStatus::Failed);
    QCOMPARE(result.error, CoveragePlanningError::CoverageImpossibleWithSafetyMargin);
}

void CoverageFreeSpaceTest::_testDeterminism()
{
    CoveragePlanningProblem problem = problemWithOuter(rectangle(0.123, 0.456, 30.123, 20.456));
    problem.swathWidthM = 6.2;
    problem.region.noGoRegions = {rectangle(14.123, 8.456, 18.123, 12.456), rectangle(8.123, 8.456, 12.123, 12.456)};

    const CoverageFreeSpaceResult first = buildCoverageFreeSpace(problem);
    const CoverageFreeSpaceResult second = buildCoverageFreeSpace(problem);

    QCOMPARE(first.error, CoveragePlanningError::None);
    QCOMPARE(first.status, PlanningStatus::Success);
    QCOMPARE(second.status, PlanningStatus::Success);
    comparePolygon(first.freeSpace.coverageTarget.outerBoundary, second.freeSpace.coverageTarget.outerBoundary);
    QCOMPARE(first.freeSpace.coverageTarget.holes.size(), second.freeSpace.coverageTarget.holes.size());
    for (std::size_t index = 0; index < first.freeSpace.coverageTarget.holes.size(); ++index) {
        comparePolygon(first.freeSpace.coverageTarget.holes[index], second.freeSpace.coverageTarget.holes[index]);
    }
    compareRegionSet(first.freeSpace.nominalTrackFeasibleRegion, second.freeSpace.nominalTrackFeasibleRegion);
    compareRegionSet(first.freeSpace.executionTrackFeasibleRegion, second.freeSpace.executionTrackFeasibleRegion);
}

void CoverageFreeSpaceTest::_testMiterPolygonShapes()
{
    const Polygon2D outer = rectangle(0.0, 0.0, 60.0, 60.0);
    const Polygon2D axisAligned = rectangle(20.0, 20.0, 30.0, 30.0);
    const Polygon2D rotated{{{25.0, 18.0}, {32.0, 25.0}, {25.0, 32.0}, {18.0, 25.0}}};
    const Polygon2D convex{{{20.0, 20.0}, {30.0, 20.0}, {33.0, 27.0}, {25.0, 33.0}, {18.0, 27.0}}};
    const Polygon2D concave{{{18.0, 18.0}, {32.0, 18.0}, {32.0, 22.0}, {24.0, 22.0}, {24.0, 30.0}, {18.0, 30.0}}};
    const std::vector<std::vector<Polygon2D>> cases = {
        {axisAligned},
        {rotated},
        {convex},
        {concave},
        {rectangle(12.0, 20.0, 20.0, 28.0), rectangle(35.0, 20.0, 43.0, 28.0)},
    };

    for (const std::vector<Polygon2D>& noGo : cases) {
        CoveragePlanningProblem problem = problemWithOuter(outer);
        problem.swathWidthM = 5.0;
        problem.safetyMarginM = 0.5;
        problem.executionSafety.executionMarginM = 0.25;
        problem.region.noGoRegions = noGo;
        const CoverageFreeSpaceResult result = buildCoverageFreeSpace(problem);
        QCOMPARE(result.status, PlanningStatus::Success);
        QCOMPARE(result.error, CoveragePlanningError::None);
        const Geometry::PolygonRegionContainmentResult contained = Geometry::isRegionSetContained(
            result.freeSpace.executionTrackFeasibleRegion, result.freeSpace.nominalTrackFeasibleRegion);
        QCOMPARE(contained.status, Geometry::PolygonRegionOperationStatus::Success);
        QVERIFY(contained.contained);
        QCOMPARE(result.freeSpace.coverageTarget.holes.size(), noGo.size());
    }
}

void CoverageFreeSpaceTest::_testExecutionMarginFailure()
{
    CoveragePlanningProblem problem = problemWithOuter(rectangle(0.0, 0.0, 20.0, 20.0));
    problem.swathWidthM = 2.0;
    problem.safetyMarginM = 0.5;
    problem.executionSafety.executionMarginM = 0.75;
    const CoverageFreeSpaceResult result = buildCoverageFreeSpace(problem);
    QCOMPARE(result.status, PlanningStatus::Failed);
    QCOMPARE(result.error, CoveragePlanningError::CoverageImpossibleWithExecutionMargin);
    QVERIFY(result.freeSpace.executionTrackFeasibleRegion.empty());
}

void CoverageFreeSpaceTest::_testSharpAngleConservativenessGate()
{
    CoveragePlanningProblem problem = problemWithOuter(rectangle(0.0, 0.0, 50.0, 50.0));
    problem.swathWidthM = 5.0;
    problem.safetyMarginM = 0.5;
    problem.region.noGoRegions = {{{{10.0, 10.0}, {40.0, 10.0}, {25.0, 10.5}}}};
    for (const double executionMarginM : {0.0, 0.25}) {
        problem.executionSafety.executionMarginM = executionMarginM;
        const Geometry::PolygonRegionOperationResult nominal = Geometry::buildTrackFeasibleRegion(
            problem.region.outerBoundary, problem.region.noGoRegions, problem.safetyMarginM);
        const Geometry::PolygonRegionOperationResult execution = Geometry::buildTrackFeasibleRegionConservativeMiter(
            problem.region.outerBoundary, problem.region.noGoRegions,
            problem.safetyMarginM + problem.executionSafety.executionMarginM);
        QCOMPARE(nominal.status, Geometry::PolygonRegionOperationStatus::Success);
        QCOMPARE(execution.status, Geometry::PolygonRegionOperationStatus::Success);
        const Geometry::PolygonRegionContainmentResult contained =
            Geometry::isRegionSetContained(execution.regions, nominal.regions);
        QCOMPARE(contained.status, Geometry::PolygonRegionOperationStatus::Success);

        const CoverageFreeSpaceResult result = buildCoverageFreeSpace(problem);
        if (contained.contained) {
            QCOMPARE(result.status, PlanningStatus::Success);
            QCOMPARE(result.error, CoveragePlanningError::None);
        } else {
            QCOMPARE(result.status, PlanningStatus::Failed);
            QCOMPARE(result.error, CoveragePlanningError::ExecutionRegionNotConservative);
            QVERIFY(result.freeSpace.executionTrackFeasibleRegion.empty());
        }
    }
}

UT_REGISTER_TEST_LIGHTWEIGHT(CoverageFreeSpaceTest, TestLabel::Unit)
