#include "NominalCoverageValidatorTest.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <set>
#include <string>
#include <vector>

#include "Geometry/MarineGeometry.h"
#include "Geometry/PolygonRegion.h"
#include "Planning/BoundaryCoverageSupport.h"
#include "Planning/BoustrophedonDecomposition.h"
#include "Planning/CellCoverage.h"
#include "Planning/ComplexCoverageAssembly.h"
#include "Planning/CoverageFreeSpace.h"
#include "Planning/GreedyCellOrdering.h"
#include "Planning/NominalCoverageValidator.h"

using namespace Marine;

namespace {

Polygon2D rectangle(double minimumX, double minimumY, double maximumX, double maximumY)
{
    return {{{minimumX, minimumY}, {maximumX, minimumY}, {maximumX, maximumY}, {minimumX, maximumY}}};
}

PolygonRegionSet2D regionSet(Polygon2D outer, std::vector<Polygon2D> holes = {})
{
    return {{.outerBoundary = std::move(outer), .holes = std::move(holes)}};
}

struct RolePath
{
    std::vector<Point2D> points;
    std::vector<PathLegRole> roles;
};

RolePath horizontalLanes(double minimumX, double maximumX, std::span<const double> laneYs)
{
    RolePath result;
    bool leftToRight = true;
    for (const double laneY : laneYs) {
        const Point2D laneStart = leftToRight ? Point2D{minimumX, laneY} : Point2D{maximumX, laneY};
        const Point2D laneEnd = leftToRight ? Point2D{maximumX, laneY} : Point2D{minimumX, laneY};
        if (!result.points.empty()) {
            result.points.push_back(laneStart);
            result.roles.push_back(PathLegRole::Transit);
        } else {
            result.points.push_back(laneStart);
        }
        result.points.push_back(laneEnd);
        result.roles.push_back(PathLegRole::Coverage);
        leftToRight = !leftToRight;
    }
    return result;
}

RolePath combineCellCoverage(std::span<const CellCoverage> cells)
{
    RolePath result;
    for (const CellCoverage& cell : cells) {
        if (result.points.empty()) {
            result.points = cell.path;
            result.roles = cell.legRoles;
            continue;
        }
        result.roles.push_back(PathLegRole::Transit);
        result.points.push_back(cell.path.front());
        result.points.insert(result.points.end(), std::next(cell.path.begin()), cell.path.end());
        result.roles.insert(result.roles.end(), cell.legRoles.begin(), cell.legRoles.end());
    }
    return result;
}

std::vector<Geometry::LineSegment2D> boundarySegments(std::span<const BoundaryCoverageComponent> components)
{
    std::vector<Geometry::LineSegment2D> segments;
    for (const BoundaryCoverageComponent& component : components) {
        for (std::size_t index = 1; index < component.path.size(); ++index) {
            segments.push_back({.start = component.path.at(index - 1), .end = component.path.at(index)});
        }
    }
    return segments;
}

bool pointsEqual(const Point2D& first, const Point2D& second)
{
    return (first.xM == second.xM) && (first.yM == second.yM);
}

void markBoundaryLegsTransit(const std::vector<BoundaryCoverageComponent>& components,
                             const std::vector<Point2D>& assembledPath, std::vector<PathLegRole>& assembledRoles)
{
    std::size_t searchStart = 0;
    for (const BoundaryCoverageComponent& component : components) {
        bool found = false;
        for (std::size_t candidate = searchStart; candidate + component.path.size() <= assembledPath.size();
             ++candidate) {
            bool matches = true;
            for (std::size_t pointIndex = 0; pointIndex < component.path.size(); ++pointIndex) {
                if (!pointsEqual(assembledPath.at(candidate + pointIndex), component.path.at(pointIndex))) {
                    matches = false;
                    break;
                }
            }
            if (!matches) {
                continue;
            }
            std::fill_n(assembledRoles.begin() + static_cast<std::ptrdiff_t>(candidate), component.legRoles.size(),
                        PathLegRole::Transit);
            searchStart = candidate + component.path.size();
            found = true;
            break;
        }
        QVERIFY(found);
    }
}

std::string coverageLaneEvidence(std::span<const CellCoverage> cells, double navigationAngleDeg)
{
    const double mathAngleDeg = Geometry::navigationAngleToMathAngle(navigationAngleDeg);
    std::vector<double> lanePositions;
    for (const CellCoverage& cell : cells) {
        for (std::size_t index = 0; index < cell.legRoles.size(); ++index) {
            if (cell.legRoles.at(index) != PathLegRole::Coverage) {
                continue;
            }
            const Point2D start = Geometry::toSweepFrame(cell.path.at(index), mathAngleDeg);
            const Point2D end = Geometry::toSweepFrame(cell.path.at(index + 1), mathAngleDeg);
            lanePositions.push_back((start.yM + end.yM) / 2.0);
        }
    }
    std::ranges::sort(lanePositions);
    const auto duplicates = std::ranges::unique(lanePositions, [](double first, double second) {
        return std::abs(first - second) <= Geometry::LengthEpsilonM;
    });
    lanePositions.erase(duplicates.begin(), duplicates.end());

    std::string evidence = " lanes=";
    for (const double lanePosition : lanePositions) {
        if (evidence.back() != '=') {
            evidence += ',';
        }
        evidence += std::to_string(lanePosition);
    }
    return evidence;
}

void verifySuccess(const CoverageCompletenessResult& result)
{
    QVERIFY2(result.status == PlanningStatus::Success, result.message.c_str());
    QCOMPARE(result.error, CoveragePlanningError::None);
    QVERIFY(result.coverageTargetAreaM2 > 0.0);
    QVERIFY(result.uncoveredAreaM2 <= result.toleranceM2);
    QVERIFY(std::abs(result.coverageTargetAreaM2 - result.coveredTargetAreaM2 - result.uncoveredAreaM2) <= 1e-9);
}

void verifyIncomplete(const CoverageCompletenessResult& result)
{
    QCOMPARE(result.status, PlanningStatus::Failed);
    QCOMPARE(result.error, CoveragePlanningError::CoverageIncomplete);
    QVERIFY(result.uncoveredAreaM2 > result.toleranceM2);
}

void compareResults(const CoverageCompletenessResult& first, const CoverageCompletenessResult& second)
{
    QCOMPARE(first.status, second.status);
    QCOMPARE(first.coverageTargetAreaM2, second.coverageTargetAreaM2);
    QCOMPARE(first.coveredTargetAreaM2, second.coveredTargetAreaM2);
    QCOMPARE(first.uncoveredAreaM2, second.uncoveredAreaM2);
    QCOMPARE(first.toleranceM2, second.toleranceM2);
    QCOMPARE(first.error, second.error);
    QCOMPARE(first.message, second.message);
}

}  // namespace

