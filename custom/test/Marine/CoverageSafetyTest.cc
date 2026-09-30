#include "CoverageSafetyTest.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "CoverageProblemValidator.h"
#include "CoverageSafety.h"
#include "Geometry/MarineGeometry.h"
#include "Geometry/PolygonRegion.h"

using namespace Marine;
using TestPath = std::vector<Point2D>;
Q_DECLARE_METATYPE(TestPath)

namespace {

Polygon2D rectangle(double left, double bottom, double right, double top)
{
    return {.vertices = {{left, bottom}, {right, bottom}, {right, top}, {left, top}}};
}

Region2D region(bool obstacle = false)
{
    Region2D result{.coverageBoundary = rectangle(2, 2, 6, 6), .navigationBoundary = rectangle(0, 0, 20, 20)};
    if (obstacle) {
        result.noGoRegions = {rectangle(8, 8, 12, 12)};
    }
    return result;
}

void verifyHierarchy(const SafetyTrackRegions& regions)
{
    const auto preferred =
        Geometry::isRegionSetContained(regions.preferredExecutionTrackRegion, regions.hardExecutionTrackRegion);
    const auto hard = Geometry::isRegionSetContained(regions.hardExecutionTrackRegion, regions.nominalHardTrackRegion);
    QCOMPARE(preferred.status, Geometry::PolygonRegionOperationStatus::Success);
    QCOMPARE(hard.status, Geometry::PolygonRegionOperationStatus::Success);
    QVERIFY(preferred.contained);
    QVERIFY(hard.contained);
}

}  // namespace

void CoverageSafetyTest::_testMargins_data()
{
    QTest::addColumn<double>("h");
    QTest::addColumn<double>("p");
    QTest::addColumn<double>("e");
    QTest::newRow("zero") << 0.0 << 0.0 << 0.0;
    QTest::newRow("equal-no-execution") << 1.0 << 1.0 << 0.0;
    QTest::newRow("preferred-no-execution") << 1.0 << 3.0 << 0.0;
    QTest::newRow("zero-hard-with-execution") << 0.0 << 1.0 << 0.5;
    QTest::newRow("equal-with-execution") << 1.0 << 1.0 << 0.5;
    QTest::newRow("preferred-with-execution") << 1.0 << 3.0 << 0.5;
    QTest::newRow("empty-preferred") << 1.0 << 10.0 << 0.0;
    QTest::newRow("empty-hard") << 9.5 << 10.0 << 0.5;
    QTest::newRow("all-empty") << 10.0 << 12.0 << 0.0;
}

void CoverageSafetyTest::_testMargins()
{
    QFETCH(double, h);
    QFETCH(double, p);
    QFETCH(double, e);
    const SafetyConfig safety{h, p};
    const auto result = buildSafetyTrackRegions(region(), safety, {e});
    QCOMPARE(result.error, CoveragePlanningError::None);
    verifyHierarchy(result.regions);
    const auto expectedArea = [](double margin) {
        const double effective = Geometry::conservativeSafetyOffsetUnits(margin) / 1000.0;
        return std::pow(std::max(0.0, 20.0 - 2.0 * effective), 2);
    };
    const auto nominal = Geometry::polygonRegionArea(result.regions.nominalHardTrackRegion);
    const auto hard = Geometry::polygonRegionArea(result.regions.hardExecutionTrackRegion);
    const auto preferred = Geometry::polygonRegionArea(result.regions.preferredExecutionTrackRegion);
    QVERIFY(std::abs(nominal.areaM2 - expectedArea(h)) < 0.005);
    QVERIFY(std::abs(hard.areaM2 - expectedArea(h + e)) < 0.005);
    QVERIFY(std::abs(preferred.areaM2 - expectedArea(p + e)) < 0.005);
    QCOMPARE(safety.hardSafetyMarginM, h);
    QCOMPARE(safety.preferredSafetyMarginM, p);
}

