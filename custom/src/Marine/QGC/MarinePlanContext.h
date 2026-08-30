#pragma once

#include <QtCore/QObject>

#include <string>
#include <unordered_map>

#include "MarineTask.h"
#include "PlannerRegistry.h"

class PlanMasterController;

namespace Marine {

class MarinePlanContext final : public QObject
{
    Q_OBJECT

public:
    explicit MarinePlanContext(PlanMasterController* controller);

    [[nodiscard]] MarineTask* task(const std::string& taskId);
    [[nodiscard]] const MarineTask* task(const std::string& taskId) const;
    void addTask(const MarineTask& task);
    void removeTask(const std::string& taskId);
    void clearTasks();
    PlannerRegistry& plannerRegistry();
    const PlannerRegistry& plannerRegistry() const;

private:
    std::unordered_map<std::string, MarineTask> _tasks;
    PlannerRegistry _plannerRegistry;
};

}  // namespace Marine
