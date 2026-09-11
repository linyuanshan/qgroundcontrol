#pragma once

#include <string>

#include "MarineTypes.h"

namespace Marine {

enum class MarineTaskType
{
    CoverageInspection,
};

struct SensorConfig
{
    bool cameraEnabled = true;
    bool cameraRecord = true;
    bool sonarEnabled = true;
    bool sonarRecord = true;
};

struct CoverageConfig
{
    double swathWidthM = 0.0;
    double safetyMarginM = 0.0;
    SweepAngleMode sweepAngleMode = SweepAngleMode::Auto;
    // Navigation bearing: 0 degrees North, 90 degrees East, clockwise positive.
    double sweepAngleDeg = 0.0;
};

struct PlannerConfig
{
    std::string plannerId;
};

struct MarineTask
{
    MarineTask();

    [[nodiscard]] bool isValid() const;

    std::string id;
    std::string name;
    MarineTaskType type = MarineTaskType::CoverageInspection;
    std::string vehicleId;
    WorkRegion region;
    CoverageConfig coverage;
    SensorConfig sensors;
    PlannerConfig planner;
};

}  // namespace Marine