void CoverageSafetyTest::_testInvalidMargins_data()
{
    QTest::addColumn<double>("h");
    QTest::addColumn<double>("p");
    QTest::addColumn<double>("e");
    QTest::addColumn<CoveragePlanningError>("error");
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    QTest::newRow("negative-h") << -1.0 << 2.0 << 0.0 << CoveragePlanningError::InvalidSafetyMargin;
    QTest::newRow("nan-h") << nan << 2.0 << 0.0 << CoveragePlanningError::InvalidSafetyMargin;
    QTest::newRow("infinite-h") << inf << inf << 0.0 << CoveragePlanningError::InvalidSafetyMargin;
    QTest::newRow("p-less-h") << 2.0 << 1.0 << 0.0 << CoveragePlanningError::InvalidPreferredSafetyMargin;
    QTest::newRow("nan-p") << 1.0 << nan << 0.0 << CoveragePlanningError::InvalidPreferredSafetyMargin;
    QTest::newRow("infinite-p") << 1.0 << inf << 0.0 << CoveragePlanningError::InvalidPreferredSafetyMargin;
    QTest::newRow("negative-e") << 1.0 << 2.0 << -1.0 << CoveragePlanningError::InvalidExecutionMargin;
    QTest::newRow("nan-e") << 1.0 << 2.0 << nan << CoveragePlanningError::InvalidExecutionMargin;
    QTest::newRow("infinite-e") << 1.0 << 2.0 << inf << CoveragePlanningError::InvalidExecutionMargin;
}

void CoverageSafetyTest::_testInvalidMargins()
{
    QFETCH(double, h);
    QFETCH(double, p);
    QFETCH(double, e);
    QFETCH(CoveragePlanningError, error);
    const auto result = buildSafetyTrackRegions(region(), {h, p}, {e});
    QCOMPARE(result.error, error);
    QVERIFY(result.regions.nominalHardTrackRegion.empty());
    QVERIFY(result.regions.hardExecutionTrackRegion.empty());
    QVERIFY(result.regions.preferredExecutionTrackRegion.empty());
    CoveragePlanningProblem problem{.region = region(), .swathWidthM = 2, .safety = {h, p}, .executionSafety = {e}};
    QCOMPARE(CoverageProblemValidator::validateAndNormalize(problem), error);
    QCOMPARE(CoverageProblemValidator::statusForError(error), PlanningStatus::InvalidInput);
}

void CoverageSafetyTest::_testSplitAndHoles()
{
    for (const double e : {0.0, 0.5}) {
        const auto holes = buildSafetyTrackRegions(region(true), {1.0, 2.0}, {e});
        QCOMPARE(holes.error, CoveragePlanningError::None);
        verifyHierarchy(holes.regions);
        QCOMPARE(holes.regions.nominalHardTrackRegion.front().holes.size(), std::size_t(1));
        QCOMPARE(holes.regions.hardExecutionTrackRegion.front().holes.size(), std::size_t(1));
        QCOMPARE(holes.regions.preferredExecutionTrackRegion.front().holes.size(), std::size_t(1));
        QVERIFY(!Geometry::pointInsidePolygonRegion(holes.regions.hardExecutionTrackRegion, {10, 10}));
        // Navigation outside C remains available; the obstacle outside C is still inflated.
        QVERIFY(Geometry::pointInsidePolygonRegion(holes.regions.hardExecutionTrackRegion, {17, 17}));
        QVERIFY(!Geometry::pointInsidePolygonRegion(holes.regions.hardExecutionTrackRegion, {7.5, 10}));
        // Miter corners are more restrictive than nominal round corners.
        QVERIFY(Geometry::pointInsidePolygonRegion(holes.regions.nominalHardTrackRegion, {7.1, 7.1}));
        QVERIFY(!Geometry::pointInsidePolygonRegion(holes.regions.hardExecutionTrackRegion, {7.1, 7.1}));
    }
    Region2D split = region();
    split.navigationBoundary.vertices = {{0, 0},   {8, 0},   {8, 4},  {12, 4}, {12, 0}, {20, 0},
                                         {20, 10}, {12, 10}, {12, 6}, {8, 6},  {8, 10}, {0, 10}};
    split.noGoRegions = {rectangle(3, 3, 4, 4)};
    const auto result = buildSafetyTrackRegions(split, {1.1, 1.3}, {0.1});
    QCOMPARE(result.error, CoveragePlanningError::None);
    verifyHierarchy(result.regions);
    QCOMPARE(result.regions.nominalHardTrackRegion.size(), std::size_t(2));
    QCOMPARE(result.regions.hardExecutionTrackRegion.size(), std::size_t(2));
    QCOMPARE(result.regions.preferredExecutionTrackRegion.size(), std::size_t(2));
    std::size_t holes = 0;
    for (const auto& component : result.regions.hardExecutionTrackRegion) {
        holes += component.holes.size();
    }
    QCOMPARE(holes, std::size_t(1));
    const TestPath crossing{{6, 5}, {14, 5}};
    const auto assessment = evaluateSafetyCandidate(result, crossing);
    QCOMPARE(assessment.error, CoveragePlanningError::UnsafeConnector);
    QVERIFY(!assessment.tier.has_value());
    QVERIFY(assessment.legs.empty());
}

