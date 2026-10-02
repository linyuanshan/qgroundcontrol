#pragma once

#include "PlanningArtifact.h"

namespace Marine {

/// Integrated Artifact v3 structural truth. Loading never solves, reevaluates, routes or repairs.
class PlanningArtifactCodec final
{
public:
    static bool save(const PlanningArtifact& artifact, const MarineTask& task, QJsonObject& json, QString& error);
    static bool load(const QJsonObject& json, const MarineTask& task, PlanningArtifact& artifact, QString& error);
    static bool validateResult(const PlanningResult& result, const MarineTask* matchingTask, QString& error);
    static bool matchesCurrentInput(const PlanningArtifact& artifact, const MarineTask& task);
    static bool uploadAllowed(const PlanningArtifact& artifact, const MarineTask& currentTask, QString& error);
};

}  // namespace Marine
