#pragma once

#include "UnitTest.h"

class StaticSafeRouterTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testDirectVisibleRoute();
    void _testSingleNoGoDetourAndReverse();
    void _testMultipleNoGoDetour();
    void _testConcaveOuterBoundary();
    void _testBoundaryAndTangentSemantics();
    void _testNoRoute();
    void _testDeterminismAndInputOrdering();
    void _testInvalidEndpoints();
};
