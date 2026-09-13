#pragma once

#include "UnitTest.h"

class MonotoneCoverageTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testFixedAngleCoverageAndRoles();
    void _testSingleLaneCoverage();
    void _testConvenienceSchedule();
    void _testUnsafeConnector();
    void _testInvalidLaneSchedule();
    void _testInvalidTarget();
};
