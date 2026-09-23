#include "BoustrophedonCoveragePlannerTest.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Geometry/MarineGeometry.h"
#include "Geometry/GeoReference.h"
#include "Geometry/PolygonRegion.h"
#include "Planning/BoustrophedonCoveragePlanner.h"
#include "Planning/CoverageFreeSpace.h"
#include "Planning/CoverageTaskAdapter.h"
#include "Planning/GlobalSweepSelector.h"
#include "Planning/NominalCoverageValidator.h"

using namespace Marine;

namespace {

Polygon2D polygon(std::initializer_list<Point2D> vertices)
{
    return {std::vector<Point2D>(vertices)};
}

Polygon2D rectangle(double minimumX, double minimumY, double maximumX, double maximumY)
{
    return polygon({{minimumX, minimumY}, {maximumX, minimumY}, {maximumX, maximumY}, {minimumX, maximumY}});
}

void translatePolygon(Polygon2D& polygonValue, const Point2D& offset)
{
    for (Point2D& point : polygonValue.vertices) {
        point.xM += offset.xM;
        point.yM += offset.yM;
    }
}

Polygon2D s04Outer()
{
    return polygon({{0.0, 0.0}, {30.0, 0.0}, {30.0, 20.0}, {18.0, 20.0}, {18.0, 10.0}, {0.0, 10.0}});
}

CoveragePlanningProblem anchoredS04Problem()
{
    CoveragePlanningProblem problemValue;
    problemValue.region.outerBoundary = s04Outer();
    problemValue.region.noGoRegions = {rectangle(22.0, 4.0, 26.0, 8.0)};
    problemValue.swathWidthM = 4.0;
    problemValue.safetyMarginM = 1.0;
    problemValue.sweepAngleMode = SweepAngleMode::Auto;

    const BoustrophedonCoveragePlanner planner;
    const CoveragePlanningSolution initial = planner.plan(problemValue);
    if ((initial.status != PlanningStatus::Success) || initial.path.empty()) {
        return {};
    }
    const Point2D offset{-initial.path.front().xM, -initial.path.front().yM};
    translatePolygon(problemValue.region.outerBoundary, offset);
    for (Polygon2D& noGo : problemValue.region.noGoRegions) {
        translatePolygon(noGo, offset);
    }
    problemValue.executionSafety.executionMarginM = 0.25;
    return problemValue;
}

GeoPolygon toGeoPolygon(const Polygon2D& polygonValue, const GeoReference& reference)
{
    GeoPolygon result;
    result.vertices.reserve(polygonValue.vertices.size());
    for (const Point2D& point : polygonValue.vertices) {
        const std::optional<GeoPoint> geoPoint = reference.toGeo(point);
        if (!geoPoint) {
            return {};
        }
        result.vertices.push_back(*geoPoint);
    }
    return result;
}

CoveragePlanningProblem problem(Polygon2D outer, std::vector<Polygon2D> noGoRegions = {}, double swathWidthM = 4.0,
                                double safetyMarginM = 1.0, double angleDeg = 90.0)
{
    CoveragePlanningProblem result;
    result.region.outerBoundary = std::move(outer);
    result.region.noGoRegions = std::move(noGoRegions);
    result.swathWidthM = swathWidthM;
    result.safetyMarginM = safetyMarginM;
    result.sweepAngleMode = SweepAngleMode::Manual;
    result.requestedSweepAngleDeg = angleDeg;
    return result;
}

double distance(const Point2D& first, const Point2D& second)
{
    return std::hypot(second.xM - first.xM, second.yM - first.yM);
}

void verifySuccess(const CoveragePlanningProblem& input, const CoveragePlanningSolution& solution)
{
    QVERIFY2(solution.status == PlanningStatus::Success, solution.message.c_str());
    QCOMPARE(solution.error, CoveragePlanningError::None);
    QVERIFY(solution.path.size() >= 2);
    QCOMPARE(solution.legRoles.size(), solution.path.size() - 1);
    QVERIFY(std::isfinite(solution.coverageLengthM));
    QVERIFY(std::isfinite(solution.transitLengthM));
    QVERIFY(std::isfinite(solution.pathLengthM));
    QVERIFY(solution.coverageLengthM > 0.0);
    QVERIFY(solution.transitLengthM >= 0.0);
    QVERIFY(std::abs(solution.coverageLengthM + solution.transitLengthM - solution.pathLengthM) <=
            Geometry::LengthEpsilonM);
    QVERIFY(solution.cellCount >= 1);
    QVERIFY(solution.turnCount >= 0);
    QVERIFY(solution.selectedSweepAngleDeg >= 0.0);
    QVERIFY(solution.selectedSweepAngleDeg < 180.0);

    double coverageLengthM = 0.0;
    double transitLengthM = 0.0;
    for (std::size_t index = 1; index < solution.path.size(); ++index) {
        const Point2D& start = solution.path.at(index - 1);
        const Point2D& end = solution.path.at(index);
        QVERIFY(start.isFinite());
        QVERIFY(end.isFinite());
        const double lengthM = distance(start, end);
        QVERIFY(lengthM > 0.0);
        if (solution.legRoles.at(index - 1) == PathLegRole::Coverage) {
            coverageLengthM += lengthM;
        } else {
            QCOMPARE(solution.legRoles.at(index - 1), PathLegRole::Transit);
            transitLengthM += lengthM;
        }
    }
    QVERIFY(std::abs(coverageLengthM - solution.coverageLengthM) <= Geometry::LengthEpsilonM);
    QVERIFY(std::abs(transitLengthM - solution.transitLengthM) <= Geometry::LengthEpsilonM);

    const CoverageFreeSpaceResult freeSpace = buildCoverageFreeSpace(input);
    QVERIFY2(freeSpace.status == PlanningStatus::Success, freeSpace.message.c_str());
    const Geometry::PolygonRegionContainmentResult conservative = Geometry::isRegionSetContained(
        freeSpace.freeSpace.executionTrackFeasibleRegion, freeSpace.freeSpace.nominalTrackFeasibleRegion);
    QCOMPARE(conservative.status, Geometry::PolygonRegionOperationStatus::Success);
    QVERIFY(conservative.contained);
    for (std::size_t index = 1; index < solution.path.size(); ++index) {
        QVERIFY(Geometry::segmentInsidePolygonRegionForValidatedGeometry(
            freeSpace.freeSpace.executionTrackFeasibleRegion, solution.path.at(index - 1), solution.path.at(index)));
    }
    const CoverageCompletenessResult completeness = validateNominalCoverage(
        PolygonRegionSet2D{freeSpace.freeSpace.coverageTarget}, solution.path, solution.legRoles, input.swathWidthM);
    QVERIFY2(completeness.status == PlanningStatus::Success, completeness.message.c_str());
}

void compareSolutions(const CoveragePlanningSolution& first, const CoveragePlanningSolution& second)
{
    QCOMPARE(first.status, second.status);
    QCOMPARE(first.error, second.error);
    QCOMPARE(first.path.size(), second.path.size());
    QCOMPARE(first.legRoles, second.legRoles);
    QCOMPARE(first.coverageLengthM, second.coverageLengthM);
    QCOMPARE(first.transitLengthM, second.transitLengthM);
    QCOMPARE(first.pathLengthM, second.pathLengthM);
    QCOMPARE(first.selectedSweepAngleDeg, second.selectedSweepAngleDeg);
    QCOMPARE(first.cellCount, second.cellCount);
    QCOMPARE(first.turnCount, second.turnCount);
    for (std::size_t index = 0; index < first.path.size(); ++index) {
        QCOMPARE(first.path.at(index).xM, second.path.at(index).xM);
        QCOMPARE(first.path.at(index).yM, second.path.at(index).yM);
    }
}

bool angleComesFromOuterEdge(const Polygon2D& outer, double selectedAngleDeg)
{
    Point2D previous = outer.vertices.back();
    for (const Point2D& current : outer.vertices) {
        const double mathAngleDeg =
            std::atan2(current.yM - previous.yM, current.xM - previous.xM) * 180.0 / std::numbers::pi;
        const double candidate = Geometry::mathAngleToNavigationAngle(mathAngleDeg);
        const double difference = std::abs(candidate - selectedAngleDeg);
        if (std::min(difference, 180.0 - difference) <= Geometry::LengthEpsilonM) {
            return true;
        }
        previous = current;
    }
    return false;
}

void verifyFailure(const BoustrophedonCoveragePlanner& planner, const CoveragePlanningProblem& input,
                   PlanningStatus status, CoveragePlanningError error)
{
    const CoveragePlanningSolution solution = planner.plan(input);
    QCOMPARE(solution.status, status);
    QCOMPARE(solution.error, error);
    QVERIFY(solution.path.empty());
    QVERIFY(solution.legRoles.empty());
    QCOMPARE(solution.pathLengthM, 0.0);
    QVERIFY(!solution.message.empty());
}

}  // namespace

