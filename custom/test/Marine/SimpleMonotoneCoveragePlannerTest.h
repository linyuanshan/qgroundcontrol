#pragma once

#include "UnitTest.h"

class SimpleMonotoneCoveragePlannerTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testMetadataAndRectangle();
    void _testConcaveTargetAndCoverageRelativeLanes();
    void _testNavigationOutsideTargetUsedOnlyForTransit();
    void _testExecutionMarginAndPreferredTier();
    void _testHardPassOutranksPreferredInsufficient();
    void _testIncompleteAndAssessmentErrorDoNotReturnPath();
    void _testDirectCapabilityFailureDoesNotEscalate();
    void _testDeterministicPathAndSafety();
};