void NominalCoverageValidatorTest::_testRectangleAndUncoveredStrip()
{
    const PolygonRegionSet2D target = regionSet(rectangle(0.0, 0.0, 10.0, 10.0));
    const std::vector<double> completeYs{2.0, 6.0, 8.0};
    const RolePath complete = horizontalLanes(0.0, 10.0, completeYs);
    verifySuccess(validateNominalCoverage(target, complete.points, complete.roles, 4.0));

    const std::vector<double> incompleteYs{2.0, 8.0};
    const RolePath incomplete = horizontalLanes(0.0, 10.0, incompleteYs);
    verifyIncomplete(validateNominalCoverage(target, incomplete.points, incomplete.roles, 4.0));
}

void NominalCoverageValidatorTest::_testCoverageRolesAndDisconnectedLegs()
{
    const PolygonRegionSet2D target = regionSet(rectangle(0.0, 0.0, 2.0, 10.0));
    const std::vector<Point2D> path{{0.0, 2.0}, {2.0, 2.0}, {2.0, 8.0}, {0.0, 8.0}};
    const std::vector<PathLegRole> transitConnector{PathLegRole::Coverage, PathLegRole::Transit, PathLegRole::Coverage};
    const CoverageCompletenessResult withoutTransitCredit =
        validateNominalCoverage(target, path, transitConnector, 4.0);
    verifyIncomplete(withoutTransitCredit);

    const std::vector<PathLegRole> coverageConnector{PathLegRole::Coverage, PathLegRole::Coverage,
                                                     PathLegRole::Coverage};
    verifySuccess(validateNominalCoverage(target, path, coverageConnector, 4.0));
}

