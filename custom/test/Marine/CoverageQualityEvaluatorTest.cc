#include "CoverageQualityEvaluatorTest.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Geometry/GeoReference.h"
#include "Geometry/PolygonRegion.h"
#include "Planning/BoustrophedonDecomposition.h"
#include "Planning/CellCoverage.h"
#include "Planning/ComplexCoverageAssembly.h"
#include "Planning/CoverageGeometry.h"
#include "Planning/CoverageQualityEvaluator.h"
#include "Planning/CoverageQualityPolicy.h"
#include "Planning/CoverageSafety.h"
#include "Planning/CoverageStrategySemantics.h"
#include "Planning/CoverageTaskAdapter.h"
#include "Planning/GreedyCellOrdering.h"

using namespace Marine;

namespace {

PolygonRegion2D rectangle(double minimumX, double minimumY, double maximumX, double maximumY)
{
    return {.outerBoundary = {
                .vertices = {{minimumX, minimumY}, {maximumX, minimumY}, {maximumX, maximumY}, {minimumX, maximumY}}}};
}

std::pair<std::vector<Point2D>, std::vector<PathLegRole>> lanePath(double widthM, const std::vector<double>& lanes)
{
    std::vector<Point2D> path;
    std::vector<PathLegRole> roles;
    if (lanes.empty()) {
        return {path, roles};
    }
    path.push_back({0.0, lanes.front()});
    for (std::size_t index = 0; index < lanes.size(); ++index) {
        const bool leftToRight = index % 2 == 0;
        path.push_back({leftToRight ? widthM : 0.0, lanes[index]});
        roles.push_back(PathLegRole::Coverage);
        if (index + 1 < lanes.size()) {
            path.push_back({leftToRight ? widthM : 0.0, lanes[index + 1]});
            roles.push_back(PathLegRole::Transit);
        }
    }
    return {path, roles};
}

PlannerStrategyIdentity testStrategy();

CoverageQualityEvaluation evaluateRectangle(double widthM, double heightM, double swathM,
                                            const std::vector<double>& lanes,
                                            CoverageRequirement requirement = CoverageRequirement::Standard)
{
    auto [path, roles] = lanePath(widthM, lanes);
    return evaluateCoverageQuality({rectangle(0.0, 0.0, widthM, heightM)}, path, roles, swathM, requirement,
                                   testStrategy());
}

double area(const PolygonRegionSet2D& regions)
{
    const auto result = Geometry::polygonRegionArea(regions);
    return result.status == Geometry::PolygonRegionOperationStatus::Success ? result.areaM2
                                                                            : std::numeric_limits<double>::quiet_NaN();
}

struct GeoPartitionWitness
{
    PolygonRegionSet2D target;
    PolygonRegionSet2D footprint;
    std::vector<Point2D> path;
    std::vector<PathLegRole> legRoles;
    CoverageQualityEvaluation quality;
};

bool buildGeoPartitionWitness(GeoPartitionWitness& witness, std::string& failure)
{
    MarineTask task;
    task.name = "V05-06C Geo partition regression";
    task.vehicleId = "v05-06c-geo-regression";
    task.planner.plannerId = CoverageStrategySemantics::BoustrophedonId;
    task.coverage.swathWidthM = 20.0;
    task.coverage.coverageRequirement = CoverageRequirement::Standard;
    task.coverage.sweepAngleMode = SweepAngleMode::Manual;
    task.coverage.sweepAngleDeg = 90.0;
    task.safety.hardSafetyMarginM = 0.0;
    task.safety.preferredSafetyMarginM = 0.0;
    task.planner.executionSafety.executionMarginM = 0.25;
    task.region.coverageBoundary.vertices = {{47.3977, 8.5455, 0.0}, {47.3977, 8.5465, 0.0},
                                              {47.3987, 8.5465, 0.0}, {47.3987, 8.5455, 0.0}};
    task.region.navigationBoundary = task.region.coverageBoundary;
    task.region.noGoRegions.push_back({.vertices = {{47.39805, 8.54585, 0.0}, {47.39805, 8.54615, 0.0},
                                                    {47.39835, 8.54615, 0.0}, {47.39835, 8.54585, 0.0}}});

    CoveragePlanningProblem problem;
    std::optional<GeoReference> reference;
    CoveragePlanningError adapterError = CoveragePlanningError::None;
    if (!CoverageTaskAdapter::buildProblem(task, problem, reference, adapterError) || !reference) {
        failure = "Geo task adaptation failed, error=" + std::to_string(static_cast<int>(adapterError));
        return false;
    }

    const CoverageGeometryResult coverage = buildCoverageGeometry(problem.region);
    if (coverage.error != CoveragePlanningError::None) {
        failure = "Coverage geometry failed, error=" + std::to_string(static_cast<int>(coverage.error));
        return false;
    }
    const SafetyTrackRegionsResult safety =
        buildSafetyTrackRegions(problem.region, problem.safety, problem.executionSafety);
    if (safety.error != CoveragePlanningError::None) {
        failure = "Safety geometry failed, error=" + std::to_string(static_cast<int>(safety.error));
        return false;
    }

    const CoverageDecompositionResult decomposition = decomposeBoustrophedon(coverage.geometry.coverageTarget, 90.0);
    if (decomposition.status != PlanningStatus::Success) {
        failure = "BCD decomposition failed, error=" + std::to_string(static_cast<int>(decomposition.error));
        return false;
    }
    const CellCoverageGenerationResult generated =
        generateCellCoverage(decomposition.cells, safety.regions.hardExecutionTrackRegion, problem.swathWidthM, 90.0);
    if (generated.status != PlanningStatus::Success) {
        failure = "Cell coverage generation failed, error=" + std::to_string(static_cast<int>(generated.error));
        return false;
    }
    const CellOrderingResult ordered =
        orderCellTraversals(safety.regions.hardExecutionTrackRegion, generated.cells, generated.traversalStates);
    if (ordered.status != PlanningStatus::Success) {
        failure = "Cell ordering failed, error=" + std::to_string(static_cast<int>(ordered.error));
        return false;
    }
    const ComplexCoverageAssemblyResult assembly =
        assembleComplexCoverage(safety.regions.hardExecutionTrackRegion, generated.cells, ordered.visits);
    if (assembly.status != PlanningStatus::Success) {
        failure = "Coverage assembly failed, error=" + std::to_string(static_cast<int>(assembly.error));
        return false;
    }
    if (evaluateSafetyCandidate(safety, assembly.path).error != CoveragePlanningError::None) {
        failure = "Assembled path did not pass safety evaluation";
        return false;
    }

    std::vector<Geometry::LineSegment2D> coverageSegments;
    for (std::size_t index = 0; index < assembly.legRoles.size(); ++index) {
        if (assembly.legRoles[index] == PathLegRole::Coverage) {
            coverageSegments.push_back({assembly.path[index], assembly.path[index + 1]});
        }
    }
    const Geometry::PolygonRegionOperationResult footprint =
        Geometry::bufferLineSegments(coverageSegments, problem.swathWidthM / 2.0);
    if (footprint.status != Geometry::PolygonRegionOperationStatus::Success) {
        failure = "Coverage footprint generation failed";
        return false;
    }

    witness.target = coverage.geometry.coverageTarget;
    witness.footprint = footprint.regions;
    witness.path = assembly.path;
    witness.legRoles = assembly.legRoles;
    witness.quality = evaluateCoverageQuality(
        witness.target, witness.path, witness.legRoles, problem.swathWidthM, problem.coverageRequirement,
        {.strategyId = CoverageStrategySemantics::BoustrophedonId,
         .semanticVersion = CoverageStrategySemantics::BoustrophedonVersion});
    return true;
}

void verifySharedEvaluationTruth(const CoverageQualityEvaluation& standard, const CoverageQualityEvaluation& strict)
{
    QCOMPARE(standard.targetAreaM2, strict.targetAreaM2);
    QCOMPARE(standard.coveredAreaM2, strict.coveredAreaM2);
    QCOMPARE(standard.uncoveredAreaM2, strict.uncoveredAreaM2);
    QCOMPARE(standard.coverageRatio, strict.coverageRatio);
    QCOMPARE(standard.numericalToleranceM2, strict.numericalToleranceM2);
    QCOMPARE(standard.criticalUncoveredAreaM2, strict.criticalUncoveredAreaM2);
    QCOMPARE(area(standard.residual.criticalCoverageCore), area(strict.residual.criticalCoverageCore));
    QCOMPARE(area(standard.residual.uncoveredRegion), area(strict.residual.uncoveredRegion));
    QCOMPARE(area(standard.residual.criticalUncoveredRegion), area(strict.residual.criticalUncoveredRegion));
    QCOMPARE(area(standard.residual.boundaryShortfallRegion), area(strict.residual.boundaryShortfallRegion));
    QCOMPARE(area(standard.residual.strictFallbackTargetComponents),
             area(strict.residual.strictFallbackTargetComponents));
    QCOMPARE(standard.strictFallbackTriggered, strict.strictFallbackTriggered);
    QCOMPARE(standard.wholeTargetStrictFallback, strict.wholeTargetStrictFallback);
    QCOMPARE(standard.residual.strictFallbackTargetComponents.size(),
             strict.residual.strictFallbackTargetComponents.size());
}

PlannerStrategyIdentity testStrategy()
{
    return {.strategyId = "marine.coverage.simple-monotone", .semanticVersion = "simple-monotone.v1"};
}

CoverageQualityEvaluation comparatorValue(double critical, double uncovered)
{
    CoverageQualityEvaluation value;
    value.status = CoverageQualityStatus::Insufficient;
    value.error = CoverageQualityError::None;
    value.policySemanticVersion = CoverageQualityPolicySemanticVersion;
    value.targetAreaM2 = 100.0;
    value.coveredAreaM2 = 100.0 - uncovered;
    value.uncoveredAreaM2 = uncovered;
    value.coverageRatio = value.coveredAreaM2 / value.targetAreaM2;
    value.criticalUncoveredAreaM2 = critical;
    value.numericalToleranceM2 = 0.01;
    return value;
}

}  // namespace

