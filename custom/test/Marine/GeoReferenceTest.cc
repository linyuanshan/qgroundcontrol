#include "GeoReferenceTest.h"

#include <array>
#include <limits>

#include "Geometry/GeoReference.h"

using namespace Marine;

void GeoReferenceTest::_testRegionOrigin()
{
    const GeoPolygon region{{
        {47.0, 8.0, 12.0},
        {47.1, 8.2, 18.0},
        {47.2, 8.1, 24.0},
    }};

    const std::optional<GeoReference> reference = GeoReference::create(region);

    QVERIFY(reference.has_value());
    QCOMPARE(reference->origin().latitudeDeg, 47.1);
    QCOMPARE(reference->origin().longitudeDeg, 8.1);
    QCOMPARE(reference->origin().altitudeM, 0.0);
}

void GeoReferenceTest::_testKnownDistance()
{
    const std::optional<GeoReference> reference = GeoReference::create({47.3764, 8.5481, 0.0});
    QVERIFY(reference.has_value());

    const std::optional<Point2D> local = reference->toLocal({47.364869, 8.594398, 0.0});

    QVERIFY(local.has_value());
    QVERIFY(qAbs(local->xM - 3497.196961) < 0.01);
    QVERIFY(qAbs(local->yM - (-1280.954612)) < 0.01);
}

void GeoReferenceTest::_testRoundTrip()
{
    const std::optional<GeoReference> reference = GeoReference::create({47.3764, 8.5481, 0.0});
    QVERIFY(reference.has_value());

    const GeoPoint input{47.364869, 8.594398, 25.0};
    const std::optional<Point2D> local = reference->toLocal(input);
    QVERIFY(local.has_value());

    const std::optional<GeoPoint> roundTrip = reference->toGeo(*local);
    QVERIFY(roundTrip.has_value());
    QVERIFY(qAbs(roundTrip->latitudeDeg - input.latitudeDeg) < 1e-9);
    QVERIFY(qAbs(roundTrip->longitudeDeg - input.longitudeDeg) < 1e-9);
    QCOMPARE(roundTrip->altitudeM, 0.0);
}

void GeoReferenceTest::_testDifferentHeadings()
{
    const std::optional<GeoReference> reference = GeoReference::create({0.0, 0.0, 0.0});
    QVERIFY(reference.has_value());

    const std::array<Point2D, 4> points{{
        {100.0, 0.0},
        {0.0, 100.0},
        {-100.0, 0.0},
        {0.0, -100.0},
    }};

    for (const Point2D& point : points) {
        const std::optional<GeoPoint> geo = reference->toGeo(point);
        QVERIFY(geo.has_value());
        const std::optional<Point2D> roundTrip = reference->toLocal(*geo);
        QVERIFY(roundTrip.has_value());
        QVERIFY(qAbs(roundTrip->xM - point.xM) < 1e-6);
        QVERIFY(qAbs(roundTrip->yM - point.yM) < 1e-6);
    }

    QVERIFY(reference->toGeo(points[0])->longitudeDeg > 0.0);
    QVERIFY(reference->toGeo(points[1])->latitudeDeg > 0.0);
    QVERIFY(reference->toGeo(points[2])->longitudeDeg < 0.0);
    QVERIFY(reference->toGeo(points[3])->latitudeDeg < 0.0);
}

void GeoReferenceTest::_testInvalidInput()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();

    QVERIFY(!GeoReference::create(GeoPolygon{}).has_value());
    QVERIFY(!GeoReference::create(GeoPoint{nan, 0.0, 0.0}).has_value());
    QVERIFY(!GeoReference::create(GeoPoint{91.0, 0.0, 0.0}).has_value());
    QVERIFY(!GeoReference::create(GeoPoint{0.0, 181.0, 0.0}).has_value());
    QVERIFY(!GeoReference::create(GeoPolygon{{{0.0, 0.0, 0.0}, {0.0, infinity, 0.0}}}).has_value());

    const std::optional<GeoReference> reference = GeoReference::create({47.0, 8.0, 0.0});
    QVERIFY(reference.has_value());
    QVERIFY(!reference->toLocal({nan, 8.0, 0.0}).has_value());
    QVERIFY(!reference->toLocal({47.0, 181.0, 0.0}).has_value());
    QVERIFY(!reference->toLocal({47.0, 8.0, infinity}).has_value());
    QVERIFY(!reference->toGeo({nan, 0.0}).has_value());
    QVERIFY(!reference->toGeo({0.0, infinity}).has_value());
}

UT_REGISTER_TEST_LIGHTWEIGHT(GeoReferenceTest, TestLabel::Unit)
