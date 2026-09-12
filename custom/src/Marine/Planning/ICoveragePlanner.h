#pragma once

#include <string>

#include "CoveragePlanningProblem.h"

namespace Marine {

class ICoveragePlanner
{
public:
    virtual ~ICoveragePlanner() = default;

    virtual std::string id() const = 0;
    virtual std::string displayName() const = 0;
    virtual CoveragePlanningSolution plan(const CoveragePlanningProblem& problem) const = 0;
};

}  // namespace Marine
