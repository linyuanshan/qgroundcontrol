#pragma once

#include "UnitTest.h"

class ComplexCoverageAssemblyTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testSingleCellForwardAndReverse();
    void _testDirectAndReverseTransit();
    void _testVisibilityGraphTransitAndRoles();
    void _testSharedEndpointsMetricsAndTurns();
    void _testDeterminismAndCoverageInputOrder();
    void _testInvalidInputsAreAtomic();
    void _testNoGoVerticalSlice();
};
