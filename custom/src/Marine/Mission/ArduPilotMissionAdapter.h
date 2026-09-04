#pragma once

#include <QtCore/QList>
#include <QtCore/QString>

#include "PlanningResult.h"

class MissionItem;
class QObject;

namespace Marine {

class ArduPilotMissionAdapter final
{
public:
    [[nodiscard]] static bool appendWaypoints(const PlanningResult& result, QList<MissionItem*>& items, QObject* parent,
                                              int& sequenceNumber, QString& errorString);
};

}  // namespace Marine
