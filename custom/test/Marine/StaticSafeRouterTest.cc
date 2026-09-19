#include "StaticSafeRouterTest.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Geometry/MarineGeometry.h"
#include "Planning/StaticSafeRouter.h"

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

PolygonRegionSet2D regionSet(Polygon2D outer, std::vector<Polygon2D> holes = {})
{
    return {{.outerBoundary = std::move(outer), .holes = std::move(holes)}};
}

bool pointsEqual(const Point2D& first, const Point2D& second)
{
    return (std::abs(first.xM - second.xM) <= Geometry::LengthEpsilonM) &&
           (std::abs(first.yM - second.yM) <= Geometry::LengthEpsilonM);
}

void verifySuccessfulRoute(const PolygonRegionSet2D& regions, const StaticRoute& route, const Point2D& start,
                           const Point2D& goal)
{
    QVERIFY2(route.status == PlanningStatus::Success, route.message.c_str());
    QCOMPARE(route.error, CoveragePlanningError::None);
    QVERIFY(route.path.size() >= 2);
    QVERIFY(pointsEqual(route.path.front(), start));
    QVERIFY(pointsEqual(route.path.back(), goal));
    double computedLengthM = 0.0;
    for (const Point2D& point : route.path) {
        QVERIFY(point.isFinite());
    }
    for (std::size_t index = 1; index < route.path.size(); ++index) {
        QVERIFY(Geometry::segmentInsidePolygonRegion(regions, route.path.at(index - 1), route.path.at(index)));
        computedLengthM += std::hypot(route.path.at(index).xM - route.path.at(index - 1).xM,
                                      route.path.at(index).yM - route.path.at(index - 1).yM);
    }
    QVERIFY(std::isfinite(route.lengthM));
    QVERIFY(std::abs(route.lengthM - computedLengthM) <= Geometry::LengthEpsilonM);
    const double directDistanceM = std::hypot(goal.xM - start.xM, goal.yM - start.yM);
    QVERIFY(route.lengthM + Geometry::LengthEpsilonM >= directDistanceM);
}

void compareRoutes(const StaticRoute& first, const StaticRoute& second)
{
    QCOMPARE(first.status, second.status);
    QCOMPARE(first.error, second.error);
    QCOMPARE(first.lengthM, second.lengthM);
    QCOMPARE(first.path.size(), second.path.size());
    for (std::size_t index = 0; index < first.path.size(); ++index) {
        QCOMPARE(first.path.at(index).xM, second.path.at(index).xM);
        QCOMPARE(first.path.at(index).yM, second.path.at(index).yM);
    }
}

}  // namespace

void StaticSafeRouterTest::_testDirectVisibleRoute()
{
    const PolygonRegionSet2D regions = regionSet(rectangle(0.0, 0.0, 20.0, 20.0));
    const Point2D start{2.0, 3.0};
    const Point2D goal{18.0, 17.0};
    const StaticRoute route = routeStatic(regions, start, goal);

    verifySuccessfulRoute(regions, route, start, goal);
    QCOMPARE(route.path.size(), std::size_t{2});
    QCOMPARE(route.lengthM, std::hypot(16.0, 14.0));
}

void StaticSafeRouterTest::_testSingleNoGoDetourAndReverse()
{
    const PolygonRegionSet2D regions = regionSet(rectangle(0.0, 0.0, 20.0, 20.0), {rectangle(8.0, 8.0, 12.0, 12.0)});
    const Point2D start{2.0, 10.0};
    const Point2D goal{18.0, 10.0};
    const StaticRoute forward = routeStatic(regions, start, goal);
    const StaticRoute reverse = routeStatic(regions, goal, start);

    verifySuccessfulRoute(regions, forward, start, goal);
    verifySuccessfulRoute(regions, reverse, goal, start);
    QCOMPARE(forward.path.size(), std::size_t{4});
    QCOMPARE(reverse.path.size(), forward.path.size());
    QVERIFY(forward.lengthM > 16.0);
    QCOMPARE(reverse.lengthM, forward.lengthM);
    for (std::size_t index = 0; index < forward.path.size(); ++index) {
        QVERIFY(pointsEqual(forward.path.at(index), reverse.path.at(reverse.path.size() - 1 - index)));
    }
}

void StaticSafeRouterTest::_testMultipleNoGoDetour()
{
    const PolygonRegionSet2D regions =
        regionSet(rectangle(0.0, 0.0, 30.0, 20.0), {rectangle(6.0, 7.0, 10.0, 13.0), rectangle(18.0, 7.0, 22.0, 13.0)});
    const Point2D start{2.0, 10.0};
    const Point2D goal{28.0, 10.0};
    const StaticRoute route = routeStatic(regions, start, goal);

    verifySuccessfulRoute(regions, route, start, goal);
    QVERIFY(route.path.size() >= 4);
    QVERIFY(route.lengthM > 26.0);
}

