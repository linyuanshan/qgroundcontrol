#include "CoverageRepairTest.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

#include "AutoCoveragePlanner.h"
#include "BoustrophedonCoveragePlanner.h"
#include "CoverageGeometry.h"
#include "CoverageRepair.h"
#include "CoverageRepairSupport.h"
#include "CoverageStrategySemantics.h"
#include "Geometry/MarineGeometry.h"
#include "SimpleMonotoneCoveragePlanner.h"
#include "StaticSafeRouter.h"

using namespace Marine;

namespace {

Polygon2D rectangle(double left, double bottom, double right, double top)
{
    return {.vertices = {{left, bottom}, {right, bottom}, {right, top}, {left, top}}};
}

const PlannerStrategyIdentity strategy{.strategyId = CoverageStrategySemantics::SimpleMonotoneId,
                                       .semanticVersion = CoverageStrategySemantics::SimpleMonotoneVersion};

SafetyTrackRegionsResult safetyFor(const PolygonRegionSet2D& active)
{
    return {.error = CoveragePlanningError::None, .regions = {active, active, active}};
}

CoveragePlanningProblem problemFor(double hard = 0, double preferred = 0, double width = 20)
{
    CoveragePlanningProblem problem;
    problem.region.coverageBoundary = rectangle(0, 0, width, width);
    problem.region.navigationBoundary = problem.region.coverageBoundary;
    problem.safety.hardSafetyMarginM = hard;
    problem.safety.preferredSafetyMarginM = preferred;
    problem.executionSafety.executionMarginM = 0;
    problem.swathWidthM = 5;
    problem.coverageRequirement = CoverageRequirement::Strict;
    problem.sweepAngleMode = SweepAngleMode::Manual;
    problem.requestedSweepAngleDeg = 90;
    return problem;
}

CoverageRepairCandidate candidateFor(const PolygonRegionSet2D& target, std::vector<Point2D> path, double swath = 2.0,
                                     CoverageRequirement requirement = CoverageRequirement::Strict)
{
    CoverageRepairCandidate candidate;
    candidate.path = std::move(path);
    candidate.legRoles.assign(candidate.path.size() - 1, PathLegRole::Coverage);
    candidate.metrics = *calculatePlanningPathMetrics(candidate.path, candidate.legRoles);
    candidate.quality =
        evaluateCoverageQuality(target, candidate.path, candidate.legRoles, swath, requirement, strategy);
    return candidate;
}

void verifySteps(const CoverageRepairResult& result, const SafetyTrackRegionsResult& safety)
{
    QVERIFY(result.steps.size() <= result.availableComponentCount);
    std::set<std::uint32_t> used;
    for (const auto& step : result.steps) {
        QVERIFY(used.insert(step.componentId).second);
        QCOMPARE(compareCoverageQuality(step.after.quality, step.before), CoverageQualityComparison::Better);
        QCOMPARE(evaluateSafetyCandidate(safety, step.after.path).error, CoveragePlanningError::None);
        const auto metrics = calculatePlanningPathMetrics(step.after.path, step.after.legRoles);
        QVERIFY(metrics.has_value());
        QCOMPARE(metrics->pathLengthM, step.after.metrics.pathLengthM);
        QCOMPARE(metrics->turnCount, step.after.metrics.turnCount);
        QVERIFY(!coveragePolicyPass(step.before));
    }
}

void compareCandidates(const CoverageRepairCandidate& first, const CoverageRepairCandidate& second)
{
    QCOMPARE(first.path.size(), second.path.size());
    QCOMPARE(first.legRoles, second.legRoles);
    QCOMPARE(first.metrics.pathLengthM, second.metrics.pathLengthM);
    QCOMPARE(first.metrics.transitLengthM, second.metrics.transitLengthM);
    QCOMPARE(first.metrics.coverageLengthM, second.metrics.coverageLengthM);
    QCOMPARE(first.metrics.turnCount, second.metrics.turnCount);
    QCOMPARE(first.quality.status, second.quality.status);
    QCOMPARE(first.quality.targetAreaM2, second.quality.targetAreaM2);
    QCOMPARE(first.quality.coveredAreaM2, second.quality.coveredAreaM2);
    QCOMPARE(first.quality.coverageRatio, second.quality.coverageRatio);
    QCOMPARE(first.quality.numericalToleranceM2, second.quality.numericalToleranceM2);
    QCOMPARE(first.quality.passesRequirement, second.quality.passesRequirement);
    QCOMPARE(first.quality.strictFallbackTriggered, second.quality.strictFallbackTriggered);
    QCOMPARE(first.quality.wholeTargetStrictFallback, second.quality.wholeTargetStrictFallback);
    QCOMPARE(first.quality.uncoveredAreaM2, second.quality.uncoveredAreaM2);
    QCOMPARE(first.quality.criticalUncoveredAreaM2, second.quality.criticalUncoveredAreaM2);
    for (std::size_t index = 0; index < first.path.size(); ++index) {
        QCOMPARE(first.path[index].xM, second.path[index].xM);
        QCOMPARE(first.path[index].yM, second.path[index].yM);
    }
}

void compareComponents(const BoundaryCoverageSupportResult& first, const BoundaryCoverageSupportResult& second)
{
    QCOMPARE(first.status, second.status);
    QCOMPARE(first.error, second.error);
    QCOMPARE(first.components.size(), second.components.size());
    for (std::size_t index = 0; index < first.components.size(); ++index) {
        const auto& left = first.components[index];
        const auto& right = second.components[index];
        QCOMPARE(left.id, right.id);
        QCOMPARE(left.path.size(), right.path.size());
        QCOMPARE(left.legRoles, right.legRoles);
        for (std::size_t point = 0; point < left.path.size(); ++point) {
            QCOMPARE(left.path[point].xM, right.path[point].xM);
            QCOMPARE(left.path[point].yM, right.path[point].yM);
        }
    }
}

}  // namespace

