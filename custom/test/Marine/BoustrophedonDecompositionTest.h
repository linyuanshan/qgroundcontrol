#pragma once

#include "UnitTest.h"

class BoustrophedonDecompositionTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testRectAndSimpleConcaveRegions();
    void _testCShapeSplitsAndMerges();
    void _testHoleAndTwoHoleTopology();
    void _testExplicitSplitAndMergeTopology();
    void _testCollinearAndNearEqualEvents();
    void _testDeterminismAndInputOrdering();
    void _testNonCardinalSweep();
    void _testInvalidInputs();
    void _testOutputInvariants();
    void _testRoundedHoleEndToEnd();
};