void CoverageQualityEvaluatorTest::_testCompleteStandardAndStrict()
{
    const auto standard = evaluateRectangle(20.0, 10.0, 2.0, {1.0, 3.0, 5.0, 7.0, 9.0});
    const auto strict = evaluateRectangle(20.0, 10.0, 2.0, {1.0, 3.0, 5.0, 7.0, 9.0}, CoverageRequirement::Strict);
    QCOMPARE(standard.status, CoverageQualityStatus::Complete);
    QVERIFY(standard.passesRequirement);
    QCOMPARE(standard.error, CoverageQualityError::None);
    QCOMPARE(strict.status, CoverageQualityStatus::Complete);
    QVERIFY(strict.passesRequirement);
    verifySharedEvaluationTruth(standard, strict);
}

void CoverageQualityEvaluatorTest::_testStrictAndStandardShareCompleteEvaluationTruth()
{
    const auto standard = evaluateRectangle(20.0, 10.0, 2.0, {1.0, 3.0, 5.0, 7.0, 9.0});
    const auto strict = evaluateRectangle(20.0, 10.0, 2.0, {1.0, 3.0, 5.0, 7.0, 9.0}, CoverageRequirement::Strict);
    verifySharedEvaluationTruth(standard, strict);
    QCOMPARE(standard.status, CoverageQualityStatus::Complete);
    QCOMPARE(strict.status, CoverageQualityStatus::Complete);
}

