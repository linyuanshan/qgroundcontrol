#include "BoundaryCoverageSupportTest.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#include "Geometry/MarineGeometry.h"
#include "Geometry/PolygonRegion.h"
#include "Planning/BoundaryCoverageSupport.h"

using namespace Marine;

namespace {

Polygon2D rectangle(double minimumX, double minimumY, double maximumX, double maximumY)
{
    return {{{minimumX, minimumY}, {maximumX, minimumY}, {maximumX, maximumY}, {minimumX, maximumY}}};
}

void rotateVertices(Polygon2D& polygon)
{
    std::rotate(polygon.vertices.begin(), std::next(polygon.vertices.begin()), polygon.vertices.end());
}

void compareResults(const BoundaryCoverageSupportResult& first, const BoundaryCoverageSupportResult& second)
{
    QCOMPARE(first.status, second.status);
    QCOMPARE(first.error, second.error);
    QCOMPARE(first.components.size(), second.components.size());
    for (std::size_t componentIndex = 0; componentIndex < first.components.size(); ++componentIndex) {
        const BoundaryCoverageComponent& a = first.components.at(componentIndex);
        const BoundaryCoverageComponent& b = second.components.at(componentIndex);
        QCOMPARE(a.id, b.id);
        QCOMPARE(a.path.size(), b.path.size());
        QCOMPARE(a.legRoles, b.legRoles);
        QCOMPARE(a.pathLengthM, b.pathLengthM);
        for (std::size_t pointIndex = 0; pointIndex < a.path.size(); ++pointIndex) {
            QCOMPARE(a.path.at(pointIndex).xM, b.path.at(pointIndex).xM);
            QCOMPARE(a.path.at(pointIndex).yM, b.path.at(pointIndex).yM);
        }
    }
}

void verifyComponents(const PolygonRegionSet2D& regions, const BoundaryCoverageSupportResult& result)
{
    QVERIFY2(result.status == PlanningStatus::Success, result.message.c_str());
    QCOMPARE(result.error, CoveragePlanningError::None);
    for (std::size_t componentIndex = 0; componentIndex < result.components.size(); ++componentIndex) {
        const BoundaryCoverageComponent& component = result.components.at(componentIndex);
        QCOMPARE(component.id, static_cast<std::uint32_t>(componentIndex));
        QVERIFY(component.path.size() >= 4);
        QCOMPARE(component.path.front().xM, component.path.back().xM);
        QCOMPARE(component.path.front().yM, component.path.back().yM);
        QCOMPARE(component.legRoles.size(), component.path.size() - 1);
        QVERIFY(
            std::ranges::all_of(component.legRoles, [](PathLegRole role) { return role == PathLegRole::Coverage; }));
        double pathLengthM = 0.0;
        for (std::size_t pointIndex = 1; pointIndex < component.path.size(); ++pointIndex) {
            const Point2D& start = component.path.at(pointIndex - 1);
            const Point2D& end = component.path.at(pointIndex);
            QVERIFY(start.isFinite());
            QVERIFY(end.isFinite());
            QVERIFY(Geometry::segmentInsidePolygonRegion(regions, start, end));
            pathLengthM += std::hypot(end.xM - start.xM, end.yM - start.yM);
        }
        QVERIFY(std::abs(pathLengthM - component.pathLengthM) <= Geometry::LengthEpsilonM);
    }
}

}  // namespace

void BoundaryCoverageSupportTest::_testRectangleComponent()
{
    const PolygonRegionSet2D regions{{.outerBoundary = rectangle(0.0, 0.0, 20.0, 10.0)}};
    const BoundaryCoverageSupportResult result = generateBoundaryCoverageSupport(regions);

    verifyComponents(regions, result);
    QCOMPARE(result.components.size(), std::size_t{1});
    QCOMPARE(result.components.front().path.size(), std::size_t{5});
    QCOMPARE(result.components.front().pathLengthM, 60.0);
    QCOMPARE(result.components.front().path.front().xM, 0.0);
    QCOMPARE(result.components.front().path.front().yM, 0.0);
}

void BoundaryCoverageSupportTest::_testDeterminismAndRoundedHole()
{
    const Polygon2D outer = rectangle(0.0, 0.0, 20.0, 20.0);
    const Polygon2D noGo = rectangle(8.0, 8.0, 12.0, 12.0);
    const Geometry::PolygonRegionOperationResult track = Geometry::buildTrackFeasibleRegion(outer, {noGo}, 1.0);
    QCOMPARE(track.status, Geometry::PolygonRegionOperationStatus::Success);
    QCOMPARE(track.regions.size(), std::size_t{1});
    QCOMPARE(track.regions.front().holes.size(), std::size_t{1});

    const BoundaryCoverageSupportResult first = generateBoundaryCoverageSupport(track.regions);
    verifyComponents(track.regions, first);
    QCOMPARE(first.components.size(), std::size_t{2});
    QVERIFY(first.components.at(1).path.size() > 5);

    PolygonRegionSet2D reordered = track.regions;
    std::ranges::reverse(reordered.front().outerBoundary.vertices);
    std::ranges::reverse(reordered.front().holes.front().vertices);
    rotateVertices(reordered.front().outerBoundary);
    rotateVertices(reordered.front().holes.front());
    compareResults(first, generateBoundaryCoverageSupport(reordered));

    const PolygonRegionSet2D disconnected = {
        {.outerBoundary = rectangle(20.0, 0.0, 24.0, 4.0)},
        {.outerBoundary = rectangle(0.0, 0.0, 4.0, 4.0)},
    };
    PolygonRegionSet2D swapped = disconnected;
    std::ranges::reverse(swapped);
    compareResults(generateBoundaryCoverageSupport(disconnected), generateBoundaryCoverageSupport(swapped));
}

void BoundaryCoverageSupportTest::_testInvalidInputIsAtomic()
{
    const PolygonRegionSet2D invalid{{.outerBoundary = {{{0.0, 0.0}, {5.0, 5.0}, {0.0, 5.0}, {5.0, 0.0}}}}};
    const BoundaryCoverageSupportResult empty = generateBoundaryCoverageSupport({});
    const BoundaryCoverageSupportResult bowTie = generateBoundaryCoverageSupport(invalid);

    QCOMPARE(empty.status, PlanningStatus::Failed);
    QCOMPARE(empty.error, CoveragePlanningError::InvalidGeneratedPath);
    QVERIFY(empty.components.empty());
    QCOMPARE(bowTie.status, PlanningStatus::Failed);
    QCOMPARE(bowTie.error, CoveragePlanningError::InvalidGeneratedPath);
    QVERIFY(bowTie.components.empty());
}

UT_REGISTER_TEST_LIGHTWEIGHT(BoundaryCoverageSupportTest, TestLabel::Unit)
