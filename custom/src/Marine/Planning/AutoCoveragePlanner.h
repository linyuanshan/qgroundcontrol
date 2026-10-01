#pragma once

#include "ICoveragePlanner.h"

namespace Marine {

class AutoCoveragePlanner final : public ICoveragePlanner
{
public:
    std::string id() const final;
    std::string semanticVersion() const final;
    std::string displayName() const final;
    CoveragePlanningSolution plan(const CoveragePlanningProblem& problem) const final;
};

}  // namespace Marine