void NominalCoverageValidatorTest::_testRoundCapsAndAreaTolerance()
{
    const PolygonRegionSet2D capTarget = regionSet(rectangle(0.0, -0.5, 10.0, 0.5));
    const std::vector<Point2D> capPath{{0.5, 0.0}, {9.5, 0.0}};
    const std::vector<PathLegRole> oneCoverage{PathLegRole::Coverage};
    verifySuccess(validateNominalCoverage(capTarget, capPath, oneCoverage, 2.0));

    const PolygonRegionSet2D toleranceTarget = regionSet(rectangle(0.0, 0.0, 10.0, 10.0));
    const std::vector<double> withinToleranceYs{1.0, 3.0, 5.0, 7.0, 8.999};
    const RolePath withinTolerance = horizontalLanes(0.0, 10.0, withinToleranceYs);
    const CoverageCompletenessResult accepted =
        validateNominalCoverage(toleranceTarget, withinTolerance.points, withinTolerance.roles, 2.0);
    QVERIFY(accepted.uncoveredAreaM2 <= accepted.toleranceM2);
    verifySuccess(accepted);

    const std::vector<double> beyondToleranceYs{1.0, 3.0, 5.0, 7.0, 8.998};
    const RolePath beyondTolerance = horizontalLanes(0.0, 10.0, beyondToleranceYs);
    verifyIncomplete(validateNominalCoverage(toleranceTarget, beyondTolerance.points, beyondTolerance.roles, 2.0));
}

void NominalCoverageValidatorTest::_testNoGoTargetSemantics()
{
    const PolygonRegionSet2D targetWithHole =
        regionSet(rectangle(0.0, 0.0, 20.0, 20.0), {rectangle(8.0, 8.0, 12.0, 12.0)});
    const Geometry::PolygonRegionAreaResult targetArea = Geometry::polygonRegionArea(targetWithHole);
    QCOMPARE(targetArea.status, Geometry::PolygonRegionOperationStatus::Success);
    QCOMPARE(targetArea.areaM2, 384.0);

    const std::vector<Point2D> legalBoundaryPath{{1.0, 7.0}, {7.0, 7.0}, {7.0, 13.0}};
    const std::vector<PathLegRole> coverageRoles{PathLegRole::Coverage, PathLegRole::Coverage};
    const CoverageCompletenessResult boundaryCoverage =
        validateNominalCoverage(targetWithHole, legalBoundaryPath, coverageRoles, 4.0);
    verifyIncomplete(boundaryCoverage);
    QVERIFY(boundaryCoverage.coveredTargetAreaM2 > 0.0);

    const std::vector<Point2D> insideHole{{9.0, 10.0}, {11.0, 10.0}};
    const std::vector<PathLegRole> oneCoverage{PathLegRole::Coverage};
    const CoverageCompletenessResult noGoExcluded =
        validateNominalCoverage(targetWithHole, insideHole, oneCoverage, 4.0);
    QCOMPARE(noGoExcluded.coverageTargetAreaM2, 384.0);
    QVERIFY(noGoExcluded.coveredTargetAreaM2 < 7.0);
}

void NominalCoverageValidatorTest::_testMalformedInputAndDeterminism()
{
    PolygonRegionSet2D target = regionSet(rectangle(0.0, 0.0, 10.0, 10.0));
    const std::vector<double> laneYs{2.0, 6.0, 8.0};
    const RolePath coverage = horizontalLanes(0.0, 10.0, laneYs);
    const CoverageCompletenessResult first = validateNominalCoverage(target, coverage.points, coverage.roles, 4.0);
    for (int repetition = 0; repetition < 10; ++repetition) {
        compareResults(first, validateNominalCoverage(target, coverage.points, coverage.roles, 4.0));
    }

    std::ranges::reverse(target.front().outerBoundary.vertices);
    compareResults(first, validateNominalCoverage(target, coverage.points, coverage.roles, 4.0));

    QCOMPARE(validateNominalCoverage({}, coverage.points, coverage.roles, 4.0).error,
             CoveragePlanningError::InvalidGeneratedPath);
    QCOMPARE(validateNominalCoverage(target, coverage.points, coverage.roles, 0.0).error,
             CoveragePlanningError::InvalidSwathWidth);
    QCOMPARE(validateNominalCoverage(target, std::span<const Point2D>{}, coverage.roles, 4.0).error,
             CoveragePlanningError::InvalidGeneratedPath);
    QCOMPARE(validateNominalCoverage(target, coverage.points, std::span<const PathLegRole>{}, 4.0).error,
             CoveragePlanningError::InvalidGeneratedPath);

    std::vector<Point2D> nonFinite = coverage.points;
    nonFinite.front().xM = std::numeric_limits<double>::infinity();
    QCOMPARE(validateNominalCoverage(target, nonFinite, coverage.roles, 4.0).error,
             CoveragePlanningError::InvalidGeneratedPath);

    const std::vector<Point2D> zeroLength{{1.0, 1.0}, {1.0, 1.0}};
    const std::vector<PathLegRole> oneCoverage{PathLegRole::Coverage};
    QCOMPARE(validateNominalCoverage(target, zeroLength, oneCoverage, 4.0).error,
             CoveragePlanningError::InvalidGeneratedPath);

    std::vector<PathLegRole> invalidRole{static_cast<PathLegRole>(42)};
    QCOMPARE(validateNominalCoverage(target, zeroLength, invalidRole, 4.0).error,
             CoveragePlanningError::InvalidGeneratedPath);

    const std::vector<PathLegRole> transitOnly{PathLegRole::Transit};
    const CoverageCompletenessResult noCoverage = validateNominalCoverage(target, zeroLength, transitOnly, 4.0);
    verifyIncomplete(noCoverage);
}