void BoustrophedonCoveragePlannerTest::_testRepresentativeManualCases()
{
    const std::vector<CoveragePlanningProblem> cases = {
        problem(rectangle(0.0, 0.0, 20.0, 12.0)),
        problem(polygon({{0.0, 0.0}, {20.0, 0.0}, {20.0, 8.0}, {8.0, 8.0}, {8.0, 20.0}, {0.0, 20.0}})),
        problem(polygon({{0.0, 0.0},
                         {20.0, 0.0},
                         {20.0, 6.0},
                         {6.0, 6.0},
                         {6.0, 14.0},
                         {20.0, 14.0},
                         {20.0, 20.0},
                         {0.0, 20.0}}),
                {}, 4.0, 1.0, 0.0),
        problem(rectangle(0.0, 0.0, 20.0, 20.0), {rectangle(8.0, 8.0, 12.0, 12.0)}),
        problem(polygon({{0.0, 0.0}, {30.0, 0.0}, {30.0, 20.0}, {18.0, 20.0}, {18.0, 10.0}, {0.0, 10.0}}),
                {rectangle(22.0, 4.0, 26.0, 8.0)}),
        problem(rectangle(0.0, 0.0, 20.0, 20.0), {rectangle(3.0, 3.0, 6.0, 6.0), rectangle(12.0, 12.0, 15.0, 15.0)},
                4.0, 0.0),
    };

    const BoustrophedonCoveragePlanner planner;
    QCOMPARE(planner.id(), std::string("marine.coverage.bcd"));
    QCOMPARE(planner.displayName(), std::string("Boustrophedon Coverage Planner"));
    for (std::size_t caseIndex = 0; caseIndex < cases.size(); ++caseIndex) {
        const CoveragePlanningSolution solution = planner.plan(cases.at(caseIndex));
        const std::string evidence = "representative case " + std::to_string(caseIndex + 1) + ": " + solution.message;
        QVERIFY2(solution.status == PlanningStatus::Success, evidence.c_str());
        verifySuccess(cases.at(caseIndex), solution);
    }
}