void StaticSafeRouterTest::_testConcaveOuterBoundary()
{
    const Polygon2D concave = polygon(
        {{0.0, 0.0}, {20.0, 0.0}, {20.0, 20.0}, {12.0, 20.0}, {12.0, 8.0}, {8.0, 8.0}, {8.0, 20.0}, {0.0, 20.0}});
    const PolygonRegionSet2D regions = regionSet(concave);
    const Point2D start{4.0, 16.0};
    const Point2D goal{16.0, 16.0};
    const StaticRoute route = routeStatic(regions, start, goal);

    verifySuccessfulRoute(regions, route, start, goal);
    QCOMPARE(route.path.size(), std::size_t{4});
    QVERIFY(route.lengthM > 12.0);
}

void StaticSafeRouterTest::_testBoundaryAndTangentSemantics()
{
    const PolygonRegionSet2D plain = regionSet(rectangle(0.0, 0.0, 20.0, 20.0));
    const StaticRoute outerBoundary = routeStatic(plain, {0.0, 0.0}, {20.0, 0.0});
    verifySuccessfulRoute(plain, outerBoundary, {0.0, 0.0}, {20.0, 0.0});
    QCOMPARE(outerBoundary.path.size(), std::size_t{2});

    const PolygonRegionSet2D withHole = regionSet(rectangle(0.0, 0.0, 20.0, 20.0), {rectangle(8.0, 8.0, 12.0, 12.0)});
    const StaticRoute holeBoundary = routeStatic(withHole, {2.0, 8.0}, {18.0, 8.0});
    const StaticRoute vertexTangent = routeStatic(withHole, {2.0, 14.0}, {14.0, 2.0});
    const StaticRoute boundaryEndpoints = routeStatic(withHole, {8.0, 8.0}, {12.0, 8.0});
    verifySuccessfulRoute(withHole, holeBoundary, {2.0, 8.0}, {18.0, 8.0});
    verifySuccessfulRoute(withHole, vertexTangent, {2.0, 14.0}, {14.0, 2.0});
    verifySuccessfulRoute(withHole, boundaryEndpoints, {8.0, 8.0}, {12.0, 8.0});
    QCOMPARE(holeBoundary.path.size(), std::size_t{2});
    QCOMPARE(vertexTangent.path.size(), std::size_t{2});
    QCOMPARE(boundaryEndpoints.path.size(), std::size_t{2});
}

void StaticSafeRouterTest::_testNoRoute()
{
    const PolygonRegionSet2D disconnected = {
        {.outerBoundary = rectangle(0.0, 0.0, 4.0, 4.0)},
        {.outerBoundary = rectangle(10.0, 0.0, 14.0, 4.0)},
    };
    const StaticRoute route = routeStatic(disconnected, {2.0, 2.0}, {12.0, 2.0});

    QCOMPARE(route.status, PlanningStatus::Failed);
    QCOMPARE(route.error, CoveragePlanningError::SafeTransitNotFound);
    QVERIFY(route.path.empty());
    QCOMPARE(route.lengthM, 0.0);
}

void StaticSafeRouterTest::_testDeterminismAndInputOrdering()
{
    Polygon2D outer = rectangle(0.0, 0.0, 20.0, 20.0);
    Polygon2D hole = rectangle(8.0, 8.0, 12.0, 12.0);
    const PolygonRegionSet2D canonical = regionSet(outer, {hole});
    const StaticRoute first = routeStatic(canonical, {2.0, 10.0}, {18.0, 10.0});
    const StaticRoute second = routeStatic(canonical, {2.0, 10.0}, {18.0, 10.0});
    compareRoutes(first, second);

    std::reverse(outer.vertices.begin(), outer.vertices.end());
    std::reverse(hole.vertices.begin(), hole.vertices.end());
    std::rotate(hole.vertices.begin(), std::next(hole.vertices.begin()), hole.vertices.end());
    const StaticRoute reordered = routeStatic(regionSet(outer, {hole}), {2.0, 10.0}, {18.0, 10.0});
    compareRoutes(first, reordered);
}

void StaticSafeRouterTest::_testInvalidEndpoints()
{
    const PolygonRegionSet2D regions = regionSet(rectangle(0.0, 0.0, 20.0, 20.0), {rectangle(8.0, 8.0, 12.0, 12.0)});
    for (const StaticRoute& route :
         {routeStatic(regions, {-1.0, 10.0}, {2.0, 2.0}), routeStatic(regions, {2.0, 2.0}, {10.0, 10.0}),
          routeStatic(regions, {std::numeric_limits<double>::infinity(), 2.0}, {2.0, 2.0})}) {
        QCOMPARE(route.status, PlanningStatus::Failed);
        QCOMPARE(route.error, CoveragePlanningError::SafeTransitNotFound);
        QVERIFY(route.path.empty());
    }
}

UT_REGISTER_TEST_LIGHTWEIGHT(StaticSafeRouterTest, TestLabel::Unit)
