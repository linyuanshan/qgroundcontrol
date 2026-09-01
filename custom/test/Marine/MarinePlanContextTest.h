#pragma once

#include "UnitTest.h"

class MarinePlanContextTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testControllerOwnsContext();
    void _testAddAndFind();
    void _testUpdateNotifies();
    void _testDuplicateIdReplaces();
    void _testEmptyIdIgnored();
    void _testRemoveAndClear();
    void _testContextsAreIsolated();
    void _testPlannerRegistriesAreIsolated();
};