void CoverageRepairTest::_testTargetRelativeSupport()
{
    PolygonRegionSet2D target{{.outerBoundary = rectangle(0, 0, 20, 20), .holes = {rectangle(5, 5, 10, 10)}},
                              {.outerBoundary = rectangle(30, 0, 40, 20)}};
    const PolygonRegionSet2D navigation{{.outerBoundary = rectangle(-100, -100, 100, 100)}};
    const auto first = generateCoverageRepairSupport(target, navigation);
    QCOMPARE(first.status, PlanningStatus::Success);
    QCOMPARE(first.components.size(), std::size_t{3});
    for (const auto& component : first.components) {
        QCOMPARE(component.path.front().xM, component.path.back().xM);
        QCOMPARE(component.path.front().yM, component.path.back().yM);
        for (std::size_t leg = 0; leg < component.legRoles.size(); ++leg) {
            QVERIFY(Geometry::segmentInsidePolygonRegionForValidatedGeometry(target, component.path[leg],
                                                                             component.path[leg + 1]));
        }
    }
    for (auto& region : target) {
        std::rotate(region.outerBoundary.vertices.begin(), region.outerBoundary.vertices.begin() + 1,
                    region.outerBoundary.vertices.end());
        std::ranges::reverse(region.outerBoundary.vertices);
        for (auto& hole : region.holes) {
            std::rotate(hole.vertices.begin(), hole.vertices.begin() + 1, hole.vertices.end());
            std::ranges::reverse(hole.vertices);
        }
    }
    std::ranges::reverse(target);
    compareComponents(first, generateCoverageRepairSupport(target, navigation));
    QCOMPARE(generateCoverageRepairSupport({}, navigation).status, PlanningStatus::Failed);
    const auto disjoint = generateCoverageRepairSupport(target, {{.outerBoundary = rectangle(200, 200, 210, 210)}});
    QCOMPARE(disjoint.status, PlanningStatus::Success);
    QVERIFY(disjoint.components.empty());
}