void CoverageQualityEvaluatorTest::_testBoundaryOnlyShortfall()
{
    std::vector<double> lanes;
    for (double y = 1.0; y <= 97.0; y += 2.0) {
        lanes.push_back(y);
    }
    lanes.push_back(98.6);
    const auto standard = evaluateRectangle(100.0, 100.0, 2.0, lanes);
    const auto strict = evaluateRectangle(100.0, 100.0, 2.0, lanes, CoverageRequirement::Strict);
    verifySharedEvaluationTruth(standard, strict);
    QCOMPARE(standard.status, CoverageQualityStatus::Acceptable);
    QVERIFY(standard.passesRequirement);
    QVERIFY(standard.coverageRatio > 0.99);
    QVERIFY(standard.uncoveredAreaM2 > standard.numericalToleranceM2);
    QVERIFY(standard.criticalUncoveredAreaM2 <= standard.numericalToleranceM2);
    QCOMPARE(strict.status, CoverageQualityStatus::Insufficient);
    QVERIFY(!strict.passesRequirement);
}

void CoverageQualityEvaluatorTest::_testInternalCriticalGap()
{
    const std::vector<double> lanes{5.0, 15.0, 25.0, 35.0, 45.0, 55.2, 65.0, 75.0, 85.0, 95.0};
    const auto standard = evaluateRectangle(1000.0, 100.0, 10.0, lanes);
    const auto strict = evaluateRectangle(1000.0, 100.0, 10.0, lanes, CoverageRequirement::Strict);
    verifySharedEvaluationTruth(standard, strict);
    QCOMPARE(standard.status, CoverageQualityStatus::Insufficient);
    QCOMPARE(strict.status, CoverageQualityStatus::Insufficient);
    QVERIFY(standard.coverageRatio > 0.99);
    QVERIFY(standard.criticalUncoveredAreaM2 > standard.numericalToleranceM2);
    QCOMPARE(standard.error, CoverageQualityError::None);
    QVERIFY(!standard.residual.criticalUncoveredRegion.empty());
    QVERIFY(!standard.residual.boundaryShortfallRegion.empty());
    const auto criticalArea = Geometry::polygonRegionArea(standard.residual.criticalUncoveredRegion);
    const auto boundaryArea = Geometry::polygonRegionArea(standard.residual.boundaryShortfallRegion);
    QCOMPARE(criticalArea.status, Geometry::PolygonRegionOperationStatus::Success);
    QCOMPARE(boundaryArea.status, Geometry::PolygonRegionOperationStatus::Success);
    QVERIFY(std::abs(criticalArea.areaM2 - standard.criticalUncoveredAreaM2) <= standard.numericalToleranceM2);
    QVERIFY(criticalArea.areaM2 <= standard.uncoveredAreaM2 + standard.numericalToleranceM2);
    QVERIFY(boundaryArea.areaM2 > 0.0);
    QVERIFY(boundaryArea.areaM2 <= standard.uncoveredAreaM2 + standard.numericalToleranceM2);
}

