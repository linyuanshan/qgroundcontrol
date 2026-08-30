#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "ICoveragePlanner.h"

namespace Marine {

class PlannerRegistry
{
public:
    bool registerPlanner(std::shared_ptr<ICoveragePlanner> planner);
    std::shared_ptr<ICoveragePlanner> planner(const std::string& id) const;

private:
    std::unordered_map<std::string, std::shared_ptr<ICoveragePlanner>> _planners;
};

}  // namespace Marine
