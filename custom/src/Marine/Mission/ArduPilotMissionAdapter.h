#pragma once

#include <QtCore/QList>
#include <QtCore/QString>

#include "PlanningArtifact.h"

class MissionItem;
class QObject;

namespace Marine {

class ArduPilotMissionAdapter final
{
public:
    [[nodiscard]] static bool appendWaypoints(const PlanningArtifact& artifact, const MarineTask& currentTask,
                                              QList<MissionItem*>& items, QObject* parent, int& sequenceNumber,
                                              QString& errorString);
    /// Uncertified legacy entry point is fail-closed; callers must supply artifact and current task.
    [[nodiscard]] static bool appendWaypoints(const PlanningResult& result, QList<MissionItem*>& items, QObject* parent,
                                              int& sequenceNumber, QString& errorString);
};

}  // namespace Marine
