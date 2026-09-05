#pragma once

#include "UnitTest.h"

class LawnmowerCoveragePlannerTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testMetadata();
    void _testRectangleManualZeroDegrees();
    void _testRectangleManualNinetyDegrees();
    void _testNarrowRegionUsesOneLane();
    void _testPositiveSafetyMargin();
    void _testGeneralConvexDeterminism();
    void _testSafetyInsetFailure();
    void _testNonMonotoneSweepFailure();
    void _testUnsafeConnectorFailure();
    void _testSuccessfulPathInvariants();
    void _testAutoModeIsDeferred();
};