void CoverageRepairTest::_testAssessmentErrorAndInitialPass()
{
    const PolygonRegionSet2D target{{.outerBoundary = rectangle(0, 0, 2, 2)}};
    const auto safety = safetyFor(target);
    auto candidate = candidateFor(target, {{0, 1}, {2, 1}}, 4);
    QVERIFY(coveragePolicyPass(candidate.quality));
    auto result = repairCoverageCandidate(target, target, safety, 4, CoverageRequirement::Strict, strategy, candidate);
    QVERIFY(!result.attempted);
    compareCandidates(candidate, result.candidate);
    candidate.quality.status = CoverageQualityStatus::AssessmentError;
    candidate.quality.error = CoverageQualityError::NumericalFailure;
    candidate.quality.uncoveredAreaM2 = std::numeric_limits<double>::quiet_NaN();
    candidate.quality.residual.uncoveredRegion = {{.outerBoundary = {.vertices = {{NAN, NAN}}}}};
    result = repairCoverageCandidate({}, {}, safety, 4, CoverageRequirement::Strict, strategy, candidate);
    QVERIFY(!result.attempted);
    QVERIFY(result.steps.empty());
    QCOMPARE(result.availableComponentCount, std::size_t{0});
    QCOMPARE(result.candidate.quality.status, CoverageQualityStatus::AssessmentError);
    candidate = candidateFor(target, {{-10, 1}, {-9, 1}}, 4);
    result = repairCoverageCandidate(target, target, safety, 4, CoverageRequirement::Strict, strategy, candidate);
    QVERIFY(!result.attempted);  // No independently hard-safe candidate.
}

void CoverageRepairTest::_testSuccessfulIncrementalRepair()
{
    const PolygonRegionSet2D target{{.outerBoundary = rectangle(0, 0, 10, 4)}};
    const PolygonRegionSet2D active{{.outerBoundary = rectangle(-20, -20, 30, 30)}};
    const auto safety = safetyFor(active);
    const auto initial = candidateFor(target, {{0, 2}, {10, 2}}, 2);
    QCOMPARE(initial.quality.status, CoverageQualityStatus::Insufficient);
    const auto result =
        repairCoverageCandidate(target, active, safety, 2, CoverageRequirement::Strict, strategy, initial);
    QVERIFY(result.attempted);
    QCOMPARE(result.steps.size(), std::size_t{1});
    QVERIFY(coveragePolicyPass(result.candidate.quality));
    verifySteps(result, safety);
    // N is much larger than C. Every repair coverage leg stays on a complete target support ring.
    for (const auto& point : result.steps.front().componentPath) {
        QVERIFY(point.xM >= 0 && point.xM <= 10 && point.yM >= 0 && point.yM <= 4);
    }

    // Both complete rings are initially useful; either footprint covers both small targets.
    // PASS after ONE application must leave the other available ring unapplied.
    const PolygonRegionSet2D closeTargets{{.outerBoundary = rectangle(0, 0, 2, 2)},
                                          {.outerBoundary = rectangle(3, 0, 5, 2)}};
    const auto emptyCoverage = candidateFor(closeTargets, {{-8, 1}, {-7, 1}}, 6);
    const auto stopOnPass =
        repairCoverageCandidate(closeTargets, active, safety, 6, CoverageRequirement::Strict, strategy, emptyCoverage);
    QCOMPARE(stopOnPass.availableComponentCount, std::size_t{2});
    QCOMPARE(stopOnPass.steps.size(), std::size_t{1});
    QVERIFY(coveragePolicyPass(stopOnPass.candidate.quality));
    verifySteps(stopOnPass, safety);
}