void CoverageQualityEvaluatorTest::_testStrategyIdentityDoesNotChangeQualityTruth()
{
    const PolygonRegionSet2D target{rectangle(0.0, 0.0, 100.0, 100.0)};
    const std::vector<double> lanes{1.0,  3.0,  5.0,  7.0,  9.0,  11.0, 13.0, 15.0, 17.0, 19.0, 21.0, 23.0, 25.0,
                                    27.0, 29.0, 31.0, 33.0, 35.0, 37.0, 39.0, 41.0, 43.0, 45.0, 47.0, 49.0, 51.0,
                                    53.0, 55.0, 57.0, 59.0, 61.0, 63.0, 65.0, 67.0, 69.0, 71.0, 73.0, 75.0, 77.0,
                                    79.0, 81.0, 83.0, 85.0, 87.0, 89.0, 91.0, 93.0, 95.0, 97.0, 99.0};
    auto [path, roles] = lanePath(100.0, lanes);
    const auto simple =
        evaluateCoverageQuality(target, path, roles, 2.0, CoverageRequirement::Standard, testStrategy());
    const PlannerStrategyIdentity bcd{.strategyId = "marine.coverage.bcd", .semanticVersion = "bcd.pre-v05-06.v1"};
    const auto boustrophedon = evaluateCoverageQuality(target, path, roles, 2.0, CoverageRequirement::Standard, bcd);
    QVERIFY(!(simple.strategy == boustrophedon.strategy));
    verifySharedEvaluationTruth(simple, boustrophedon);
    QCOMPARE(simple.status, boustrophedon.status);
    QCOMPARE(simple.passesRequirement, boustrophedon.passesRequirement);
    QCOMPARE(simple.requirement, boustrophedon.requirement);
    QCOMPARE(simple.policySemanticVersion, boustrophedon.policySemanticVersion);
}

