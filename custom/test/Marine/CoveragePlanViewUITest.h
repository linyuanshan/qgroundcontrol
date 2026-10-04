#pragma once

#include "QmlUITestBase.h"

class CoveragePlanViewUITest : public QmlUITestBase
{
    Q_OBJECT
private slots:
    void _testOrdinaryDialogLifecycle();
    void _testActualUploadAndConfirmation_data();
    void _testActualUploadAndConfirmation();
};