void BoustrophedonCoveragePlannerTest::_testManualNormalizationAndDeterminism()
{
    CoveragePlanningProblem input = problem(rectangle(0.0, 0.0, 20.0, 12.0));
    const BoustrophedonCoveragePlanner planner;
    const CoveragePlanningSolution ninety = planner.plan(input);
    verifySuccess(input, ninety);

    input.requestedSweepAngleDeg = 270.0;
    const CoveragePlanningSolution twoSeventy = planner.plan(input);
    input.requestedSweepAngleDeg = -90.0;
    const CoveragePlanningSolution negativeNinety = planner.plan(input);

    QCOMPARE(ninety.selectedSweepAngleDeg, 90.0);
    compareSolutions(ninety, twoSeventy);
    compareSolutions(ninety, negativeNinety);
    for (int repetition = 0; repetition < 3; ++repetition) {
        compareSolutions(ninety, planner.plan(input));
    }
}

void BoustrophedonCoveragePlannerTest::_testAutoSelectionAndInputOrder()
{
    const BoustrophedonCoveragePlanner planner;
    std::vector<CoveragePlanningProblem> cases = {
        problem(rectangle(0.0, 0.0, 30.0, 10.0)),
        problem(polygon({{10.0, 0.0}, {30.0, 20.0}, {22.0, 28.0}, {2.0, 8.0}})),
        problem(polygon({{0.0, 0.0}, {30.0, 0.0}, {30.0, 8.0}, {8.0, 8.0}, {8.0, 16.0}, {0.0, 16.0}})),
    };
    constexpr std::array<double, 3> ExpectedAnglesDeg{90.0, 45.0, 90.0};
    for (std::size_t caseIndex = 0; caseIndex < cases.size(); ++caseIndex) {
        CoveragePlanningProblem& input = cases.at(caseIndex);
        input.sweepAngleMode = SweepAngleMode::Auto;
        const CoverageFreeSpaceResult freeSpace = buildCoverageFreeSpace(input);
        QVERIFY2(freeSpace.status == PlanningStatus::Success, freeSpace.message.c_str());
        const GlobalSweepSelectionResult selection = selectGlobalSweepAngle(
            input.region.outerBoundary, freeSpace.freeSpace.executionTrackFeasibleRegion, input.swathWidthM);
        QVERIFY2(selection.status == PlanningStatus::Success, selection.message.c_str());
        QVERIFY(angleComesFromOuterEdge(input.region.outerBoundary, selection.selectedSweepAngleDeg));
        QCOMPARE(selection.selectedSweepAngleDeg, ExpectedAnglesDeg.at(caseIndex));

        const CoveragePlanningSolution first = planner.plan(input);
        const std::string evidence = "auto case " + std::to_string(caseIndex + 1) + ": " + first.message;
        QVERIFY2(first.status == PlanningStatus::Success, evidence.c_str());
        verifySuccess(input, first);
        QCOMPARE(first.selectedSweepAngleDeg, selection.selectedSweepAngleDeg);

        std::ranges::reverse(input.region.outerBoundary.vertices);
        std::rotate(input.region.outerBoundary.vertices.begin(), std::next(input.region.outerBoundary.vertices.begin()),
                    input.region.outerBoundary.vertices.end());
        compareSolutions(first, planner.plan(input));
    }
    QCOMPARE(cases.front().sweepAngleMode, SweepAngleMode::Auto);
}