void NominalCoverageValidatorTest::_testBackendMinimumAndZeroLengthCoverageLegs()
{
    const PolygonRegionSet2D target = regionSet(rectangle(-5.0, -5.0, 5.0, 5.0));
    constexpr double BackendMinimumLengthM = 1.0 / Geometry::CoordinateScalePerM;
    const std::vector<PathLegRole> oneCoverage{PathLegRole::Coverage};
    const std::vector<Point2D> backendMinimum{{0.0, 0.0}, {BackendMinimumLengthM, 0.0}};
    const double backendMinimumLengthM = std::hypot(backendMinimum.back().xM - backendMinimum.front().xM,
                                                    backendMinimum.back().yM - backendMinimum.front().yM);
    QVERIFY(backendMinimumLengthM > 0.0);
    QVERIFY(backendMinimumLengthM <= Geometry::LengthEpsilonM);
    verifySuccess(validateNominalCoverage(target, backendMinimum, oneCoverage, 20.0));

    const std::vector<Point2D> zeroLength{{0.0, 0.0}, {0.0, 0.0}};
    const CoverageCompletenessResult zeroCoverage = validateNominalCoverage(target, zeroLength, oneCoverage, 20.0);
    QCOMPARE(zeroCoverage.status, PlanningStatus::Failed);
    QCOMPARE(zeroCoverage.error, CoveragePlanningError::InvalidGeneratedPath);
    QCOMPARE(zeroCoverage.message, std::string("Coverage path contains a zero-length coverage leg"));

    const std::vector<PathLegRole> transitOnly{PathLegRole::Transit};
    verifyIncomplete(validateNominalCoverage(target, zeroLength, transitOnly, 20.0));
}

void NominalCoverageValidatorTest::_testArtificialGapRegression()
{
    const PolygonRegionSet2D target = regionSet(rectangle(0.0, 0.0, 10.0, 12.0));
    const std::vector<double> gappedYs{2.0, 8.1, 10.0};
    const RolePath gapped = horizontalLanes(0.0, 10.0, gappedYs);
    verifyIncomplete(validateNominalCoverage(target, gapped.points, gapped.roles, 4.0));

    const std::vector<double> sharedScheduleYs{2.0, 6.0, 10.0};
    const RolePath sharedSchedule = horizontalLanes(0.0, 10.0, sharedScheduleYs);
    verifySuccess(validateNominalCoverage(target, sharedSchedule.points, sharedSchedule.roles, 4.0));
}