void CoverageSafetyTest::_testEntirePath_data()
{
    QTest::addColumn<TestPath>("path");
    QTest::addColumn<CoveragePlanningError>("error");
    QTest::addColumn<bool>("preferred");
    QTest::newRow("d0") << TestPath{{3, 3}, {5, 3}, {5, 5}} << CoveragePlanningError::None << true;
    QTest::newRow("d1") << TestPath{{2, 3}, {2, 5}} << CoveragePlanningError::None << false;
    QTest::newRow("preferred-endpoints-hard-only-interior")
        << TestPath{{5, 6}, {15, 6}} << CoveragePlanningError::None << false;
    QTest::newRow("hard-endpoints-unsafe-interior")
        << TestPath{{5, 10}, {15, 10}} << CoveragePlanningError::UnsafeConnector << false;
    QTest::newRow("later-leg-unsafe") << TestPath{{5, 5}, {5, 10}, {15, 10}} << CoveragePlanningError::UnsafeConnector
                                      << false;
    QTest::newRow("closed-hard-boundary") << TestPath{{1.503, 3}, {1.503, 5}} << CoveragePlanningError::None << false;
    QTest::newRow("violates-e") << TestPath{{1.25, 3}, {1.25, 5}} << CoveragePlanningError::UnsafeConnector << false;
    QTest::newRow("empty") << TestPath{} << CoveragePlanningError::InvalidGeneratedPath << false;
    QTest::newRow("single") << TestPath{{3, 3}} << CoveragePlanningError::InvalidGeneratedPath << false;
    QTest::newRow("duplicate-leg") << TestPath{{3, 3}, {3, 3}, {4, 4}} << CoveragePlanningError::InvalidGeneratedPath
                                   << false;
    QTest::newRow("nonfinite") << TestPath{{3, 3}, {std::numeric_limits<double>::infinity(), 4}}
                               << CoveragePlanningError::InvalidGeneratedPath << false;
}

void CoverageSafetyTest::_testEntirePath()
{
    QFETCH(TestPath, path);
    QFETCH(CoveragePlanningError, error);
    QFETCH(bool, preferred);
    const auto regions = buildSafetyTrackRegions(region(true), {1, 2}, {0.5});
    QCOMPARE(regions.error, CoveragePlanningError::None);
    const auto result = evaluateSafetyCandidate(regions, path);
    QCOMPARE(result.error, error);
    if (error == CoveragePlanningError::None) {
        QVERIFY(result.tier.has_value());
        QCOMPARE(*result.tier, preferred ? SafetySolutionTier::D0 : SafetySolutionTier::D1);
        QCOMPARE(result.legs.size(), path.size() - 1);
    } else {
        QVERIFY(!result.tier.has_value());
        QVERIFY(result.legs.empty());
    }
}

