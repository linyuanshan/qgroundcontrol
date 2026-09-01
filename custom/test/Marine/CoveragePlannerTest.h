#pragma once

#include "UnitTest.h"

class CoveragePlannerTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testRegisterAndLookup();
    void _testDuplicateId();
    void _testMissingPlanner();
    void _testValidPolygon();
    void _testInvalidPolygon();
    void _testDeterministicPath();
};
