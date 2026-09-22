#pragma once

#include "UnitTest.h"

class MarineSITLValidationTest : public UnitTest
{
    Q_OBJECT

private slots:
    void init() override;
    void cleanup() override;
    void _validateP2Scenarios();
};
