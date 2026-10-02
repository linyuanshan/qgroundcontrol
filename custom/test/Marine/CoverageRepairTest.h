#pragma once

#include "UnitTest.h"

class CoverageRepairTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testTargetRelativeSupport();
    void _testAssessmentErrorAndInitialPass();
    void _testSuccessfulIncrementalRepair();
    void _testFiniteInsufficientAndNoUsefulRepair();
    void _testHoleAndRouteAwareOrdering();
    void _testStableTrialKeys();
    void _testRepairDeterminism();
    void _testPlannerInitialPassAndStandardStrict();
    void _testAllCandidatesAndHardPassPriority();
    void _testAutoNoEscalationAndBcdIntegration();
};
