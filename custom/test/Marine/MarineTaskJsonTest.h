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
    void _testMissingRequiredField_data();
    void _testMissingRequiredField();
    void _testInvalidField_data();
    void _testInvalidField();
    void _testInvalidSave();
    void _testManualAngleNormalization();
    void _testInvalidCoordinates();
    void _testNoTopologyValidation();
};