void NominalCoverageValidatorTest::_testP2CVerticalSlice()
{
    CoveragePlanningProblem problem;
    problem.region.outerBoundary = rectangle(0.0, 0.0, 20.0, 20.0);
    problem.region.noGoRegions = {rectangle(8.0, 8.0, 12.0, 12.0)};
    problem.swathWidthM = 4.0;
    problem.safetyMarginM = 1.0;
    problem.sweepAngleMode = SweepAngleMode::Manual;
    problem.requestedSweepAngleDeg = 90.0;

    const CoverageFreeSpaceResult freeSpace = buildCoverageFreeSpace(problem);
    QVERIFY2(freeSpace.status == PlanningStatus::Success, freeSpace.message.c_str());
    const CoverageDecompositionResult decomposition =
        decomposeBoustrophedon(freeSpace.freeSpace.executionTrackFeasibleRegion, problem.requestedSweepAngleDeg);
    QVERIFY2(decomposition.status == PlanningStatus::Success, decomposition.message.c_str());
    const CellCoverageGenerationResult generated =
        generateCellCoverage(decomposition.cells, problem.swathWidthM, problem.requestedSweepAngleDeg);
    QVERIFY2(generated.status == PlanningStatus::Success, generated.message.c_str());

    const PolygonRegionSet2D coverageTarget{freeSpace.freeSpace.coverageTarget};
    const RolePath combinedCoverage = combineCellCoverage(generated.cells);
    const CoverageCompletenessResult preOrderCompleteness =
        validateNominalCoverage(coverageTarget, combinedCoverage.points, combinedCoverage.roles, problem.swathWidthM);
    const std::string preOrderEvidence = "target=" + std::to_string(preOrderCompleteness.coverageTargetAreaM2) +
                                         " covered=" + std::to_string(preOrderCompleteness.coveredTargetAreaM2) +
                                         " uncovered=" + std::to_string(preOrderCompleteness.uncoveredAreaM2) +
                                         " tolerance=" + std::to_string(preOrderCompleteness.toleranceM2) +
                                         coverageLaneEvidence(generated.cells, problem.requestedSweepAngleDeg);
    QVERIFY2(preOrderCompleteness.status == PlanningStatus::Failed, preOrderEvidence.c_str());
    QCOMPARE(preOrderCompleteness.error, CoveragePlanningError::CoverageIncomplete);
    QVERIFY(preOrderCompleteness.uncoveredAreaM2 > preOrderCompleteness.toleranceM2);

    const BoundaryCoverageSupportResult boundarySupport =
        generateBoundaryCoverageSupport(freeSpace.freeSpace.executionTrackFeasibleRegion);
    QVERIFY2(boundarySupport.status == PlanningStatus::Success, boundarySupport.message.c_str());
    QCOMPARE(boundarySupport.components.size(), std::size_t{2});

    const Geometry::PolygonRegionOperationResult boundaryFootprint =
        Geometry::bufferLineSegments(boundarySegments(boundarySupport.components), problem.swathWidthM / 2.0);
    QCOMPARE(boundaryFootprint.status, Geometry::PolygonRegionOperationStatus::Success);
    for (const Point2D point : std::vector<Point2D>{{0.0, 0.0}, {10.0, 0.0}, {7.9, 10.0}, {10.0, 7.9}}) {
        QVERIFY(Geometry::pointInsidePolygonRegion(boundaryFootprint.regions, point));
    }

    const CellOrderingResult ordered = orderCellTraversals(freeSpace.freeSpace.executionTrackFeasibleRegion,
                                                           generated.cells, generated.traversalStates);
    QVERIFY2(ordered.status == PlanningStatus::Success, ordered.message.c_str());
    const ComplexCoverageAssemblyResult assembly = assembleComplexCoverage(
        freeSpace.freeSpace.executionTrackFeasibleRegion, boundarySupport.components, generated.cells, ordered.visits);
    QVERIFY2(assembly.status == PlanningStatus::Success, assembly.message.c_str());

    std::set<CoverageCellId> visited;
    for (const OrderedCellTraversal& visit : ordered.visits) {
        QVERIFY(visited.insert(visit.state.cellId).second);
    }
    QCOMPARE(visited.size(), decomposition.cells.size());

    const CoverageCompletenessResult completeness =
        validateNominalCoverage(coverageTarget, assembly.path, assembly.legRoles, problem.swathWidthM);
    const std::string evidence = "target=" + std::to_string(completeness.coverageTargetAreaM2) +
                                 " covered=" + std::to_string(completeness.coveredTargetAreaM2) +
                                 " uncovered=" + std::to_string(completeness.uncoveredAreaM2) +
                                 " tolerance=" + std::to_string(completeness.toleranceM2);
    QVERIFY2(completeness.status == PlanningStatus::Success, evidence.c_str());
    QVERIFY(completeness.uncoveredAreaM2 <= 0.01);

    std::vector<PathLegRole> boundaryAsTransit = assembly.legRoles;
    markBoundaryLegsTransit(boundarySupport.components, assembly.path, boundaryAsTransit);
    const CoverageCompletenessResult withoutBoundaryCredit =
        validateNominalCoverage(coverageTarget, assembly.path, boundaryAsTransit, problem.swathWidthM);
    verifyIncomplete(withoutBoundaryCredit);
}

void NominalCoverageValidatorTest::_testHalfSwathSafetyPrecheck()
{
    CoveragePlanningProblem problem;
    problem.region.outerBoundary = rectangle(0.0, 0.0, 20.0, 20.0);
    problem.swathWidthM = 4.0;
    problem.safetyMarginM = problem.swathWidthM / 2.0;
    problem.sweepAngleMode = SweepAngleMode::Manual;
    problem.requestedSweepAngleDeg = 90.0;

    const CoverageFreeSpaceResult freeSpace = buildCoverageFreeSpace(problem);
    QCOMPARE(freeSpace.status, PlanningStatus::Failed);
    QCOMPARE(freeSpace.error, CoveragePlanningError::CoverageImpossibleWithSafetyMargin);
    QVERIFY(freeSpace.freeSpace.executionTrackFeasibleRegion.empty());
}

UT_REGISTER_TEST_LIGHTWEIGHT(NominalCoverageValidatorTest, TestLabel::Unit)