void CoverageQualityEvaluatorTest::_testCoverageRatioGate()
{
    const auto value = evaluateRectangle(20.0, 50.0, 2.0,
                                         {1.4,  3.4,  5.4,  7.4,  9.4,  11.4, 13.4, 15.4, 17.4, 19.4, 21.4, 23.4, 25.4,
                                          27.4, 29.4, 31.4, 33.4, 35.4, 37.4, 39.4, 41.4, 43.4, 45.4, 47.4, 48.6});
    QCOMPARE(value.status, CoverageQualityStatus::Insufficient);
    QVERIFY(value.criticalUncoveredAreaM2 <= value.numericalToleranceM2);
    QVERIFY(value.coverageRatio < 0.99);
}

void CoverageQualityEvaluatorTest::_testTransitDoesNotCover()
{
    const std::vector<Point2D> path{{-5.0, 5.0}, {15.0, 5.0}};
    const std::vector<PathLegRole> roles{PathLegRole::Transit};
    const auto value = evaluateCoverageQuality({rectangle(0.0, 0.0, 10.0, 10.0)}, path, roles, 2.0,
                                               CoverageRequirement::Standard, testStrategy());
    QCOMPARE(value.status, CoverageQualityStatus::Insufficient);
    QCOMPARE(value.coveredAreaM2, 0.0);
    QCOMPARE(value.uncoveredAreaM2, value.targetAreaM2);
    QCOMPARE(value.coverageRatio, 0.0);
    QVERIFY(!value.passesRequirement);
}

void CoverageQualityEvaluatorTest::_testOverlappingFootprintsCountOnce()
{
    const std::vector<Point2D> path{{0.0, 5.0}, {10.0, 5.0}, {0.0, 5.0}};
    const std::vector<PathLegRole> roles{PathLegRole::Coverage, PathLegRole::Coverage};
    const auto value = evaluateCoverageQuality({rectangle(0.0, 0.0, 10.0, 10.0)}, path, roles, 2.0,
                                               CoverageRequirement::Strict, testStrategy());
    QCOMPARE(value.status, CoverageQualityStatus::Insufficient);
    QVERIFY(qAbs(value.coveredAreaM2 - 20.0) < 0.02);
}

void CoverageQualityEvaluatorTest::_testFootprintOutsideTargetIsClipped()
{
    const std::vector<Point2D> path{{-5.0, 5.0}, {15.0, 5.0}};
    const std::vector<PathLegRole> roles{PathLegRole::Coverage};
    const auto value = evaluateCoverageQuality({rectangle(0.0, 0.0, 10.0, 10.0)}, path, roles, 2.0,
                                               CoverageRequirement::Strict, testStrategy());
    QCOMPARE(value.status, CoverageQualityStatus::Insufficient);
    QVERIFY(qAbs(value.coveredAreaM2 - 20.0) < 0.02);
    QVERIFY(value.coveredAreaM2 <= value.targetAreaM2);
}

