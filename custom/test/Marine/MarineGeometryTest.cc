#include "MarineGeometryTest.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Geometry/MarineGeometry.h"

using namespace Marine;

namespace {

struct Bounds
{
    double minimumX = std::numeric_limits<double>::max();
    double minimumY = std::numeric_limits<double>::max();
    double maximumX = std::numeric_limits<double>::lowest();
    double maximumY = std::numeric_limits<double>::lowest();
};

Bounds boundsFor(const Polygon2D& polygon)
{
    Bounds bounds;
    for (const Point2D& vertex : polygon.vertices) {
        bounds.minimumX = std::min(bounds.minimumX, vertex.xM);
        bounds.minimumY = std::min(bounds.minimumY, vertex.yM);
        bounds.maximumX = std::max(bounds.maximumX, vertex.xM);
        bounds.maximumY = std::max(bounds.maximumY, vertex.yM);
    }
    return bounds;
}

void compareWithinTolerance(double actual, double expected)
{
    QVERIFY2(std::abs(actual - expected) <= Geometry::LengthEpsilonM, "Coordinate exceeded geometry tolerance");
}

Polygon2D rectangle()
{
    return {{{0.0, 0.0}, {20.0, 0.0}, {20.0, 10.0}, {0.0, 10.0}}};
}

}  // namespace

void MarineGeometryTest::_testRectangleInset()
{
    const Geometry::PolygonInsetResult result = Geometry::insetPolygon(rectangle(), 2.0);

    QCOMPARE(result.status, Geometry::PolygonInsetStatus::Success);
    QCOMPARE(result.polygon.vertices.size(), 4);
    const Bounds bounds = boundsFor(result.polygon);
    compareWithinTolerance(bounds.minimumX, 2.0);
    compareWithinTolerance(bounds.minimumY, 2.0);
    compareWithinTolerance(bounds.maximumX, 18.0);
    compareWithinTolerance(bounds.maximumY, 8.0);
    QVERIFY(Geometry::isSimpleNonDegeneratePolygon(result.polygon));
}

void MarineGeometryTest::_testZeroMargin()
{
    const Polygon2D input = rectangle();
    const Geometry::PolygonInsetResult result = Geometry::insetPolygon(input, 0.0);

    QCOMPARE(result.status, Geometry::PolygonInsetStatus::Success);
    QCOMPARE(result.polygon.vertices.size(), input.vertices.size());
    for (std::size_t index = 0; index < input.vertices.size(); ++index) {
        QCOMPARE(result.polygon.vertices[index].xM, input.vertices[index].xM);
        QCOMPARE(result.polygon.vertices[index].yM, input.vertices[index].yM);
    }
}

void MarineGeometryTest::_testEmptyInset()
{
    const Polygon2D narrowRegion{{{0.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}, {0.0, 4.0}}};
    const Geometry::PolygonInsetResult result = Geometry::insetPolygon(narrowRegion, 2.0);

    QCOMPARE(result.status, Geometry::PolygonInsetStatus::Empty);
    QVERIFY(result.polygon.vertices.empty());
}

void MarineGeometryTest::_testDisconnectedInset()
{
    const Polygon2D dumbbell{{{0.0, 0.0},
                              {10.0, 0.0},
                              {10.0, 4.0},
                              {20.0, 4.0},
                              {20.0, 0.0},
                              {30.0, 0.0},
                              {30.0, 10.0},
                              {20.0, 10.0},
                              {20.0, 6.0},
                              {10.0, 6.0},
                              {10.0, 10.0},
                              {0.0, 10.0}}};
    const Geometry::PolygonInsetResult result = Geometry::insetPolygon(dumbbell, 1.1);

    QCOMPARE(result.status, Geometry::PolygonInsetStatus::Disconnected);
    QVERIFY(result.polygon.vertices.empty());
}

void MarineGeometryTest::_testOrientationIndependence()
{
    Polygon2D clockwise = rectangle();
    std::reverse(clockwise.vertices.begin(), clockwise.vertices.end());

    const Geometry::PolygonInsetResult result = Geometry::insetPolygon(clockwise, 2.0);

    QCOMPARE(result.status, Geometry::PolygonInsetStatus::Success);
    const Bounds bounds = boundsFor(result.polygon);
    compareWithinTolerance(bounds.minimumX, 2.0);
    compareWithinTolerance(bounds.minimumY, 2.0);
    compareWithinTolerance(bounds.maximumX, 18.0);
    compareWithinTolerance(bounds.maximumY, 8.0);
}

void MarineGeometryTest::_testInvalidInput()
{
    Polygon2D invalid{{{0.0, 0.0}, {10.0, 10.0}, {0.0, 10.0}, {10.0, 0.0}}};
    QCOMPARE(Geometry::insetPolygon(invalid, 1.0).status, Geometry::PolygonInsetStatus::InvalidInput);
    QCOMPARE(Geometry::insetPolygon(rectangle(), -1.0).status, Geometry::PolygonInsetStatus::InvalidInput);
    QCOMPARE(Geometry::insetPolygon(rectangle(), std::numeric_limits<double>::infinity()).status,
             Geometry::PolygonInsetStatus::InvalidInput);
}

