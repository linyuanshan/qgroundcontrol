#include "GeometryTypesTest.h"

#include <limits>

#include "Geometry/GeometryTypes.h"

using namespace Marine;

void GeometryTypesTest::_testDefaults()
{
    const Point2D point;
    const Polygon2D polygon;
    const Region2D region;

    QCOMPARE(point.xM, 0.0);
    QCOMPARE(point.yM, 0.0);
    QVERIFY(point.isFinite());
    QVERIFY(polygon.vertices.empty());
    QVERIFY(polygon.isFinite());
    QVERIFY(region.outerBoundary.vertices.empty());
    QVERIFY(region.noGoRegions.empty());
    QVERIFY(region.isFinite());
}

void GeometryTypesTest::_testFiniteValidation()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();

    QVERIFY((Point2D{1.0, -2.0}.isFinite()));
    QVERIFY((!Point2D{nan, 0.0}.isFinite()));
    QVERIFY((!Point2D{0.0, infinity}.isFinite()));

    Polygon2D polygon{{{0.0, 0.0}, {10.0, 0.0}, {0.0, 10.0}}};
    QVERIFY(polygon.isFinite());
    polygon.vertices[1].xM = nan;
    QVERIFY(!polygon.isFinite());

    Region2D region;
    region.outerBoundary = {{{0.0, 0.0}, {10.0, 0.0}, {0.0, 10.0}}};
    region.noGoRegions.push_back({{{1.0, 1.0}, {2.0, 1.0}, {1.0, 2.0}}});
    QVERIFY(region.isFinite());
    region.noGoRegions.front().vertices.front().yM = infinity;
    QVERIFY(!region.isFinite());
}

UT_REGISTER_TEST_LIGHTWEIGHT(GeometryTypesTest, TestLabel::Unit)
