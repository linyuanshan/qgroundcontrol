#pragma once

#include "UnitTest.h"

class CoverageQualityEvaluatorTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testCompleteStandardAndStrict();
    void _testStrictAndStandardShareCompleteEvaluationTruth();
    void _testBoundaryOnlyShortfall();
    void _testInternalCriticalGap();
    void _testCoverageRatioGate();
    void _testStrategyIdentityDoesNotChangeQualityTruth();
    void _testTransitDoesNotCover();
    void _testOverlappingFootprintsCountOnce();
    void _testFootprintOutsideTargetIsClipped();
    void _testHoleCriticalCore();
    void _testMultipleComponentsAndFallbacks();
    void _testTransitOnlyIsReliableInsufficient();
    void _testMalformedInputs();
    void _testUnsupportedPolicy();
    void _testNumericalToleranceBranches();
    void _testResidualAreaConsistencyAndDeterminism();
    void _testComparatorOrderingAndEquivalence();
    void _testComparatorErrorAndTransitivity();
};
