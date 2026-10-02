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
#include "Planning/BoustrophedonDecomposition.h"
#include "Planning/BoustrophedonCoveragePlanner.h"
#include "Planning/CellCoverage.h"
#include "Planning/ComplexCoverageAssembly.h"
#include "Planning/CoverageGeometry.h"
#include "Planning/CoverageQualityEvaluator.h"
#include "Planning/CoverageSafety.h"
#include "Planning/CoverageTaskAdapter.h"
#include "Planning/CoverageStrategySemantics.h"
#include "Planning/GlobalSweepSelector.h"
#include "Planning/GreedyCellOrdering.h"
#include "Planning/PlanningPathMetrics.h"

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

Polygon2D expandedNavigation(const Polygon2D& coverage, double expansionM)
{
    double minimumX = std::numeric_limits<double>::infinity();
    double minimumY = std::numeric_limits<double>::infinity();
    double maximumX = -std::numeric_limits<double>::infinity();
    double maximumY = -std::numeric_limits<double>::infinity();
    for (const Point2D& point : coverage.vertices) {
        minimumX = std::min(minimumX, point.xM);
        minimumY = std::min(minimumY, point.yM);
        maximumX = std::max(maximumX, point.xM);
        maximumY = std::max(maximumY, point.yM);
    }
    return rectangle(minimumX - expansionM, minimumY - expansionM, maximumX + expansionM, maximumY + expansionM);
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
    problemValue.executionSafety.executionMarginM = 0.0;
    problemValue.region.coverageBoundary = s04Outer();
    problemValue.region.navigationBoundary = expandedNavigation(problemValue.region.coverageBoundary, 10.0);
    problemValue.region.noGoRegions.clear();
    problemValue.swathWidthM = 4.0;
    problemValue.safety.hardSafetyMarginM = 0.0;
    problemValue.safety.preferredSafetyMarginM = problemValue.safety.hardSafetyMarginM;
    problemValue.sweepAngleMode = SweepAngleMode::Auto;

    const BoustrophedonCoveragePlanner planner;
    const CoveragePlanningSolution initial = planner.plan(problemValue);
    if ((initial.status != PlanningStatus::Success) || initial.path.empty()) {
        return {};
    }
    const Point2D offset{-initial.path.front().xM, -initial.path.front().yM};
    translatePolygon(problemValue.region.coverageBoundary, offset);
    translatePolygon(problemValue.region.navigationBoundary, offset);
    for (Polygon2D& noGo : problemValue.region.noGoRegions) {
        translatePolygon(noGo, offset);
    }
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
    result.executionSafety.executionMarginM = 0.0;
    result.region.coverageBoundary = std::move(outer);
    result.region.navigationBoundary = expandedNavigation(result.region.coverageBoundary, safetyMarginM + 2.0);
    result.region.noGoRegions = std::move(noGoRegions);
    result.swathWidthM = swathWidthM;
    result.safety.hardSafetyMarginM = safetyMarginM;
    result.safety.preferredSafetyMarginM = result.safety.hardSafetyMarginM;
    result.sweepAngleMode = SweepAngleMode::Manual;
    result.requestedSweepAngleDeg = angleDeg;
    return result;
}

CoveragePlanningProblem cNProblem(Polygon2D coverage, Polygon2D navigation,
                                  std::vector<Polygon2D> noGoRegions = {}, double swathWidthM = 4.0,
                                  double hardMarginM = 0.0, double preferredMarginM = 0.0,
                                  double executionMarginM = 0.0, double angleDeg = 90.0,
                                  SweepAngleMode angleMode = SweepAngleMode::Manual)
{
    CoveragePlanningProblem result;
    result.region.coverageBoundary = std::move(coverage);
    result.region.navigationBoundary = std::move(navigation);
    result.region.noGoRegions = std::move(noGoRegions);
    result.swathWidthM = swathWidthM;
    result.safety.hardSafetyMarginM = hardMarginM;
    result.safety.preferredSafetyMarginM = preferredMarginM;
    result.executionSafety.executionMarginM = executionMarginM;
    result.sweepAngleMode = angleMode;
    result.requestedSweepAngleDeg = angleDeg;
    return result;
}

