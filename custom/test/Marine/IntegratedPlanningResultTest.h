#pragma once

#include "BaseClasses/MissionTest.h"

class IntegratedPlanningResultTest : public OfflineMissionTest
{
    Q_OBJECT
private slots:
    void _testT01();
    void _testT02();
    void _testT03();
    void _testT04();
    void _testT05();
    void _testT06();
    void _testT07();
    void _testT08();
    void _testT09();
    void _testT10();
    void _testT11();
    void _testT12();
    void _testT13();
    void _testT14();
    void _testT15();
    void _testT16();
    void _testT17();
    void _testT18();
    void _testT19();
    void _testT20();
    void _testCoverageRequirementBinding();
    void _testRepairEvaluationConsistency();
    void _testFailedCurrentOutcome();
    void _testPlannerSourceBinding_data();
    void _testPlannerSourceBinding();
    void _testPlannerSourceCurrentAndStale();
};