void CoverageQualityEvaluatorTest::_testHoleCriticalCore()
{
    PolygonRegion2D target = rectangle(0.0, 0.0, 100.0, 100.0);
    Polygon2D hole{.vertices = {{40.0, 40.0}, {40.0, 60.0}, {60.0, 60.0}, {60.0, 40.0}}};
    target.holes.push_back(hole);
    auto [path, roles] = lanePath(
        100.0, {1.0,  3.0,  5.0,  7.0,  9.0,  11.0, 13.0, 15.0, 17.0, 19.0, 21.0, 23.0, 25.0, 27.0, 29.0, 31.0, 33.0,
                35.0, 37.0, 39.0, 41.0, 43.0, 45.0, 47.0, 49.0, 51.0, 53.0, 55.0, 57.0, 59.0, 61.0, 63.0, 65.0, 67.0,
                69.0, 71.0, 73.0, 75.0, 77.0, 79.0, 81.0, 83.0, 85.0, 87.0, 89.0, 91.0, 93.0, 95.0, 97.0, 99.0});
    const auto value =
        evaluateCoverageQuality({target}, path, roles, 2.0, CoverageRequirement::Standard, testStrategy());
    QVERIFY(value.status != CoverageQualityStatus::AssessmentError);
    QVERIFY(!value.residual.criticalCoverageCore.empty());
    QVERIFY(!value.residual.criticalCoverageCore.front().holes.empty());
    const auto& insetHole = value.residual.criticalCoverageCore.front().holes.front();
    double minimumX = std::numeric_limits<double>::infinity();
    for (const Point2D& point : insetHole.vertices) {
        minimumX = std::min(minimumX, point.xM);
    }
    QVERIFY(minimumX <= 39.501);
}

void CoverageQualityEvaluatorTest::_testMultipleComponentsAndFallbacks()
{
    const PolygonRegionSet2D target{rectangle(0.0, 0.0, 10.0, 10.0), rectangle(20.0, 0.0, 30.0, 10.0)};
    const std::vector<Point2D> path{{0.0, 5.0}, {10.0, 5.0}};
    const std::vector<PathLegRole> roles{PathLegRole::Coverage};
    const auto multi =
        evaluateCoverageQuality(target, path, roles, 10.0, CoverageRequirement::Standard, testStrategy());
    QCOMPARE(multi.status, CoverageQualityStatus::Insufficient);
    QCOMPARE(multi.residual.criticalCoverageCore.size(), std::size_t{2});

    const PolygonRegionSet2D tinyTarget{rectangle(0.0, 0.0, 0.4, 0.4)};
    const std::vector<Point2D> tinyPath{{0.0, 0.2}, {0.4, 0.2}};
    const auto fallback =
        evaluateCoverageQuality(tinyTarget, tinyPath, roles, 1.0, CoverageRequirement::Standard, testStrategy());
    QVERIFY(fallback.strictFallbackTriggered);
    QVERIFY(fallback.wholeTargetStrictFallback);
    QCOMPARE(fallback.residual.strictFallbackTargetComponents.size(), std::size_t{1});
}

void CoverageQualityEvaluatorTest::_testTransitOnlyIsReliableInsufficient()
{
    const std::vector<Point2D> path{{-1.0, 5.0}, {11.0, 5.0}};
    const std::vector<PathLegRole> roles{PathLegRole::Transit};
    const auto value = evaluateCoverageQuality({rectangle(0.0, 0.0, 10.0, 10.0)}, path, roles, 2.0,
                                               CoverageRequirement::Standard, testStrategy());
    QCOMPARE(value.status, CoverageQualityStatus::Insufficient);
    QCOMPARE(value.error, CoverageQualityError::None);
    QCOMPARE(value.coveredAreaM2, 0.0);
    QCOMPARE(value.uncoveredAreaM2, 100.0);
    QCOMPARE(value.coverageRatio, 0.0);
}