void CoverageSafetyTest::_testFallback()
{
    const Region2D input = region(true);
    const SafetyConfig safety{1, 2};
    const auto regions = buildSafetyTrackRegions(input, safety, {0.5});
    const TestPath preferred{{3, 3}, {5, 3}};
    const TestPath hard{{2, 3}, {2, 5}};
    const auto d0 = selectPreferredOrHardCandidate(regions, preferred, hard);
    QCOMPARE(d0.assessment.error, CoveragePlanningError::None);
    QCOMPARE(*d0.assessment.tier, SafetySolutionTier::D0);
    QVERIFY(!d0.usedHardFallback);
    QCOMPARE(d0.path.front().xM, 3.0);
    const auto d1 = selectPreferredOrHardCandidate(regions, {}, hard);
    QCOMPARE(d1.assessment.error, CoveragePlanningError::None);
    QCOMPARE(*d1.assessment.tier, SafetySolutionTier::D1);
    QVERIFY(d1.usedHardFallback);
    QCOMPARE(d1.assessment.legs.front(), SafetyLegClass::HardSafeWarning);
    QCOMPARE(d1.path.front().xM, 2.0);
    const auto retained = selectPreferredOrHardCandidate(regions, hard, {});
    QCOMPARE(retained.assessment.error, CoveragePlanningError::None);
    QCOMPARE(*retained.assessment.tier, SafetySolutionTier::D1);
    QVERIFY(retained.usedHardFallback);
    // The tier follows full geometry certification, not the generation attempt's label.
    const auto hardGeneratedPreferred = selectPreferredOrHardCandidate(regions, {}, preferred);
    QCOMPARE(*hardGeneratedPreferred.assessment.tier, SafetySolutionTier::D0);
    QVERIFY(hardGeneratedPreferred.usedHardFallback);
    const auto emptyPreferred = buildSafetyTrackRegions(input, {1, 11}, {0.5});
    QCOMPARE(emptyPreferred.error, CoveragePlanningError::None);
    QVERIFY(emptyPreferred.regions.preferredExecutionTrackRegion.empty());
    const auto emptyFallback = selectPreferredOrHardCandidate(emptyPreferred, {}, hard);
    QCOMPARE(*emptyFallback.assessment.tier, SafetySolutionTier::D1);
    QVERIFY(emptyFallback.usedHardFallback);
    QCOMPARE(safety.hardSafetyMarginM, 1.0);
    QCOMPARE(safety.preferredSafetyMarginM, 2.0);
    QCOMPARE(input.navigationBoundary.vertices[2].xM, 20.0);
    QCOMPARE(input.noGoRegions.front().vertices.front().xM, 8.0);
}

void CoverageSafetyTest::_testEmptyAndFailure()
{
    const TestPath path{{3, 3}, {5, 3}};
    const auto empty = buildSafetyTrackRegions(region(), {10, 12}, {0.0});
    QCOMPARE(empty.error, CoveragePlanningError::None);
    QCOMPARE(evaluateSafetyCandidate(empty, path).error, CoveragePlanningError::NoNavigableArea);
    const double huge = std::numeric_limits<double>::max();
    for (const auto& safety : {SafetyConfig{1, 1e13}, SafetyConfig{huge, huge}}) {
        const auto failed = buildSafetyTrackRegions(region(), safety, {huge});
        QCOMPARE(failed.error, CoveragePlanningError::GeometryFailure);
        QVERIFY(failed.regions.hardExecutionTrackRegion.empty());
        const auto selected = selectPreferredOrHardCandidate(failed, path, path);
        QCOMPARE(selected.assessment.error, CoveragePlanningError::GeometryFailure);
        QVERIFY(selected.path.empty());
        QVERIFY(!selected.assessment.tier.has_value());
    }
    // A preferred offset outside the backend range must not be treated as an empty preferred set.
    const auto failedPreferred = buildSafetyTrackRegions(region(), {1, 1e13}, {0.0});
    QCOMPARE(failedPreferred.error, CoveragePlanningError::GeometryFailure);
    QVERIFY(selectPreferredOrHardCandidate(failedPreferred, {}, path).path.empty());
}

