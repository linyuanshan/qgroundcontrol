#pragma once

#include "UnitTest.h"

class CoverageTaskAdapterTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testBuildProblem();
    void _testInvalidTaskGeometry();
    void _testValidationAndNormalization();
    void _testUnsupportedNoGoRegion();
    void _testSolutionMapping();
    void _testTaskPlannerRoundTrip();
};
