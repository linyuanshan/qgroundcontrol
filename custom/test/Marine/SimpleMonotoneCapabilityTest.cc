#include "SimpleMonotoneCapabilityTest.h"

#include <limits>

#include "Geometry/PolygonRegion.h"
#include "Planning/SimpleMonotoneCapability.h"

using namespace Marine;

namespace {

Polygon2D rectangle(double widthM, double heightM)
{
    return {.vertices = {{0.0, 0.0}, {widthM, 0.0}, {widthM, heightM}, {0.0, heightM}}};
}

Polygon2D uShape()
{
    return {.vertices = {
                {0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {7.0, 10.0}, {7.0, 3.0}, {3.0, 3.0}, {3.0, 10.0}, {0.0, 10.0}}};
}

}  // namespace

void SimpleMonotoneCapabilityTest::_testRectangleAndConcaveMonotone()
{
    QCOMPARE(assessSimpleMonotoneCapability({{.outerBoundary = rectangle(10.0, 8.0)}}, 0.0).applicable, true);
    const PolygonRegionSet2D concave{{.outerBoundary = uShape()}};
    QCOMPARE(assessSimpleMonotoneCapability(concave, 0.0).applicable, true);
}

void SimpleMonotoneCapabilityTest::_testWrongAngleAndInvalidAngle()
{
    const PolygonRegionSet2D concave{{.outerBoundary = uShape()}};
    const auto wrong = assessSimpleMonotoneCapability(concave, 90.0);
    QVERIFY(!wrong.applicable);
    QCOMPARE(wrong.reason, PlannerResolutionReason::TargetNonMonotoneForSelectedSweep);
    QCOMPARE(assessSimpleMonotoneCapability(concave, std::numeric_limits<double>::quiet_NaN()).reason,
             PlannerResolutionReason::TargetNonMonotoneForSelectedSweep);
}

void SimpleMonotoneCapabilityTest::_testHoleAndMultipleComponentPrecedence()
{
    PolygonRegion2D withHole{.outerBoundary = rectangle(10.0, 10.0),
                             .holes = {{{.vertices = {{4.0, 4.0}, {4.0, 6.0}, {6.0, 6.0}, {6.0, 4.0}}}}}};
    const auto hole = assessSimpleMonotoneCapability({withHole}, 0.0);
    QVERIFY(!hole.applicable);
    QCOMPARE(hole.reason, PlannerResolutionReason::TargetHasHoles);
    const auto multiple = assessSimpleMonotoneCapability({withHole, {.outerBoundary = rectangle(5.0, 5.0)}}, 0.0);
    QCOMPARE(multiple.reason, PlannerResolutionReason::TargetHasMultipleComponents);
}

void SimpleMonotoneCapabilityTest::_testDeterministicAssessment()
{
    const PolygonRegionSet2D concave{{.outerBoundary = uShape()}};
    const auto first = assessSimpleMonotoneCapability(concave, 0.0);
    const auto second = assessSimpleMonotoneCapability(concave, 0.0);
    QCOMPARE(first.applicable, second.applicable);
    QCOMPARE(first.reason, second.reason);
    QCOMPARE(first.message, second.message);
}

UT_REGISTER_TEST_LIGHTWEIGHT(SimpleMonotoneCapabilityTest, TestLabel::Unit)
