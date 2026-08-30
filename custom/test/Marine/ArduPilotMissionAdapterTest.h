#pragma once

#include "UnitTest.h"

class ArduPilotMissionAdapterTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testAppendWaypoints();
    void _testRejectsUnsuccessfulResult();
    void _testRejectsInvalidPath();
};