void CoverageQualityEvaluatorTest::_testMalformedInputs()
{
    const std::vector<Point2D> validPath{{0.0, 5.0}, {10.0, 5.0}};
    const std::vector<PathLegRole> validRoles{PathLegRole::Coverage};
    const PolygonRegionSet2D target{rectangle(0.0, 0.0, 10.0, 10.0)};
    QCOMPARE(
        evaluateCoverageQuality({}, validPath, validRoles, 2.0, CoverageRequirement::Standard, testStrategy()).error,
        CoverageQualityError::InvalidTarget);
    QCOMPARE(evaluateCoverageQuality(target, validPath, validRoles, 0.0, CoverageRequirement::Standard, testStrategy())
                 .error,
             CoverageQualityError::InvalidSwathWidth);
    QCOMPARE(evaluateCoverageQuality(target, validPath, validRoles, 2.0, static_cast<CoverageRequirement>(99),
                                     testStrategy())
                 .error,
             CoverageQualityError::InvalidRequirement);
    const std::vector<Point2D> shortPath{{0.0, 0.0}};
    QCOMPARE(evaluateCoverageQuality(target, shortPath, {}, 2.0, CoverageRequirement::Standard, testStrategy()).error,
             CoverageQualityError::InvalidPath);
    QCOMPARE(evaluateCoverageQuality(target, validPath, {}, 2.0, CoverageRequirement::Standard, testStrategy()).error,
             CoverageQualityError::InvalidPath);
    const std::vector<Point2D> nonFinite{{0.0, 0.0}, {std::numeric_limits<double>::quiet_NaN(), 1.0}};
    QCOMPARE(evaluateCoverageQuality(target, nonFinite, validRoles, 2.0, CoverageRequirement::Standard, testStrategy())
                 .error,
             CoverageQualityError::InvalidPath);
    const std::vector<Point2D> zeroLeg{{1.0, 1.0}, {1.0, 1.0}};
    QCOMPARE(
        evaluateCoverageQuality(target, zeroLeg, validRoles, 2.0, CoverageRequirement::Standard, testStrategy()).error,
        CoverageQualityError::InvalidPath);
    const std::vector<PathLegRole> invalidRole{static_cast<PathLegRole>(99)};
    QCOMPARE(evaluateCoverageQuality(target, validPath, invalidRole, 2.0, CoverageRequirement::Standard, testStrategy())
                 .error,
             CoverageQualityError::InvalidPath);
    const PlannerStrategyIdentity invalidStrategy;
    QCOMPARE(evaluateCoverageQuality(target, validPath, validRoles, 2.0, CoverageRequirement::Standard, invalidStrategy)
                 .error,
             CoverageQualityError::InvalidStrategy);
}

void CoverageQualityEvaluatorTest::_testUnsupportedPolicy()
{
    const std::vector<Point2D> path{{0.0, 5.0}, {10.0, 5.0}};
    const std::vector<PathLegRole> roles{PathLegRole::Coverage};
    const auto value =
        evaluateCoverageQuality({rectangle(0.0, 0.0, 10.0, 10.0)}, path, roles, 2.0, CoverageRequirement::Standard,
                                testStrategy(), "coverage-quality.future");
    QCOMPARE(value.status, CoverageQualityStatus::AssessmentError);
    QCOMPARE(value.error, CoverageQualityError::UnsupportedPolicySemantics);
}

void CoverageQualityEvaluatorTest::_testNumericalToleranceBranches()
{
    const auto floorTolerance = evaluateRectangle(10.0, 10.0, 2.0, {5.0});
    QCOMPARE(floorTolerance.numericalToleranceM2, 0.01);
    const auto relativeTolerance = evaluateRectangle(1000.0, 100.0, 10.0, {5.0});
    QCOMPARE(relativeTolerance.numericalToleranceM2, 0.1);
}

void CoverageQualityEvaluatorTest::_testResidualAreaConsistencyAndDeterminism()
{
    const auto first = evaluateRectangle(20.0, 10.0, 2.0, {1.0, 3.0, 5.0, 7.0});
    const auto second = evaluateRectangle(20.0, 10.0, 2.0, {1.0, 3.0, 5.0, 7.0});
    QVERIFY(std::abs(first.coveredAreaM2 + first.uncoveredAreaM2 - first.targetAreaM2) <= first.numericalToleranceM2);
    QVERIFY(std::abs(area(first.residual.uncoveredRegion) - first.uncoveredAreaM2) <= first.numericalToleranceM2);
    QVERIFY(std::abs(area(first.residual.criticalUncoveredRegion) - first.criticalUncoveredAreaM2) <=
            first.numericalToleranceM2);
    QCOMPARE(first.status, second.status);
    QCOMPARE(first.coveredAreaM2, second.coveredAreaM2);
    QCOMPARE(first.uncoveredAreaM2, second.uncoveredAreaM2);
    QCOMPARE(first.criticalUncoveredAreaM2, second.criticalUncoveredAreaM2);
}

