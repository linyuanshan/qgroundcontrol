#include "BoustrophedonCoveragePlannerTest.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include "Geometry/MarineGeometry.h"
#include "Planning/BoustrophedonCoveragePlanner.h"
#include "Planning/CoverageFreeSpace.h"
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
    for (std::size_t index = 1; index < solution.path.size(); ++index) {
        QVERIFY(Geometry::segmentInsidePolygonRegionForValidatedGeometry(
            freeSpace.freeSpace.trackFeasibleRegion, solution.path.at(index - 1), solution.path.at(index)));
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
            input.region.outerBoundary, freeSpace.freeSpace.trackFeasibleRegion, input.swathWidthM);
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

UT_REGISTER_TEST_LIGHTWEIGHT(BoustrophedonCoveragePlannerTest, TestLabel::Unit)
