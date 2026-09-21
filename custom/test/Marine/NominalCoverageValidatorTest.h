#pragma once

#include "UnitTest.h"

class NominalCoverageValidatorTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testRectangleAndUncoveredStrip();
    void _testCoverageRolesAndDisconnectedLegs();
    void _testRoundCapsAndAreaTolerance();
    void _testNoGoTargetSemantics();
    void _testMalformedInputAndDeterminism();
    void _testBackendMinimumAndZeroLengthCoverageLegs();
    void _testArtificialGapRegression();
    void _testP2CVerticalSlice();
    void _testHalfSwathSafetyPrecheck();
};