void MarineGeometryTest::_testDeterminismAndPrecision()
{
    const Polygon2D input{{{0.1234, 0.5678}, {20.1234, 0.5678}, {20.1234, 10.5678}, {0.1234, 10.5678}}};
    const Geometry::PolygonInsetResult first = Geometry::insetPolygon(input, 1.234);
    const Geometry::PolygonInsetResult second = Geometry::insetPolygon(input, 1.234);

    QCOMPARE(first.status, Geometry::PolygonInsetStatus::Success);
    QCOMPARE(second.status, Geometry::PolygonInsetStatus::Success);
    QCOMPARE(first.polygon.vertices.size(), second.polygon.vertices.size());
    for (std::size_t index = 0; index < first.polygon.vertices.size(); ++index) {
        QCOMPARE(first.polygon.vertices[index].xM, second.polygon.vertices[index].xM);
        QCOMPARE(first.polygon.vertices[index].yM, second.polygon.vertices[index].yM);
    }

    const Bounds bounds = boundsFor(first.polygon);
    compareWithinTolerance(bounds.minimumX, 1.357);
    compareWithinTolerance(bounds.minimumY, 1.802);
    compareWithinTolerance(bounds.maximumX, 18.889);
    compareWithinTolerance(bounds.maximumY, 9.334);
}

void MarineGeometryTest::_testSweepFrameTransform()
{
    compareWithinTolerance(Geometry::navigationAngleToMathAngle(0.0), 90.0);
    compareWithinTolerance(Geometry::navigationAngleToMathAngle(90.0), 0.0);
    compareWithinTolerance(Geometry::mathAngleToNavigationAngle(30.0), 60.0);

    const Point2D input{3.0, 4.0};
    const Point2D sweepPoint = Geometry::toSweepFrame(input, 90.0);
    compareWithinTolerance(sweepPoint.xM, 4.0);
    compareWithinTolerance(sweepPoint.yM, -3.0);

    const Point2D restored = Geometry::fromSweepFrame(sweepPoint, 90.0);
    compareWithinTolerance(restored.xM, input.xM);
    compareWithinTolerance(restored.yM, input.yM);
}

void MarineGeometryTest::_testPolygonMonotonicity()
{
    const Polygon2D convex{{{0.0, 0.0}, {8.0, 1.0}, {10.0, 6.0}, {5.0, 10.0}, {-1.0, 7.0}}};
    QVERIFY(Geometry::isSweepMonotone(convex, 0.0));
    QVERIFY(Geometry::isSweepMonotone(convex, 37.0));
    QVERIFY(Geometry::isSweepMonotone(convex, 90.0));

    const Polygon2D lShape{{{0.0, 0.0}, {8.0, 0.0}, {8.0, 3.0}, {3.0, 3.0}, {3.0, 8.0}, {0.0, 8.0}}};
    QVERIFY(Geometry::isSweepMonotone(lShape, 0.0));

    const Polygon2D cShape{
        {{0.0, 0.0}, {10.0, 0.0}, {10.0, 3.0}, {3.0, 3.0}, {3.0, 7.0}, {10.0, 7.0}, {10.0, 10.0}, {0.0, 10.0}}};
    QVERIFY(Geometry::isSweepMonotone(cShape, 0.0));
    QVERIFY(!Geometry::isSweepMonotone(cShape, 90.0));

    Polygon2D clockwise = lShape;
    std::reverse(clockwise.vertices.begin(), clockwise.vertices.end());
    QCOMPARE(Geometry::isSweepMonotone(clockwise, 0.0), Geometry::isSweepMonotone(lShape, 0.0));
}