void BoustrophedonCoveragePlannerTest::_testFailurePropagation()
{
    const BoustrophedonCoveragePlanner planner;

    verifyFailure(planner, problem(polygon({{0.0, 0.0}, {10.0, 10.0}, {0.0, 10.0}, {10.0, 0.0}})),
                  PlanningStatus::InvalidInput, CoveragePlanningError::InvalidOuterBoundary);

    const Polygon2D outer = rectangle(0.0, 0.0, 20.0, 20.0);
    verifyFailure(planner, problem(outer, {polygon({{8.0, 8.0}, {12.0, 12.0}, {8.0, 12.0}, {12.0, 8.0}})}),
                  PlanningStatus::InvalidInput, CoveragePlanningError::InvalidNoGoRegion);
    verifyFailure(planner, problem(outer, {rectangle(0.0, 5.0, 3.0, 8.0)}), PlanningStatus::InvalidInput,
                  CoveragePlanningError::NoGoBoundaryConflict);
    verifyFailure(planner, problem(outer, {rectangle(5.0, 5.0, 10.0, 10.0), rectangle(8.0, 8.0, 13.0, 13.0)}),
                  PlanningStatus::InvalidInput, CoveragePlanningError::NoGoOverlapOrTouch);
    verifyFailure(planner, problem(outer, {}, 1.9, 1.0), PlanningStatus::Failed,
                  CoveragePlanningError::CoverageImpossibleWithSafetyMargin);
    verifyFailure(planner, problem(rectangle(0.0, 0.0, 4.0, 4.0), {}, 4.0, 2.0), PlanningStatus::Failed,
                  CoveragePlanningError::NoNavigableArea);
    verifyFailure(planner, problem(rectangle(0.0, 0.0, 20.0, 10.0), {rectangle(9.0, 1.5, 11.0, 8.5)}),
                  PlanningStatus::Failed, CoveragePlanningError::DisconnectedFeasibleRegion);
}

