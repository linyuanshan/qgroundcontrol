#pragma once

#include "UnitTest.h"

class PlanningInputIdentityTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testPlanningFields();
    void _testCanonicalGeometry();
    void _testNonPlanningFields();
    void _testSemanticIdentity();
    void _testInvalidIdentity();
};
