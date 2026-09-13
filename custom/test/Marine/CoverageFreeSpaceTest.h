#pragma once

#include "UnitTest.h"

class CoverageFreeSpaceTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testValidNoGoValidation();
    void _testOuterBoundaryConflicts();
    void _testNoGoPairConflicts();
    void _testValidationErrorMapping();
    void _testCoverageTargetAndTrackFeasibleRegion();
    void _testInflationMergesNoGo();
    void _testDisconnectedFeasibleRegion();
    void _testNoNavigableArea();
    void _testSafetyExceedsHalfSwath();
    void _testUnreachableCoverage();
    void _testDeterminism();
};
