#include "PlannerRegistry.h"

#include <utility>

namespace Marine {

bool PlannerRegistry::registerPlanner(std::shared_ptr<ICoveragePlanner> planner)
{
    if (!planner) {
        return false;
    }

    const std::string plannerId = planner->id();
    if (plannerId.empty()) {
        return false;
    }

    return _planners.emplace(plannerId, std::move(planner)).second;
}

std::shared_ptr<ICoveragePlanner> PlannerRegistry::planner(const std::string& id) const
{
    const auto iterator = _planners.find(id);
    return (iterator == _planners.end()) ? nullptr : iterator->second;
}

}  // namespace Marine
