#include "MarinePlanContext.h"

#include "PlanMasterController.h"

namespace Marine {

MarinePlanContext::MarinePlanContext(PlanMasterController* controller) : QObject(controller) {}

MarineTask* MarinePlanContext::task(const std::string& taskId)
{
    const auto iterator = _tasks.find(taskId);
    return (iterator == _tasks.end()) ? nullptr : &iterator->second;
}

const MarineTask* MarinePlanContext::task(const std::string& taskId) const
{
    const auto iterator = _tasks.find(taskId);
    return (iterator == _tasks.end()) ? nullptr : &iterator->second;
}

void MarinePlanContext::addTask(const MarineTask& task)
{
    if (task.id.empty()) {
        return;
    }

    _tasks.insert_or_assign(task.id, task);
    emit taskChanged(QString::fromStdString(task.id));
}

void MarinePlanContext::removeTask(const std::string& taskId)
{
    if (_tasks.erase(taskId) != 0) {
        emit taskChanged(QString::fromStdString(taskId));
    }
}

void MarinePlanContext::clearTasks()
{
    if (_tasks.empty()) {
        return;
    }

    _tasks.clear();
    emit tasksCleared();
}

PlannerRegistry& MarinePlanContext::plannerRegistry()
{
    return _plannerRegistry;
}

const PlannerRegistry& MarinePlanContext::plannerRegistry() const
{
    return _plannerRegistry;
}

}  // namespace Marine
