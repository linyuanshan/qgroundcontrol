#pragma once

#include "UnitTest.h"

class SimpleMonotoneCapabilityTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testRectangleAndConcaveMonotone();
    void _testWrongAngleAndInvalidAngle();
    void _testHoleAndMultipleComponentPrecedence();
    void _testDeterministicAssessment();
};
