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
    void _testGeoRoundTripS04Regression();
};
