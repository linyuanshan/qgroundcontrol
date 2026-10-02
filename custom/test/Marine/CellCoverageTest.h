#pragma once

#include "UnitTest.h"

class CellCoverageTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testSingleCellCoverageAndTraversalStates();
    void _testSharedLaneLatticeAcrossArtificialBoundary();
    void _testNoGoDecompositionCoversEveryCell();
    void _testBackendDerivedRoundedCells();
    void _testNonCardinalAndNarrowCells();
    void _testAlternateLaneParity();
    void _testFailureIsAtomic();
    void _testTargetTrackClipping();
    void _testTransitCanLeaveTargetAndUsesPathMetrics();
    void _testMultiIntervalSelectionIsDeterministic();
    void _testActiveTrackHoleIsRespected();
    void _testNoTrackOverlap();
    void _testTrackOverloadFailureIsAtomic();
    void _testTrackOverloadInputValidation();
};