void MarineGeometryTest::_testScanlineIntervals()
{
    const Geometry::ScanlineResult rectangleResult = Geometry::intersectScanline(rectangle(), 5.0);
    QCOMPARE(rectangleResult.status, Geometry::ScanlineStatus::Success);
    QCOMPARE(rectangleResult.intervals.size(), 1);
    compareWithinTolerance(rectangleResult.intervals[0].minimumXM, 0.0);
    compareWithinTolerance(rectangleResult.intervals[0].maximumXM, 20.0);

    const Polygon2D cShape{
        {{0.0, 0.0}, {10.0, 0.0}, {10.0, 3.0}, {3.0, 3.0}, {3.0, 7.0}, {10.0, 7.0}, {10.0, 10.0}, {0.0, 10.0}}};
    const Geometry::ScanlineResult cShapeHorizontal = Geometry::intersectScanline(cShape, 5.0);
    QCOMPARE(cShapeHorizontal.status, Geometry::ScanlineStatus::Success);
    QCOMPARE(cShapeHorizontal.intervals.size(), 1);
    compareWithinTolerance(cShapeHorizontal.intervals[0].minimumXM, 0.0);
    compareWithinTolerance(cShapeHorizontal.intervals[0].maximumXM, 3.0);

    const Polygon2D rotatedCShape = Geometry::toSweepFrame(cShape, 90.0);
    const Geometry::ScanlineResult cShapeVertical = Geometry::intersectScanline(rotatedCShape, -5.0);
    QCOMPARE(cShapeVertical.status, Geometry::ScanlineStatus::Success);
    QCOMPARE(cShapeVertical.intervals.size(), 2);
    compareWithinTolerance(cShapeVertical.intervals[0].minimumXM, 0.0);
    compareWithinTolerance(cShapeVertical.intervals[0].maximumXM, 3.0);
    compareWithinTolerance(cShapeVertical.intervals[1].minimumXM, 7.0);
    compareWithinTolerance(cShapeVertical.intervals[1].maximumXM, 10.0);

    const Polygon2D uShape{
        {{0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {7.0, 10.0}, {7.0, 3.0}, {3.0, 3.0}, {3.0, 10.0}, {0.0, 10.0}}};
    const Geometry::ScanlineResult twoIntervals = Geometry::intersectScanline(uShape, 5.0);
    QCOMPARE(twoIntervals.status, Geometry::ScanlineStatus::Success);
    QCOMPARE(twoIntervals.intervals.size(), 2);
    compareWithinTolerance(twoIntervals.intervals[0].minimumXM, 0.0);
    compareWithinTolerance(twoIntervals.intervals[0].maximumXM, 3.0);
    compareWithinTolerance(twoIntervals.intervals[1].minimumXM, 7.0);
    compareWithinTolerance(twoIntervals.intervals[1].maximumXM, 10.0);

    const Geometry::ScanlineResult outside = Geometry::intersectScanline(rectangle(), 20.0);
    QCOMPARE(outside.status, Geometry::ScanlineStatus::NoIntersection);
    QVERIFY(outside.intervals.empty());
}

void MarineGeometryTest::_testScanlineVertexAndBoundaryCases()
{
    const Polygon2D diamond{{{0.0, 5.0}, {5.0, 0.0}, {10.0, 5.0}, {5.0, 10.0}}};
    const Geometry::ScanlineResult throughVertices = Geometry::intersectScanline(diamond, 5.0);
    QCOMPARE(throughVertices.status, Geometry::ScanlineStatus::Success);
    QCOMPARE(throughVertices.intervals.size(), 1);
    compareWithinTolerance(throughVertices.intervals[0].minimumXM, 0.0);
    compareWithinTolerance(throughVertices.intervals[0].maximumXM, 10.0);

    const Geometry::ScanlineResult onBoundary = Geometry::intersectScanline(rectangle(), 0.0);
    QCOMPARE(onBoundary.status, Geometry::ScanlineStatus::Success);
    QCOMPARE(onBoundary.intervals.size(), 1);
    compareWithinTolerance(onBoundary.intervals[0].minimumXM, 0.0);
    compareWithinTolerance(onBoundary.intervals[0].maximumXM, 20.0);

    Polygon2D invalid{{{0.0, 0.0}, {10.0, 10.0}, {0.0, 10.0}, {10.0, 0.0}}};
    QCOMPARE(Geometry::intersectScanline(invalid, 5.0).status, Geometry::ScanlineStatus::InvalidInput);
    QCOMPARE(Geometry::intersectScanline(rectangle(), std::numeric_limits<double>::infinity()).status,
             Geometry::ScanlineStatus::InvalidInput);
}

void MarineGeometryTest::_testPointContainment()
{
    const Polygon2D polygon = rectangle();

    QVERIFY(Geometry::containsPoint(polygon, {10.0, 5.0}));
    QVERIFY(Geometry::containsPoint(polygon, {0.0, 5.0}));
    QVERIFY(Geometry::containsPoint(polygon, {0.0, 0.0}));
    QVERIFY(!Geometry::containsPoint(polygon, {-1.0, 5.0}));
    QVERIFY(!Geometry::containsPoint(polygon, {std::numeric_limits<double>::infinity(), 5.0}));
}

void MarineGeometryTest::_testSegmentContainment()
{
    const Polygon2D concave{{{0.0, 0.0}, {10.0, 0.0}, {10.0, 1.5}, {3.0, 3.0}, {10.0, 4.5}, {10.0, 10.0}, {0.0, 10.0}}};

    QVERIFY(Geometry::containsSegment(concave, {0.0, 1.0}, {8.0, 1.0}));
    QVERIFY(Geometry::containsSegment(concave, {0.0, 0.0}, {10.0, 0.0}));
    QVERIFY(Geometry::containsSegment(concave, {1.0, 1.0}, {1.0, 9.0}));
    QVERIFY(!Geometry::containsSegment(concave, {9.0, 1.0}, {9.0, 5.0}));
    QVERIFY(!Geometry::containsSegment(concave, {1.0, 1.0}, {11.0, 1.0}));
}

UT_REGISTER_TEST_LIGHTWEIGHT(MarineGeometryTest, TestLabel::Unit)
