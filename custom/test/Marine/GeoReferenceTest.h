#pragma once

#include "UnitTest.h"

class GeoReferenceTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testRegionOrigin();
    void _testKnownDistance();
    void _testRoundTrip();
    void _testDifferentHeadings();
    void _testInvalidInput();
};
