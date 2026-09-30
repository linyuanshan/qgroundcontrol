#pragma once

#include "UnitTest.h"

class CoverageTaskAdapterTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testBuildProblem();
    void _testInvalidTaskGeometry();
    void _testValidationAndNormalization();
    void _testNoGoConversion();
    void _testSeparateNavigationConversion();
    void _testSolutionMapping();
    void _testTaskPlannerRoundTrip();
    void _testOptionalClosingVertex();
    void _testRejectInvalidSuccessPath();
    void _testPathMetricsConsistencyTolerance();
};