void CoverageRepairTest::_testFiniteInsufficientAndNoUsefulRepair()
{
    const PolygonRegionSet2D target{{.outerBoundary = rectangle(0, 0, 10, 10)}};
    const auto safety = safetyFor(target);
    const auto initial = candidateFor(target, {{0, 5}, {10, 5}}, 1);
    const auto result =
        repairCoverageCandidate(target, target, safety, 1, CoverageRequirement::Strict, strategy, initial);
    QVERIFY(result.attempted);
    QCOMPARE(result.steps.size(), std::size_t{1});
    QCOMPARE(result.candidate.quality.status, CoverageQualityStatus::Insufficient);
    verifySteps(result, safety);
    // Reapplying the already covered ring gives Equivalent; it must preserve the retained best fact.
    const auto equivalent =
        repairCoverageCandidate(target, target, safety, 1, CoverageRequirement::Strict, strategy, result.candidate);
    QVERIFY(equivalent.attempted);
    QVERIFY(equivalent.steps.empty());
    compareCandidates(result.candidate, equivalent.candidate);
    const auto component = generateCoverageRepairSupport(target, target).components.front();
    QVERIFY(!evaluateCoverageRepairTrial(target, target, safety, 1, CoverageRequirement::Strict, strategy,
                                         result.candidate, component, 0, false));  // Actual Equivalent trial rejected.
    const PolygonRegionSet2D disconnected{{.outerBoundary = rectangle(0, 0, 2, 2)},
                                          {.outerBoundary = rectangle(10, 0, 12, 2)}};
    const auto isolated = candidateFor(disconnected, {{0, 1}, {2, 1}}, 4);
    const auto unreachable = repairCoverageCandidate(disconnected, disconnected, safetyFor(disconnected), 4,
                                                     CoverageRequirement::Strict, strategy, isolated);
    QVERIFY(unreachable.attempted);
    QVERIFY(unreachable.steps.empty());
    compareCandidates(isolated, unreachable.candidate);
    // A trial evaluator failure cannot replace a reliable current result or become its residual baseline.
    const auto failedAssessment =
        repairCoverageCandidate(target, target, safety, 1e150, CoverageRequirement::Strict, strategy, initial);
    QVERIFY(failedAssessment.attempted);
    QVERIFY(failedAssessment.steps.empty());
    compareCandidates(initial, failedAssessment.candidate);
    const auto failedQuality =
        evaluateCoverageQuality(target, initial.path, initial.legRoles, 1e150, CoverageRequirement::Strict, strategy);
    QCOMPARE(failedQuality.status, CoverageQualityStatus::AssessmentError);
    QVERIFY(!evaluateCoverageRepairTrial(target, target, safety, 1e150, CoverageRequirement::Strict, strategy, initial,
                                         component, 0, false));  // Actual trial evaluator failure rejected.
    const auto unsafeComponent = generateCoverageRepairSupport({{.outerBoundary = rectangle(0, 0, 20, 20)}},
                                                               {{.outerBoundary = rectangle(-1, -1, 21, 21)}})
                                     .components.front();
    QVERIFY(!evaluateCoverageRepairTrial(target, target, safety, 1, CoverageRequirement::Strict, strategy, initial,
                                         unsafeComponent, 0, false));  // Whole traversal violates hard space.
    auto invalidComponent = component;
    invalidComponent.path.pop_back();
    QVERIFY(!evaluateCoverageRepairTrial(target, target, safety, 1, CoverageRequirement::Strict, strategy, initial,
                                         invalidComponent, 0, false));
}

