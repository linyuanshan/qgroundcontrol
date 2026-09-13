#pragma once

#include "UnitTest.h"

class CoverageProblemValidatorTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testPolygonValidation();
    void _testNumericValidation();
    void _testAngleNormalization();
    void _testNoGoCapabilityBoundary();
    void _testErrorMapping();
};
