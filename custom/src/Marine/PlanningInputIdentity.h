#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QString>

#include <optional>

#include "MarineTask.h"

namespace Marine {

struct PlanningSemantics
{
    QString planningVersion = QStringLiteral("p2.v0.5.infrastructure.1");
    QString policyVersion = QStringLiteral("unresolved");
    QString resolvedStrategy = QStringLiteral("unresolved");
    QString strategyVersion = QStringLiteral("unresolved");

    bool operator==(const PlanningSemantics&) const = default;
};

struct PlanningInputIdentity
{
    int encodingVersion = 1;
    PlanningSemantics semantics;
    QString fingerprint;

    bool operator==(const PlanningInputIdentity&) const = default;

    [[nodiscard]] static std::optional<PlanningInputIdentity> fromTask(const MarineTask& task,
                                                                       const PlanningSemantics& semantics = {});
    [[nodiscard]] bool matches(const MarineTask& task, const PlanningSemantics& supported = {}) const;
    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static bool fromJson(const QJsonObject& json, PlanningInputIdentity& identity, QString& error);
};

}  // namespace Marine