void CoverageRepairTest::_testHoleAndRouteAwareOrdering()
{
    const PolygonRegionSet2D active{{.outerBoundary = rectangle(-10, -10, 15, 30), .holes = {rectangle(3, -5, 5, 25)}}};
    const PolygonRegionSet2D target{{.outerBoundary = rectangle(0, 0, 2, 2)},
                                    {.outerBoundary = rectangle(6, 9, 8, 11)}};
    const auto initial = candidateFor(target, {{1, 10}, {2, 10}}, 4);
    const auto support = generateCoverageRepairSupport(target, active);
    QCOMPARE(support.components.size(), std::size_t{2});
    const auto nearestCost = [&](const BoundaryCoverageComponent& component, bool routed) {
        double best = std::numeric_limits<double>::infinity();
        for (const auto& entry : component.path) {
            const auto route = routeStatic(active, initial.path.back(), entry);
            if (route.status != PlanningStatus::Success) {
                return std::numeric_limits<double>::infinity();
            }
            best = std::min(best, routed ? route.lengthM : std::hypot(entry.xM - 2, entry.yM - 10));
        }
        return best;
    };
    QVERIFY(nearestCost(support.components[0], false) > nearestCost(support.components[1], false));
    QVERIFY(nearestCost(support.components[0], true) < nearestCost(support.components[1], true));
    const auto result =
        repairCoverageCandidate(target, active, safetyFor(active), 4, CoverageRequirement::Strict, strategy, initial);
    QCOMPARE(result.steps.size(), std::size_t{2});
    QCOMPARE(result.steps.front().componentId, std::uint32_t{0});
    // Equal area/critical improvement: the first step is chosen by real legal route cost.
    auto other = candidateFor(target, support.components[1].path, 4);
    auto firstOnly = candidateFor(target, support.components[0].path, 4);
    QCOMPARE(compareCoverageQuality(other.quality, firstOnly.quality), CoverageQualityComparison::Equivalent);
    QVERIFY(coveragePolicyPass(result.candidate.quality));
    verifySteps(result, safetyFor(active));
    bool transitOutsideTarget = false;
    for (std::size_t leg = 0; leg < result.candidate.legRoles.size(); ++leg) {
        if (result.candidate.legRoles[leg] == PathLegRole::Transit) {
            transitOutsideTarget |= !Geometry::segmentInsidePolygonRegionForValidatedGeometry(
                target, result.candidate.path[leg], result.candidate.path[leg + 1]);
        }
    }
    QVERIFY(transitOutsideTarget);

    const PolygonRegionSet2D holed{{.outerBoundary = rectangle(0, 0, 20, 20), .holes = {rectangle(8, 8, 12, 12)}}};
    const auto holeInitial = candidateFor(holed, {{0, 5}, {20, 5}}, 2);
    const auto holeResult =
        repairCoverageCandidate(holed, holed, safetyFor(holed), 2, CoverageRequirement::Strict, strategy, holeInitial);
    QVERIFY(holeResult.attempted);
    QCOMPARE(holeResult.steps.size(), std::size_t{2});
    QVERIFY(std::ranges::any_of(holeResult.steps, [](const auto& step) { return step.componentPath.front().xM == 8; }));
    verifySteps(holeResult, safetyFor(holed));
}

void CoverageRepairTest::_testStableTrialKeys()
{
    const PolygonRegionSet2D target{{.outerBoundary = rectangle(0, 0, 10, 10)}};
    CoverageRepairStep low;
    low.after = candidateFor(target, {{0, 5}, {10, 5}});
    low.transitionCostM = 10;
    auto qualityBetter = low;
    qualityBetter.after = candidateFor(target, {{0, 3}, {10, 3}, {0, 7}, {10, 7}});
    qualityBetter.transitionCostM = 100;
    auto qualityWorse = low;
    qualityWorse.transitionCostM = 1;
    QCOMPARE(compareCoverageQuality(qualityBetter.after.quality, qualityWorse.after.quality),
             CoverageQualityComparison::Better);
    QVERIFY(coverageRepairTrialBetter(qualityBetter, qualityWorse));
    QVERIFY(!coverageRepairTrialBetter(qualityWorse, qualityBetter));

    auto lowerCost = low;
    auto higherCost = low;
    lowerCost.transitionCostM = 5;
    higherCost.transitionCostM = 50;
    higherCost.componentId = 0;
    lowerCost.componentId = 100;
    QVERIFY(coverageRepairTrialBetter(lowerCost, higherCost));
    QVERIFY(!coverageRepairTrialBetter(higherCost, lowerCost));
    for (int dimension = 0; dimension < 3; ++dimension) {
        auto high = low;
        if (dimension == 0) {
            high.componentId = 1;
            low.entryIndex = 100;
            low.reverse = true;
        } else if (dimension == 1) {
            high.entryIndex = low.entryIndex + 1;
            high.reverse = false;
        } else {
            low.reverse = false;
            high = low;
            high.reverse = true;
        }
        QVERIFY(coverageRepairTrialBetter(low, high));
        QVERIFY(!coverageRepairTrialBetter(high, low));
    }
    auto notComparable = low;
    notComparable.after.quality.status = CoverageQualityStatus::AssessmentError;
    QVERIFY(!coverageRepairTrialBetter(notComparable, low));
}