void BoustrophedonCoveragePlannerTest::_testExecutionSafeScenarios()
{
    std::vector<CoveragePlanningProblem> cases = {
        problem(polygon({{0.0, 0.0}, {20.0, 0.0}, {20.0, 8.0}, {8.0, 8.0}, {8.0, 20.0}, {0.0, 20.0}})),
        problem(polygon(
            {{0.0, 0.0}, {20.0, 0.0}, {20.0, 6.0}, {6.0, 6.0}, {6.0, 14.0}, {20.0, 14.0}, {20.0, 20.0}, {0.0, 20.0}})),
        problem(rectangle(0.0, 0.0, 20.0, 20.0), {rectangle(8.0, 8.0, 12.0, 12.0)}),
        problem(polygon({{0.0, 0.0}, {30.0, 0.0}, {30.0, 20.0}, {18.0, 20.0}, {18.0, 10.0}, {0.0, 10.0}}),
                {rectangle(22.0, 4.0, 26.0, 8.0)}, 4.0, 1.0, 90.00014626),
        problem(rectangle(0.0, 0.0, 30.0, 20.0), {rectangle(8.0, 8.0, 12.0, 12.0), rectangle(18.0, 8.0, 22.0, 12.0)}),
    };
    cases[1].sweepAngleMode = SweepAngleMode::Auto;
    const BoustrophedonCoveragePlanner planner;
    for (std::size_t index = 0; index < cases.size(); ++index) {
        CoveragePlanningProblem& input = cases[index];
        input.executionSafety.executionMarginM = 0.25;
        const CoveragePlanningSolution result = planner.plan(input);
        const std::string evidence = "execution-safe scenario " + std::to_string(index) + ": " + result.message;
        QVERIFY2(result.status == PlanningStatus::Success, evidence.c_str());
        verifySuccess(input, result);
        compareSolutions(result, planner.plan(input));
        if ((index == 2) || (index == 3)) {
            QCOMPARE(result.path.size(), index == 2 ? std::size_t{27} : std::size_t{31});
            QCOMPARE(result.turnCount, index == 2 ? 25 : 29);
            double shortestLegM = std::numeric_limits<double>::infinity();
            for (std::size_t leg = 1; leg < result.path.size(); ++leg) {
                shortestLegM = std::min(shortestLegM, distance(result.path[leg - 1], result.path[leg]));
            }
            QVERIFY(std::abs(shortestLegM - 1.5) <= Geometry::LengthEpsilonM);
            QVERIFY(shortestLegM >= 0.5);
        }
    }

    CoveragePlanningProblem autoS04 = cases[3];
    autoS04.sweepAngleMode = SweepAngleMode::Auto;
    const CoveragePlanningSolution autoResult = planner.plan(autoS04);
    verifySuccess(autoS04, autoResult);
    compareSolutions(autoResult, planner.plan(autoS04));
}

void BoustrophedonCoveragePlannerTest::_testGeoRoundTripS04Regression()
{
    CoveragePlanningProblem localProblem = anchoredS04Problem();
    QVERIFY(localProblem.region.isFinite());
    const BoustrophedonCoveragePlanner planner;
    const CoveragePlanningSolution localResult = planner.plan(localProblem);
    verifySuccess(localProblem, localResult);

    const std::optional<GeoReference> reference = GeoReference::create({47.3980756, 8.5458749, 0.0});
    QVERIFY(reference.has_value());
    MarineTask task;
    task.name = "P2-13I S04 Geo round-trip";
    task.planner.plannerId = "marine.coverage.bcd";
    task.planner.executionSafety.executionMarginM = localProblem.executionSafety.executionMarginM;
    task.coverage.swathWidthM = localProblem.swathWidthM;
    task.coverage.safetyMarginM = localProblem.safetyMarginM;
    task.coverage.sweepAngleMode = localProblem.sweepAngleMode;
    task.coverage.sweepAngleDeg = localProblem.requestedSweepAngleDeg;
    task.region.outerBoundary = toGeoPolygon(localProblem.region.outerBoundary, *reference);
    for (const Polygon2D& noGo : localProblem.region.noGoRegions) {
        task.region.noGoRegions.push_back(toGeoPolygon(noGo, *reference));
    }

    CoveragePlanningProblem roundTripProblem;
    std::optional<GeoReference> roundTripReference;
    CoveragePlanningError adapterError = CoveragePlanningError::None;
    QVERIFY2(CoverageTaskAdapter::buildProblem(task, roundTripProblem, roundTripReference, adapterError),
             "S04 Geo round-trip adapter failed");
    QVERIFY(roundTripReference.has_value());

    const CoveragePlanningSolution first = planner.plan(roundTripProblem);
    verifySuccess(roundTripProblem, first);
    for (int repetition = 0; repetition < 3; ++repetition) {
        compareSolutions(first, planner.plan(roundTripProblem));
    }
}

UT_REGISTER_TEST_LIGHTWEIGHT(BoustrophedonCoveragePlannerTest, TestLabel::Unit)