void CoverageSafetyTest::_testHierarchyFailure()
{
    const TestPath path{{3, 3}, {5, 3}};
    auto regions = buildSafetyTrackRegions(region(), {1, 2}, {0.5});
    QCOMPARE(regions.error, CoveragePlanningError::None);
    regions.regions.preferredExecutionTrackRegion = regions.regions.nominalHardTrackRegion;
    QCOMPARE(evaluateSafetyCandidate(regions, path).error, CoveragePlanningError::ExecutionRegionNotConservative);
    regions = buildSafetyTrackRegions(region(), {1, 2}, {0.5});
    regions.regions.nominalHardTrackRegion.clear();
    QCOMPARE(evaluateSafetyCandidate(regions, path).error, CoveragePlanningError::ExecutionRegionNotConservative);
    regions = buildSafetyTrackRegions(region(), {1, 2}, {0.5});
    regions.regions.hardExecutionTrackRegion.front().outerBoundary.vertices.clear();
    QCOMPARE(evaluateSafetyCandidate(regions, path).error, CoveragePlanningError::GeometryFailure);
}

void CoverageSafetyTest::_testSubMillimeterClearance()
{
    for (const double shift : {-0.0005, 0.0005}) {
        Region2D input = region(true);
        input.coverageBoundary = rectangle(2 + shift, 2 + shift, 6 + shift, 6 + shift);
        input.navigationBoundary = rectangle(shift, shift, 20 + shift, 20 + shift);
        input.noGoRegions = {rectangle(8 + shift, 8 + shift, 12 + shift, 12 + shift)};
        for (const double h : {0.0, 0.0001, 0.00049, 0.0005, 0.00051, 0.0009}) {
            for (const double e : {0.0, 0.0001, 0.0005}) {
                const double clearance = h + e;
                const auto regions = buildSafetyTrackRegions(input, {h, h}, {e});
                QCOMPARE(regions.error, CoveragePlanningError::None);
                verifyHierarchy(regions.regions);
                const auto& nominal = regions.regions.nominalHardTrackRegion;
                const auto& hard = regions.regions.hardExecutionTrackRegion;
                // The predicate's 1 mm boundary tolerance must still reject physically under-clear points.
                QVERIFY(!Geometry::pointInsidePolygonRegion(nominal, {shift + h - 0.0001, 5 + shift}));
                QVERIFY(!Geometry::pointInsidePolygonRegion(nominal, {8 + shift - h + 0.0001, 10 + shift}));
                QVERIFY(!Geometry::pointInsidePolygonRegion(hard, {shift + clearance - 0.0001, 5 + shift}));
                QVERIFY(!Geometry::pointInsidePolygonRegion(hard, {8 + shift - clearance + 0.0001, 10 + shift}));
                QVERIFY(Geometry::pointInsidePolygonRegion(hard, {shift + clearance + 0.01, 5 + shift}));
                QVERIFY(Geometry::pointInsidePolygonRegion(hard, {8 + shift - clearance - 0.01, 10 + shift}));
                const TestPath unsafe{{shift + clearance - 0.0001, 4 + shift}, {shift + clearance - 0.0001, 6 + shift}};
                QCOMPARE(evaluateSafetyCandidate(regions, unsafe).error, CoveragePlanningError::UnsafeConnector);
            }
        }
    }
}

UT_REGISTER_TEST_LIGHTWEIGHT(CoverageSafetyTest, TestLabel::Unit)
