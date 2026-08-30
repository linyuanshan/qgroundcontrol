#pragma once

#include "UnitTest.h"

class MarinePlanIntegrationTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testPlanFileRoundTrip();
};