void CoverageQualityEvaluatorTest::_testComparatorOrderingAndEquivalence()
{
    const auto base = comparatorValue(0.04, 2.0);
    QCOMPARE(compareCoverageQuality(comparatorValue(0.02, 5.0), base), CoverageQualityComparison::Better);
    QCOMPARE(compareCoverageQuality(comparatorValue(0.04, 1.0), base), CoverageQualityComparison::Better);
    QCOMPARE(compareCoverageQuality(comparatorValue(0.041, 2.004), base), CoverageQualityComparison::Equivalent);
}

void CoverageQualityEvaluatorTest::_testComparatorErrorAndTransitivity()
{
    auto error = comparatorValue(0.0, 1.0);
    error.status = CoverageQualityStatus::AssessmentError;
    QCOMPARE(compareCoverageQuality(error, comparatorValue(0.0, 2.0)), CoverageQualityComparison::NotComparable);
    const auto a = comparatorValue(0.0, 1.0);
    const auto b = comparatorValue(0.0, 2.0);
    const auto c = comparatorValue(0.0, 3.0);
    QCOMPARE(compareCoverageQuality(a, b), CoverageQualityComparison::Better);
    QCOMPARE(compareCoverageQuality(b, c), CoverageQualityComparison::Better);
    QCOMPARE(compareCoverageQuality(a, c), CoverageQualityComparison::Better);
}

void CoverageQualityEvaluatorTest::_testGeoResidualTruthRegression()
{
    GeoPartitionWitness witness;
    std::string failure;
    QVERIFY2(buildGeoPartitionWitness(witness, failure), failure.c_str());

    const auto targetArea = Geometry::polygonRegionArea(witness.target);
    const auto uncovered = Geometry::differencePolygonRegions(witness.target, witness.footprint);
    QCOMPARE(targetArea.status, Geometry::PolygonRegionOperationStatus::Success);
    QCOMPARE(uncovered.status, Geometry::PolygonRegionOperationStatus::Success);
    QVERIFY(std::isfinite(targetArea.areaM2));
    QVERIFY(targetArea.areaM2 > 0.0);

    const auto uncoveredArea = Geometry::polygonRegionArea(uncovered.regions);
    QCOMPARE(uncoveredArea.status, Geometry::PolygonRegionOperationStatus::Success);
    QVERIFY(std::isfinite(uncoveredArea.areaM2));
    QVERIFY(uncoveredArea.areaM2 >= 0.0);
    QCOMPARE(witness.quality.status, CoverageQualityStatus::Complete);
    QCOMPARE(witness.quality.error, CoverageQualityError::None);
    QVERIFY(witness.quality.passesRequirement);
    QCOMPARE(witness.quality.targetAreaM2, targetArea.areaM2);
    const auto residualArea = Geometry::polygonRegionArea(witness.quality.residual.uncoveredRegion);
    QCOMPARE(residualArea.status, Geometry::PolygonRegionOperationStatus::Success);
    QVERIFY(std::abs(witness.quality.uncoveredAreaM2 - uncoveredArea.areaM2) <= witness.quality.numericalToleranceM2);
    QVERIFY(std::abs(witness.quality.uncoveredAreaM2 - residualArea.areaM2) <= witness.quality.numericalToleranceM2);
    QVERIFY(std::abs(witness.quality.coveredAreaM2 + witness.quality.uncoveredAreaM2 - witness.quality.targetAreaM2) <=
            witness.quality.numericalToleranceM2);
    QVERIFY(witness.quality.uncoveredAreaM2 <= witness.quality.numericalToleranceM2);
    QVERIFY(witness.quality.criticalUncoveredAreaM2 <=
            witness.quality.uncoveredAreaM2 + witness.quality.numericalToleranceM2);
}

UT_REGISTER_TEST_LIGHTWEIGHT(CoverageQualityEvaluatorTest, TestLabel::Unit)
