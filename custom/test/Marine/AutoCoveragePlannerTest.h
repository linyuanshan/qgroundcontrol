#pragma once

#include "UnitTest.h"

class AutoCoveragePlannerTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testManualRectangleResolvesSimpleMonotone();
    void _testConcaveMonotoneResolvesSimpleMonotone();
    void _testCapabilityEscalationIsPendingBcd();
    void _testNoGoOutsideCoverageDoesNotEscalate();
    void _testCoverageAssessmentAndSafetyFailureDoNotEscalate();
    void _testAutoSweepIsDeterministicAndDistinctFromPlannerAuto();
};
