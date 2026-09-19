#pragma once

#include "UnitTest.h"

class GreedyCellOrderingTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testSingleCell();
    void _testTwoCellsDirectTransit();
    void _testOrientationAffectsTransitDistance();
    void _testGreedyNearestSelection();
    void _testNoGoDetour();
    void _testForwardUnreachableReverseReachable();
    void _testRemainingCellUnreachable();
    void _testEqualDistanceTieBreak();
    void _testInputOrderingIndependence();
    void _testRepeatedRunsDeterministic();
};
