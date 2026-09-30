#pragma once

#include "UnitTest.h"

class CoverageSafetyTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testMargins_data();
    void _testMargins();
    void _testInvalidMargins_data();
    void _testInvalidMargins();
    void _testSplitAndHoles();
    void _testEntirePath_data();
    void _testEntirePath();
    void _testFallback();
    void _testEmptyAndFailure();
    void _testHierarchyFailure();
    void _testSubMillimeterClearance();
};
