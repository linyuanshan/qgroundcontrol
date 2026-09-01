#pragma once

#include "ICoveragePlanner.h"

namespace Marine {

class MockCoveragePlanner final : public ICoveragePlanner
{
public:
    std::string id() const final;
    std::string displayName() const final;
    PlanningResult plan(const MarineTask& task) const final;
};

}  // namespace Marine
