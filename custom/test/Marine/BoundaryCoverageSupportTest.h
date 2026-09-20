#pragma once

#include "UnitTest.h"

class BoundaryCoverageSupportTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testRectangleComponent();
    void _testDeterminismAndRoundedHole();
    void _testInvalidInputIsAtomic();
};
