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
    void _testM00CanonicalReady();
    void _testM01ConcaveMonotone();
    void _testM02TopologyEscalation();
    void _testM03NavigationOutsideCoverage();
    void _testM04HardSafePreferredWarning();
    void _testM05RawDiagnosticOnly();
    void _testM06StandardStrictSameGeometry();
    void _testM07RepairSuccess();
    void _testM08RepairExhausted();
    void _testM09UnsupportedAndInvalid();
    void _testCoverageRequirementBinding();
    void _testRepairEvaluationConsistency();
    void _testFailedCurrentOutcome();
    void _testPlannerSourceBinding_data();
    void _testPlannerSourceBinding();
    void _testPlannerSourceCurrentAndStale();
};
