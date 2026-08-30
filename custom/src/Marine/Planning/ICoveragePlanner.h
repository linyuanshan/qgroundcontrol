#pragma once

#include <string>
#include <vector>

#include "MarineTask.h"

namespace Marine {

enum class PlanningStatus
{
    Success,
    InvalidInput,
    Failed,
};

struct PlanningResult
{
    PlanningStatus status = PlanningStatus::Failed;
    std::vector<GeoPoint> path;
    double pathLengthM = 0.0;
    std::string message;
};

class ICoveragePlanner
{
public:
    virtual ~ICoveragePlanner() = default;

    virtual std::string id() const = 0;
    virtual std::string displayName() const = 0;
    virtual PlanningResult plan(const MarineTask& task) const = 0;
};

}  // namespace Marine