void CoverageRepairTest::_testRepairDeterminism()
{
    PolygonRegionSet2D target{{.outerBoundary = rectangle(0, 0, 20, 20), .holes = {rectangle(8, 8, 12, 12)}},
                              {.outerBoundary = rectangle(30, 0, 40, 20)}};
    const PolygonRegionSet2D active{{.outerBoundary = rectangle(-10, -10, 50, 30), .holes = {rectangle(8, 8, 12, 12)}}};
    const auto initial = candidateFor(target, {{0, 5}, {20, 5}}, 2);
    const auto first =
        repairCoverageCandidate(target, active, safetyFor(active), 2, CoverageRequirement::Strict, strategy, initial);
    QVERIFY(!first.steps.empty());
    for (int iteration = 0; iteration < 3; ++iteration) {
        for (auto& region : target) {
            std::rotate(region.outerBoundary.vertices.begin(), region.outerBoundary.vertices.begin() + 1,
                        region.outerBoundary.vertices.end());
            std::ranges::reverse(region.outerBoundary.vertices);
            for (auto& hole : region.holes) {
                std::rotate(hole.vertices.begin(), hole.vertices.begin() + 1, hole.vertices.end());
                std::ranges::reverse(hole.vertices);
            }
        }
        std::ranges::reverse(target);
        const auto current = candidateFor(target, initial.path, 2);
        const auto result = repairCoverageCandidate(target, active, safetyFor(active), 2, CoverageRequirement::Strict,
                                                    strategy, current);
        compareCandidates(first.candidate, result.candidate);
        QCOMPARE(first.steps.size(), result.steps.size());
        for (std::size_t index = 0; index < first.steps.size(); ++index) {
            QCOMPARE(first.steps[index].componentId, result.steps[index].componentId);
            QCOMPARE(first.steps[index].entryIndex, result.steps[index].entryIndex);
            QCOMPARE(first.steps[index].reverse, result.steps[index].reverse);
        }
    }
}

void CoverageRepairTest::_testPlannerInitialPassAndStandardStrict()
{
    const auto initialPass = SimpleMonotoneCoveragePlanner{}.plan(problemFor());
    QCOMPARE(initialPass.status, PlanningStatus::Success);
    QCOMPARE(initialPass.repairCandidates.size(), std::size_t{4});
    for (const auto& repair : initialPass.repairCandidates) {
        QVERIFY(!repair.attempted);
        QVERIFY(repair.steps.empty());
    }
    QCOMPARE(initialPass.path.size(), std::size_t{8});  // Four unchanged lane endpoints, no perimeter.
    compareCandidates(initialPass.repairCandidates[0].candidate,
                      SimpleMonotoneCoveragePlanner{}.plan(problemFor()).repairCandidates[0].candidate);

    const auto mixedInitial = SimpleMonotoneCoveragePlanner{}.plan(problemFor(0, 1));
    QCOMPARE(mixedInitial.repairCandidates.front().candidate.quality.status, CoverageQualityStatus::Insufficient);
    QVERIFY(coveragePolicyPass(mixedInitial.repairCandidates.back().candidate.quality));
    for (const auto& repair : mixedInitial.repairCandidates) {
        QVERIFY(!repair.attempted);  // ANY initial PASS suppresses repair for the entire collection.
        QVERIFY(repair.steps.empty());
    }

    auto standardProblem = problemFor(0.5, 0.5, 100);
    standardProblem.coverageRequirement = CoverageRequirement::Standard;
    const auto standard = SimpleMonotoneCoveragePlanner{}.plan(standardProblem);
    QCOMPARE(standard.status, PlanningStatus::Success);
    QCOMPARE(standard.outcome.coverageQuality->status, CoverageQualityStatus::Acceptable);
    for (const auto& repair : standard.repairCandidates) {
        QVERIFY(!repair.attempted);
    }
    standardProblem.coverageRequirement = CoverageRequirement::Strict;
    const auto strict = SimpleMonotoneCoveragePlanner{}.plan(standardProblem);
    QCOMPARE(strict.status, PlanningStatus::Success);
    QCOMPARE(strict.outcome.coverageQuality->status, CoverageQualityStatus::Complete);
    for (std::size_t index = 0; index < strict.repairCandidates.size(); ++index) {
        const auto& repair = strict.repairCandidates[index];
        QVERIFY(repair.attempted);
        QVERIFY(!repair.steps.empty());
        QCOMPARE(repair.steps.front().before.uncoveredAreaM2,
                 standard.repairCandidates[index].candidate.quality.uncoveredAreaM2);
        QCOMPARE(repair.steps.front().before.numericalToleranceM2,
                 standard.repairCandidates[index].candidate.quality.numericalToleranceM2);
    }
}

