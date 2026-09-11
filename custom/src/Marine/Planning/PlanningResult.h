#pragma once

#include <string>
#include <vector>

#include "MarineTypes.h"

namespace Marine {

struct PlanningResult
{
    PlanningStatus status = PlanningStatus::Failed;
    std::vector<GeoPoint> path;
    double pathLengthM = 0.0;
    // Navigation bearing: 0 degrees North, 90 degrees East, clockwise positive.
    double selectedSweepAngleDeg = 0.0;
    int turnCount = 0;
    std::string message;
};

}  // namespace Marine