double distance(const Point2D& first, const Point2D& second)
{
    return std::hypot(second.xM - first.xM, second.yM - first.yM);
}

void verifySuccess(const CoveragePlanningProblem& input, const CoveragePlanningSolution& solution)
{
    std::string statusEvidence = solution.message;
    if (solution.coverageQuality) {
        statusEvidence += " (quality=" + std::to_string(static_cast<int>(solution.coverageQuality->status)) +
                          ", ratio=" + std::to_string(solution.coverageQuality->coverageRatio) +
                          ", uncovered=" + std::to_string(solution.coverageQuality->uncoveredAreaM2) +
                          ", critical=" + std::to_string(solution.coverageQuality->criticalUncoveredAreaM2) +
                          ", tolerance=" + std::to_string(solution.coverageQuality->numericalToleranceM2) + ")";
    }
    QVERIFY2(solution.status == PlanningStatus::Success, statusEvidence.c_str());
    QCOMPARE(solution.error, CoveragePlanningError::None);
    QVERIFY(solution.path.size() >= 2);
    QCOMPARE(solution.legRoles.size(), solution.path.size() - 1);
    QVERIFY(std::isfinite(solution.coverageLengthM));
    QVERIFY(std::isfinite(solution.transitLengthM));
    QVERIFY(std::isfinite(solution.pathLengthM));
    QVERIFY(solution.coverageLengthM > 0.0);
    QVERIFY(solution.transitLengthM >= 0.0);
    QVERIFY(solution.cellCount >= 1);
    QVERIFY(solution.turnCount >= 0);
    QVERIFY(solution.selectedSweepAngleDeg >= 0.0);
    QVERIFY(solution.selectedSweepAngleDeg < 180.0);
    QVERIFY(solution.coverageQuality.has_value());
    QVERIFY((solution.coverageQuality->status == CoverageQualityStatus::Complete) ||
            (solution.coverageQuality->status == CoverageQualityStatus::Acceptable));
    QVERIFY(solution.coverageQuality->passesRequirement);
    QCOMPARE(solution.coverageQuality->requirement, input.coverageRequirement);
    QCOMPARE(solution.coverageQuality->strategy.strategyId,
             std::string(CoverageStrategySemantics::BoustrophedonId));
    QCOMPARE(solution.coverageQuality->strategy.semanticVersion,
             std::string(CoverageStrategySemantics::BoustrophedonVersion));
    QVERIFY(solution.plannerSource.has_value());
    QCOMPARE(solution.plannerSource->requestedPlannerId,
             std::string(CoverageStrategySemantics::BoustrophedonId));
    QCOMPARE(solution.plannerSource->resolvedStrategy.semanticVersion,
             std::string(CoverageStrategySemantics::BoustrophedonVersion));
    QCOMPARE(solution.plannerSource->resolutionStatus, PlannerResolutionStatus::Resolved);
    QCOMPARE(solution.plannerSource->resolutionReason, PlannerResolutionReason::None);
    QVERIFY(!solution.plannerSource->escalated);

    const CoverageGeometryResult coverageGeometry = buildCoverageGeometry(input.region);
    QCOMPARE(coverageGeometry.error, CoveragePlanningError::None);
    const SafetyTrackRegionsResult safetyRegions =
        buildSafetyTrackRegions(input.region, input.safety, input.executionSafety);
    QCOMPARE(safetyRegions.error, CoveragePlanningError::None);
    const SafetyCandidateAssessment safety = evaluateSafetyCandidate(safetyRegions, solution.path);
    QCOMPARE(safety.error, CoveragePlanningError::None);
    QVERIFY(safety.tier.has_value());
    const std::optional<PlanningPathMetrics> metrics = calculatePlanningPathMetrics(solution.path, solution.legRoles);
    QVERIFY(metrics.has_value());
    QVERIFY(planningPathMetricsMatch(*metrics, solution.coverageLengthM, solution.transitLengthM,
                                     solution.pathLengthM));
    QCOMPARE(solution.coverageLengthM, metrics->coverageLengthM);
    QCOMPARE(solution.transitLengthM, metrics->transitLengthM);
    QCOMPARE(solution.pathLengthM, metrics->pathLengthM);
    QCOMPARE(solution.turnCount, metrics->turnCount);

    for (std::size_t index = 1; index < solution.path.size(); ++index) {
        const Point2D& start = solution.path.at(index - 1);
        const Point2D& end = solution.path.at(index);
        QVERIFY(start.isFinite());
        QVERIFY(end.isFinite());
        const double lengthM = distance(start, end);
        QVERIFY(lengthM > 0.0);
        QVERIFY(Geometry::segmentInsidePolygonRegion(safetyRegions.regions.hardExecutionTrackRegion, start, end));
        if (solution.legRoles.at(index - 1) == PathLegRole::Coverage) {
            QVERIFY(Geometry::segmentInsidePolygonRegion(coverageGeometry.geometry.coverageTarget, start, end));
        }
    }
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
    QCOMPARE(first.plannerSource.has_value(), second.plannerSource.has_value());
    if (first.plannerSource) {
        QCOMPARE(first.plannerSource.value(), second.plannerSource.value());
    }
    QCOMPARE(first.coverageQuality.has_value(), second.coverageQuality.has_value());
    if (first.coverageQuality) {
        QCOMPARE(first.coverageQuality->status, second.coverageQuality->status);
        QCOMPARE(first.coverageQuality->coverageRatio, second.coverageQuality->coverageRatio);
        QCOMPARE(first.coverageQuality->uncoveredAreaM2, second.coverageQuality->uncoveredAreaM2);
        QCOMPARE(first.coverageQuality->criticalUncoveredAreaM2, second.coverageQuality->criticalUncoveredAreaM2);
    }
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
        problem(rectangle(0.0, 0.0, 20.0, 20.0), {rectangle(8.0, 8.0, 12.0, 12.0)}, 4.0, 0.0),
        problem(polygon({{0.0, 0.0}, {30.0, 0.0}, {30.0, 20.0}, {18.0, 20.0}, {18.0, 10.0}, {0.0, 10.0}}),
                {rectangle(22.0, 4.0, 26.0, 8.0)}, 2.0, 0.0),
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
    CoveragePlanningProblem input = problem(rectangle(0.0, 0.0, 20.0, 12.0), {}, 4.0, 0.0);
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
        const CoveragePlanningSolution repeated = planner.plan(input);
        compareSolutions(ninety, repeated);
        QVERIFY(repeated.coverageQuality.has_value());
        QCOMPARE(repeated.coverageQuality->status, ninety.coverageQuality->status);
        QCOMPARE(repeated.coverageQuality->coverageRatio, ninety.coverageQuality->coverageRatio);
        QCOMPARE(repeated.coverageQuality->criticalUncoveredAreaM2, ninety.coverageQuality->criticalUncoveredAreaM2);
    }
}

