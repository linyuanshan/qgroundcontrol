#pragma once

#include "BaseClasses/MissionTest.h"

class CustomPluginIntegrationTest : public OfflineMissionTest
{
    Q_OBJECT

protected:
    void init() final;

private slots:
    void _testPlanContextAndCreatorRegistration();
    void _testComplexItemMenuRegistration();
    void _testFactoryRegistrationAndContextReuse();
    void _testMarineEntriesFilteredForNonMarineVehicle();
    void _testNullVehicleMenuHandled();
    void _testMarinePlanSaveFiltersOrphans();
    void _testMarinePlanPreload();
    void _testMarinePlanPreloadValidation();
};
