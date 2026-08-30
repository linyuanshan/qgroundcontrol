#pragma once

#include "BaseClasses/MissionTest.h"

namespace Marine {
class MarinePlanContext;
}

class CoverageInspectionComplexItem;

class CoverageComplexItemTest : public OfflineMissionTest
{
    Q_OBJECT

protected:
    void init() final;
    void cleanup() final;

private slots:
    void _testDefaults();
    void _testPlanning();
    void _testPlanningFailures();
    void _testInvalidation();
    void _testQmlRegistration();
    void _testQmlTaskProperties();
    void _testSaveLoad();
    void _testLoadValidation();

private:
    Marine::MarinePlanContext* _marineContext = nullptr;
    CoverageInspectionComplexItem* _item = nullptr;
};