void BoustrophedonCoveragePlannerTest::_testAutoSelectionAndInputOrder()
{
    const BoustrophedonCoveragePlanner planner;
    std::vector<CoveragePlanningProblem> cases = {
        problem(rectangle(0.0, 0.0, 30.0, 10.0), {}, 4.0, 0.0),
        problem(rectangle(0.0, 0.0, 10.0, 30.0), {}, 4.0, 0.0),
        problem(polygon({{0.0, 0.0}, {30.0, 0.0}, {30.0, 8.0}, {8.0, 8.0}, {8.0, 16.0}, {0.0, 16.0}}),
                {}, 4.0, 0.0),
    };
    constexpr std::array<double, 3> ExpectedAnglesDeg{90.0, 0.0, 90.0};
    for (std::size_t caseIndex = 0; caseIndex < cases.size(); ++caseIndex) {
        CoveragePlanningProblem& input = cases.at(caseIndex);
        input.sweepAngleMode = SweepAngleMode::Auto;
        const CoverageGeometryResult coverageGeometry = buildCoverageGeometry(input.region);
        QCOMPARE(coverageGeometry.error, CoveragePlanningError::None);
        const SafetyTrackRegionsResult safetyRegions =
            buildSafetyTrackRegions(input.region, input.safety, input.executionSafety);
        QCOMPARE(safetyRegions.error, CoveragePlanningError::None);
        const GlobalSweepSelectionResult selection = selectGlobalSweepAngle(
            input.region.coverageBoundary, safetyRegions.regions.hardExecutionTrackRegion, input.swathWidthM);
        QVERIFY2(selection.status == PlanningStatus::Success, selection.message.c_str());
        QVERIFY(angleComesFromOuterEdge(input.region.coverageBoundary, selection.selectedSweepAngleDeg));
        QCOMPARE(selection.selectedSweepAngleDeg, ExpectedAnglesDeg.at(caseIndex));

        const CoveragePlanningSolution first = planner.plan(input);
        const std::string evidence = "auto case " + std::to_string(caseIndex + 1) + ": " + first.message;
        QVERIFY2(first.status == PlanningStatus::Success, evidence.c_str());
        verifySuccess(input, first);
        QCOMPARE(first.selectedSweepAngleDeg, selection.selectedSweepAngleDeg);

        std::ranges::reverse(input.region.coverageBoundary.vertices);
        std::rotate(input.region.coverageBoundary.vertices.begin(),
                    std::next(input.region.coverageBoundary.vertices.begin()),
                    input.region.coverageBoundary.vertices.end());
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
    verifyFailure(planner, problem(outer, {rectangle(5.0, 5.0, 10.0, 10.0), rectangle(8.0, 8.0, 13.0, 13.0)}),
                  PlanningStatus::InvalidInput, CoveragePlanningError::NoGoOverlapOrTouch);
    verifyFailure(planner, cNProblem(rectangle(0.0, 0.0, 4.0, 4.0), rectangle(0.0, 0.0, 4.0, 4.0), {},
                                     2.0, 2.0, 2.0),
                  PlanningStatus::Failed, CoveragePlanningError::NoNavigableArea);
}

void BoustrophedonCoveragePlannerTest::_testExecutionSafeScenarios()
{
    std::vector<CoveragePlanningProblem> cases = {
        problem(polygon({{0.0, 0.0}, {20.0, 0.0}, {20.0, 8.0}, {8.0, 8.0}, {8.0, 20.0}, {0.0, 20.0}})),
        problem(polygon(
            {{0.0, 0.0}, {20.0, 0.0}, {20.0, 6.0}, {6.0, 6.0}, {6.0, 14.0}, {20.0, 14.0}, {20.0, 20.0}, {0.0, 20.0}})),
        problem(rectangle(0.0, 0.0, 20.0, 20.0), {rectangle(8.0, 8.0, 12.0, 12.0)}, 4.0, 0.0),
        problem(polygon({{0.0, 0.0}, {30.0, 0.0}, {30.0, 20.0}, {18.0, 20.0}, {18.0, 10.0}, {0.0, 10.0}}),
                {rectangle(22.0, 4.0, 26.0, 8.0)}, 4.0, 0.0, 90.00014626),
        problem(rectangle(0.0, 0.0, 30.0, 20.0),
                {rectangle(8.0, 8.0, 12.0, 12.0), rectangle(18.0, 8.0, 22.0, 12.0)}, 4.0, 0.0),
    };
    cases[1].sweepAngleMode = SweepAngleMode::Auto;
    const BoustrophedonCoveragePlanner planner;
    for (std::size_t index = 0; index < cases.size(); ++index) {
        CoveragePlanningProblem& input = cases[index];
        input.executionSafety.executionMarginM = ((index == 2) || (index == 4)) ? 0.0 : 0.25;
        const CoveragePlanningSolution result = planner.plan(input);
        const std::string evidence = "execution-safe scenario " + std::to_string(index) + ": " + result.message;
        QVERIFY2(result.status == PlanningStatus::Success, evidence.c_str());
        verifySuccess(input, result);
        compareSolutions(result, planner.plan(input));
    }

    CoveragePlanningProblem autoS04 = cases[3];
    autoS04.sweepAngleMode = SweepAngleMode::Auto;
    const CoveragePlanningSolution autoResult = planner.plan(autoS04);
    verifySuccess(autoS04, autoResult);
    compareSolutions(autoResult, planner.plan(autoS04));
}

void BoustrophedonCoveragePlannerTest::_testCoverageNavigationSeparation()
{
    const BoustrophedonCoveragePlanner planner;
    const Polygon2D navigation = rectangle(-5.0, -5.0, 25.0, 25.0);

    const CoveragePlanningProblem separated =
        cNProblem(rectangle(0.0, 0.0, 20.0, 20.0), navigation, {}, 4.0, 1.0, 1.0, 0.0);
    const CoveragePlanningSolution separatedResult = planner.plan(separated);
    verifySuccess(separated, separatedResult);

    const CoveragePlanningProblem split = cNProblem(
        rectangle(0.0, 0.0, 20.0, 20.0), navigation, {rectangle(9.0, -2.0, 11.0, 22.0)}, 4.0);
    const CoveragePlanningSolution splitResult = planner.plan(split);
    verifySuccess(split, splitResult);
    QVERIFY(splitResult.cellCount >= 2);
    bool transitOutsideTarget = false;
    const CoverageGeometryResult splitGeometry = buildCoverageGeometry(split.region);
    QCOMPARE(splitGeometry.error, CoveragePlanningError::None);
    for (std::size_t leg = 0; leg < splitResult.legRoles.size(); ++leg) {
        if (splitResult.legRoles[leg] == PathLegRole::Transit) {
            transitOutsideTarget |= !Geometry::segmentInsidePolygonRegion(
                splitGeometry.geometry.coverageTarget, splitResult.path[leg], splitResult.path[leg + 1]);
        }
    }
    QVERIFY(transitOutsideTarget);
    compareSolutions(splitResult, planner.plan(split));

    const CoveragePlanningProblem hole = cNProblem(
        rectangle(0.0, 0.0, 20.0, 20.0), navigation, {rectangle(8.0, 8.0, 12.0, 12.0)}, 4.0);
    const CoveragePlanningSolution holeFirst = planner.plan(hole);
    verifySuccess(hole, holeFirst);
    QVERIFY(holeFirst.cellCount > 1);
    compareSolutions(holeFirst, planner.plan(hole));

    const Polygon2D uShape = polygon({{0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {7.0, 10.0},
                                      {7.0, 3.0}, {3.0, 3.0}, {3.0, 10.0}, {0.0, 10.0}});
    const CoveragePlanningProblem nonMonotone = cNProblem(uShape, rectangle(-5.0, -5.0, 15.0, 15.0), {}, 4.0);
    const CoveragePlanningSolution uResult = planner.plan(nonMonotone);
    verifySuccess(nonMonotone, uResult);
    QVERIFY(uResult.cellCount > 1);
}

void BoustrophedonCoveragePlannerTest::_testCoverageFirstCandidateRanking()
{
    const CoveragePlanningProblem input = cNProblem(rectangle(0.0, 0.0, 20.0, 20.0),
                                                    rectangle(0.0, -10.0, 20.0, 30.0), {}, 4.0, 0.0, 3.0, 0.0);
    const CoverageGeometryResult coverageGeometry = buildCoverageGeometry(input.region);
    QCOMPARE(coverageGeometry.error, CoveragePlanningError::None);
    const SafetyTrackRegionsResult safetyRegions =
        buildSafetyTrackRegions(input.region, input.safety, input.executionSafety);
    QCOMPARE(safetyRegions.error, CoveragePlanningError::None);
    const CoverageDecompositionResult decomposition =
        decomposeBoustrophedon(coverageGeometry.geometry.coverageTarget, 90.0);
    QCOMPARE(decomposition.status, PlanningStatus::Success);
    const PlannerStrategyIdentity strategy{.strategyId = CoverageStrategySemantics::BoustrophedonId,
                                           .semanticVersion = CoverageStrategySemantics::BoustrophedonVersion};

    const CellCoverageGenerationResult preferredCoverage = generateCellCoverage(
        decomposition.cells, safetyRegions.regions.preferredExecutionTrackRegion, input.swathWidthM, 90.0);
    QCOMPARE(preferredCoverage.status, PlanningStatus::Success);
    const CellOrderingResult preferredOrdering = orderCellTraversals(
        safetyRegions.regions.preferredExecutionTrackRegion, preferredCoverage.cells, preferredCoverage.traversalStates);
    QCOMPARE(preferredOrdering.status, PlanningStatus::Success);
    const ComplexCoverageAssemblyResult preferredAssembly = assembleComplexCoverage(
        safetyRegions.regions.preferredExecutionTrackRegion, preferredCoverage.cells, preferredOrdering.visits);
    QCOMPARE(preferredAssembly.status, PlanningStatus::Success);
    const SafetyCandidateAssessment preferredSafety = evaluateSafetyCandidate(safetyRegions, preferredAssembly.path);
    QCOMPARE(preferredSafety.error, CoveragePlanningError::None);
    QCOMPARE(preferredSafety.tier, std::optional<SafetySolutionTier>{SafetySolutionTier::D0});
    const CoverageQualityEvaluation preferredQuality = evaluateCoverageQuality(
        coverageGeometry.geometry.coverageTarget, preferredAssembly.path, preferredAssembly.legRoles, input.swathWidthM,
        input.coverageRequirement, strategy);
    QCOMPARE(preferredQuality.status, CoverageQualityStatus::Insufficient);

    const CellCoverageGenerationResult hardCoverage = generateCellCoverage(
        decomposition.cells, safetyRegions.regions.hardExecutionTrackRegion, input.swathWidthM, 90.0);
    QCOMPARE(hardCoverage.status, PlanningStatus::Success);
    const CellOrderingResult hardOrdering = orderCellTraversals(
        safetyRegions.regions.hardExecutionTrackRegion, hardCoverage.cells, hardCoverage.traversalStates);
    QCOMPARE(hardOrdering.status, PlanningStatus::Success);
    const ComplexCoverageAssemblyResult hardAssembly = assembleComplexCoverage(
        safetyRegions.regions.hardExecutionTrackRegion, hardCoverage.cells, hardOrdering.visits);
    QCOMPARE(hardAssembly.status, PlanningStatus::Success);
    const SafetyCandidateAssessment hardSafety = evaluateSafetyCandidate(safetyRegions, hardAssembly.path);
    QCOMPARE(hardSafety.error, CoveragePlanningError::None);
    QCOMPARE(hardSafety.tier, std::optional<SafetySolutionTier>{SafetySolutionTier::D1});
    const CoverageQualityEvaluation hardQuality = evaluateCoverageQuality(
        coverageGeometry.geometry.coverageTarget, hardAssembly.path, hardAssembly.legRoles, input.swathWidthM,
        input.coverageRequirement, strategy);
    QVERIFY((hardQuality.status == CoverageQualityStatus::Complete) ||
            (hardQuality.status == CoverageQualityStatus::Acceptable));
    QVERIFY(hardQuality.passesRequirement);

    const CoveragePlanningSolution result = BoustrophedonCoveragePlanner{}.plan(input);
    verifySuccess(input, result);
    bool pathUsesNonPreferredTrack = false;
    for (std::size_t leg = 0; leg < result.legRoles.size(); ++leg) {
        pathUsesNonPreferredTrack |= !Geometry::segmentInsidePolygonRegion(
            safetyRegions.regions.preferredExecutionTrackRegion, result.path[leg], result.path[leg + 1]);
    }
    QVERIFY(pathUsesNonPreferredTrack);
}

void BoustrophedonCoveragePlannerTest::_testDirectAutoSweepAndFailureSource()
{
    const Polygon2D uShape = polygon({{0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {7.0, 10.0},
                                      {7.0, 3.0}, {3.0, 3.0}, {3.0, 10.0}, {0.0, 10.0}});
    CoveragePlanningProblem automatic =
        cNProblem(uShape, rectangle(-5.0, -5.0, 15.0, 15.0), {}, 4.0, 0.0, 0.0, 0.0, 0.0, SweepAngleMode::Auto);
    const SafetyTrackRegionsResult safetyRegions =
        buildSafetyTrackRegions(automatic.region, automatic.safety, automatic.executionSafety);
    QCOMPARE(safetyRegions.error, CoveragePlanningError::None);
    const GlobalSweepSelectionResult expected = selectGlobalSweepAngle(
        automatic.region.coverageBoundary, safetyRegions.regions.hardExecutionTrackRegion, automatic.swathWidthM);
    QCOMPARE(expected.status, PlanningStatus::Success);
    const BoustrophedonCoveragePlanner planner;
    const CoveragePlanningSolution first = planner.plan(automatic);
    const CoveragePlanningSolution second = planner.plan(automatic);
    verifySuccess(automatic, first);
    compareSolutions(first, second);
    QCOMPARE(first.selectedSweepAngleDeg, expected.selectedSweepAngleDeg);
    QCOMPARE(first.plannerSource->requestedSweepMode, SweepAngleMode::Auto);
    QCOMPARE(first.plannerSource->sweepSemanticVersion, std::string(CoverageStrategySemantics::GlobalSweepVersion));

    CoveragePlanningProblem noHardTrack =
        cNProblem(rectangle(0.0, 0.0, 4.0, 4.0), rectangle(0.0, 0.0, 4.0, 4.0), {}, 2.0, 2.0, 2.0);
    const CoveragePlanningSolution failed = planner.plan(noHardTrack);
    QCOMPARE(failed.status, PlanningStatus::Failed);
    QCOMPARE(failed.error, CoveragePlanningError::NoNavigableArea);
    QVERIFY(failed.path.empty());
    QVERIFY(failed.legRoles.empty());
    QVERIFY(failed.plannerSource.has_value());
    QCOMPARE(failed.plannerSource->requestedPlannerId, std::string(CoverageStrategySemantics::BoustrophedonId));
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
    task.safety.hardSafetyMarginM = localProblem.safety.hardSafetyMarginM;
    task.safety.preferredSafetyMarginM = localProblem.safety.hardSafetyMarginM;
    task.coverage.sweepAngleMode = localProblem.sweepAngleMode;
    task.coverage.sweepAngleDeg = localProblem.requestedSweepAngleDeg;
    task.region.coverageBoundary = toGeoPolygon(localProblem.region.coverageBoundary, *reference);
    task.region.navigationBoundary = toGeoPolygon(localProblem.region.navigationBoundary, *reference);
    for (const Polygon2D& noGo : localProblem.region.noGoRegions) {
        task.region.noGoRegions.push_back(toGeoPolygon(noGo, *reference));
    }

    CoveragePlanningProblem roundTripProblem;
    roundTripProblem.executionSafety.executionMarginM = 0.0;
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

void BoustrophedonCoveragePlannerTest::_testGeoAdapterMarinePlanRoundTripRegression()
{
    MarineTask task;
    task.name = "Harbor inspection";
    task.vehicleId = "usv-01";
    task.planner.plannerId = CoverageStrategySemantics::BoustrophedonId;
    task.coverage.swathWidthM = 20.0;
    task.coverage.coverageRequirement = CoverageRequirement::Standard;
    task.coverage.sweepAngleMode = SweepAngleMode::Manual;
    task.coverage.sweepAngleDeg = 90.0;
    task.safety.hardSafetyMarginM = 0.0;
    task.safety.preferredSafetyMarginM = 0.0;
    task.planner.executionSafety.executionMarginM = 0.25;
    task.region.coverageBoundary.vertices = {
        {47.3977, 8.5455, 0.0}, {47.3977, 8.5465, 0.0}, {47.3987, 8.5465, 0.0}, {47.3987, 8.5455, 0.0}};
    task.region.navigationBoundary = task.region.coverageBoundary;
    task.region.noGoRegions.push_back({.vertices = {{47.39805, 8.54585, 0.0},
                                                    {47.39805, 8.54615, 0.0},
                                                    {47.39835, 8.54615, 0.0},
                                                    {47.39835, 8.54585, 0.0}}});

    CoveragePlanningProblem adaptedProblem;
    std::optional<GeoReference> geoReference;
    CoveragePlanningError adapterError = CoveragePlanningError::None;
    QVERIFY2(CoverageTaskAdapter::buildProblem(task, adaptedProblem, geoReference, adapterError),
             "MarinePlanIntegration fixture did not pass CoverageTaskAdapter::buildProblem");
    QVERIFY(geoReference.has_value());

    const CoveragePlanningSolution solution = BoustrophedonCoveragePlanner{}.plan(adaptedProblem);
    QCOMPARE(solution.status, PlanningStatus::Success);
    QCOMPARE(solution.error, CoveragePlanningError::None);
    QVERIFY(!solution.message.empty());
    QVERIFY(solution.coverageQuality.has_value());
    const CoverageQualityEvaluation& quality = *solution.coverageQuality;
    QCOMPARE(quality.error, CoverageQualityError::None);
    QVERIFY((quality.status == CoverageQualityStatus::Complete) ||
            (quality.status == CoverageQualityStatus::Acceptable));
    QVERIFY(quality.passesRequirement);
    QVERIFY(std::isfinite(quality.coverageRatio));
    QVERIFY(std::isfinite(quality.uncoveredAreaM2));
    QVERIFY(std::isfinite(quality.criticalUncoveredAreaM2));
    QVERIFY(std::isfinite(quality.numericalToleranceM2));
    QVERIFY(solution.plannerSource.has_value());
    QCOMPARE(solution.plannerSource->resolvedStrategy.strategyId,
             std::string(CoverageStrategySemantics::BoustrophedonId));
    QCOMPARE(solution.plannerSource->resolvedStrategy.semanticVersion,
             std::string(CoverageStrategySemantics::BoustrophedonVersion));

    const PlanningResult result = CoverageTaskAdapter::toPlanningResult(solution, *geoReference);
    QCOMPARE(result.status, PlanningStatus::Success);
    QVERIFY(!result.path.empty());
    QCOMPARE(result.legRoles.size(), result.path.size() - 1);

    std::vector<Point2D> localPath;
    localPath.reserve(result.path.size());
    for (const GeoPoint& point : result.path) {
        const std::optional<Point2D> localPoint = geoReference->toLocal(point);
        QVERIFY(localPoint.has_value());
        localPath.push_back(*localPoint);
    }
    const std::optional<PlanningPathMetrics> roundTripMetrics =
        calculatePlanningPathMetrics(localPath, result.legRoles);
    QVERIFY(roundTripMetrics.has_value());
    QVERIFY(planningPathMetricsMatch(*roundTripMetrics, result.coverageLengthM, result.transitLengthM,
                                     result.pathLengthM));
}

UT_REGISTER_TEST_LIGHTWEIGHT(BoustrophedonCoveragePlannerTest, TestLabel::Unit)
