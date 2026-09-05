#pragma once

#include "UnitTest.h"

class CoveragePlannerTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testRegisterAndLookup();
    void _testDuplicateId();
    void _testMissingPlanner();
    void _testValidProblem();
    void _testInvalidProblem();
    void _testCapabilityGate();
    void _testDeterministicPath();
};
