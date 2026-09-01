#pragma once

#include "UnitTest.h"

class MarineTaskJsonTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testRoundTrip();
    void _testNoGoRegions();
    void _testSensorConfig();
    void _testUnknownField();
    void _testUnsupportedVersion();
    void _testMissingId();
    void _testMissingRegion();
};