void CoverageRepairTest::_testAllCandidatesAndHardPassPriority()
{
    const auto problem = problemFor(1, 2);
    const auto result = SimpleMonotoneCoveragePlanner{}.plan(problem);
    QCOMPARE(result.status, PlanningStatus::Success);
    QCOMPARE(result.repairCandidates.size(), std::size_t{4});
    const auto safety = buildSafetyTrackRegions(problem.region, problem.safety, problem.executionSafety);
    for (std::size_t index = 0; index < result.repairCandidates.size(); ++index) {
        const auto& repair = result.repairCandidates[index];
        QVERIFY(repair.attempted);
        QVERIFY(!repair.steps.empty());
        verifySteps(repair, safety);
        QCOMPARE(repair.steps.front().before.status, CoverageQualityStatus::Insufficient);
        if (index < 2) {
            QVERIFY(repair.candidate.preferredSafe);
            QVERIFY(!coveragePolicyPass(repair.candidate.quality));
        } else {
            QVERIFY(!repair.candidate.preferredSafe);
            QVERIFY(coveragePolicyPass(repair.candidate.quality));
        }
    }
    QCOMPARE(evaluateSafetyCandidate(safety, result.path).tier, SafetySolutionTier::D1);
    QVERIFY(result.outcome.coverageQuality->passesRequirement);
}

void CoverageRepairTest::_testAutoNoEscalationAndBcdIntegration()
{
    const auto problem = problemFor(2, 2);
    const auto result = AutoCoveragePlanner{}.plan(problem);
    QCOMPARE(result.status, PlanningStatus::Success);
    QCOMPARE(result.outcome.readiness, MissionReadiness::ReviewRequired);
    QCOMPARE(result.error, CoveragePlanningError::None);
    QVERIFY(!result.path.empty());
    QCOMPARE(result.plannerSource->resolvedStrategy.strategyId,
             std::string(CoverageStrategySemantics::SimpleMonotoneId));
    QVERIFY(!result.plannerSource->escalated);
    for (const auto& repair : result.repairCandidates) {
        QVERIFY(repair.attempted);
        QVERIFY(!repair.steps.empty());
        QCOMPARE(repair.candidate.quality.status, CoverageQualityStatus::Insufficient);
        QVERIFY(!repair.candidate.path.empty());  // Best internal fact retained, not externally executable.
    }

    auto holed = problemFor(1, 1);
    holed.region.noGoRegions.push_back(rectangle(8, 8, 12, 12));
    const auto bcd = BoustrophedonCoveragePlanner{}.plan(holed);
    QVERIFY(!bcd.repairCandidates.empty());
    const auto safety = buildSafetyTrackRegions(holed.region, holed.safety, holed.executionSafety);
    for (const auto& repair : bcd.repairCandidates) {
        QVERIFY(repair.attempted);
        QVERIFY(!repair.steps.empty());
        verifySteps(repair, safety);
    }
    const auto automatic = AutoCoveragePlanner{}.plan(holed);
    QCOMPARE(automatic.plannerSource->resolvedStrategy.strategyId,
             std::string(CoverageStrategySemantics::BoustrophedonId));
    QCOMPARE(automatic.plannerSource->resolvedStrategy.semanticVersion,
             std::string(CoverageStrategySemantics::BoustrophedonVersion));
    QVERIFY(automatic.plannerSource->escalated);
    QCOMPARE(automatic.repairCandidates.size(), bcd.repairCandidates.size());
    for (std::size_t index = 0; index < bcd.repairCandidates.size(); ++index) {
        compareCandidates(bcd.repairCandidates[index].candidate, automatic.repairCandidates[index].candidate);
    }
}

UT_REGISTER_TEST_LIGHTWEIGHT(CoverageRepairTest, TestLabel::Unit)
