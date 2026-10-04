#pragma once

#include "BaseClasses/VehicleTest.h"

class MarineUploadGateTest : public VehicleTestAPM
{
    Q_OBJECT

private slots:
    void _blockedResults_data();
    void _blockedResults();
    void _failedFileLoad_data();
    void _failedFileLoad();
    void _acceptedFileUpload_data();
    void _acceptedFileUpload();
    void _conversionRefusalIsAtomic();
    void _admissionNotifications();
};
