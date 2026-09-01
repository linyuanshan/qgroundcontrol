#pragma once

#include "UnitTest.h"

class MarineTaskModelTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testDefaults();
    void _testTaskId();
    void _testWorkRegion();
    void _testConfiguration();
    void _testValidity();
};
