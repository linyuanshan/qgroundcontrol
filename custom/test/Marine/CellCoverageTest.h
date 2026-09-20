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
    void _testFailureIsAtomic();
};
