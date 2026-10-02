#pragma once

#include "UnitTest.h"

class AutoCoveragePlannerTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testManualRectangleResolvesSimpleMonotone();
    void _testConcaveMonotoneResolvesSimpleMonotone();
    void _testCapabilityEscalationDelegatesToBcd();
    void _testNoGoOutsideCoverageDoesNotEscalate();
    void _testCoverageAssessmentAndSafetyFailureDoNotEscalate();
    void _testBcdFailurePreservesResolvedStrategyProvenance();
    void _testAutoSweepIsDeterministicAndDistinctFromPlannerAuto();
    void _testAutoSweepEscalationSelectsOnce();
};
