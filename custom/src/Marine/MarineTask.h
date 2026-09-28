#pragma once

#include <string>

#include "MarineTypes.h"

namespace Marine {

enum class MarineTaskType
{
    CoverageInspection,
};

enum class CoverageRequirement
{
    Standard,
    Strict,
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
    CoverageRequirement coverageRequirement = CoverageRequirement::Standard;
    SweepAngleMode sweepAngleMode = SweepAngleMode::Auto;
    // Navigation bearing: 0 degrees North, 90 degrees East, clockwise positive.
    double sweepAngleDeg = 0.0;
};

struct SafetyConfig
{
    double hardSafetyMarginM = 0.0;
    double preferredSafetyMarginM = 0.0;
};

struct ExecutionSafetyProfile
{
    double executionMarginM = 0.0;
};

struct PlannerConfig
{
    std::string plannerId = "marine.coverage.auto";
    ExecutionSafetyProfile executionSafety;
};

struct MarineTask
{
    MarineTask();

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] bool schemaValid() const;

    std::string id;
    std::string name;
    MarineTaskType type = MarineTaskType::CoverageInspection;
    std::string vehicleId;
    WorkRegion region;
    CoverageConfig coverage;
    SafetyConfig safety;
    SensorConfig sensors;
    PlannerConfig planner;
};

}  // namespace Marine
