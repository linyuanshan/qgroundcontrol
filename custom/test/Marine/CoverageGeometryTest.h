#pragma once

#include "UnitTest.h"

class CoverageGeometryTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testRegionSets_data();
    void _testRegionSets();
    void _testInvalidTopology_data();
    void _testInvalidTopology();
    void _testNavigationOutsideCoverage();
    void _testHistoricalPlannerBoundary();
};
