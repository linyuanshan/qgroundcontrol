#pragma once

#include "UnitTest.h"

class BoustrophedonCoveragePlannerTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testRepresentativeManualCases();
    void _testManualNormalizationAndDeterminism();
    void _testAutoSelectionAndInputOrder();
    void _testFailurePropagation();
    void _testExecutionSafeScenarios();
    void _testCoverageNavigationSeparation();
    void _testCoverageFirstCandidateRanking();
    void _testDirectAutoSweepAndFailureSource();
    void _testGeoRoundTripS04Regression();
    void _testGeoAdapterMarinePlanRoundTripRegression();
};
