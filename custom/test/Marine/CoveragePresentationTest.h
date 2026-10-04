#pragma once

#include "BaseClasses/MissionTest.h"

class CoveragePresentationTest : public OfflineMissionTest
{
    Q_OBJECT
private slots:
    void _testResultMatrix_data();
    void _testResultMatrix();
    void _testRepairPresentation_data();
    void _testRepairPresentation();
    void _testIndependentEditingAndStale();
    void _testActualEditorControls();
    void _testCodesReferencesAndSafetyRuns();
    void _testProductMapHolesAndPalettes();
    void _testToolbarBindingAndOverride();
};
