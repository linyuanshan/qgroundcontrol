#pragma once

#include "UnitTest.h"

class MonotoneCoverageTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testFixedAngleCoverageAndRoles();
    void _testSingleLaneCoverage();
    void _testConvenienceSchedule();
    void _testAutomaticTargetAndNavigableSchedule();
    void _testImpossibleAutomaticSchedule();
    void _testExplicitNonMonotoneSweep();
    void _testUnsafeConnector();
    void _testInvalidLaneSchedule();
    void _testInvalidTarget();
    void _testStrictNearDuplicateInputRejected();
};
