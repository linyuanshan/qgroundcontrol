#pragma once

#include "BaseClasses/MissionTest.h"

namespace Marine {
class MarinePlanContext;
}

class CoverageInspectionPlanCreator;

class CoverageInspectionPlanCreatorTest : public OfflineMissionTest
{
    Q_OBJECT

protected:
    void init() final;
    void cleanup() final;

private slots:
    void _testMetadata();
    void _testCreatePlan();
    void _testCreatePlanWithTwoDimensionalCenter();
    void _testCreatePlanReplacesExistingPlan();

private:
    Marine::MarinePlanContext* _marineContext = nullptr;
    CoverageInspectionPlanCreator* _creator = nullptr;
};
