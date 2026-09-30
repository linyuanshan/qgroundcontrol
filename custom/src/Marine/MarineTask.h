#pragma once

#include <limits>
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
    // H/P are explicit Task v3 inputs; an intentional zero must still be assigned by the caller or codec.
    double hardSafetyMarginM = std::numeric_limits<double>::quiet_NaN();
    double preferredSafetyMarginM = std::numeric_limits<double>::quiet_NaN();
};

struct ExecutionSafetyProfile
{
    // A caller or the Task v3 codec must explicitly configure E, including an intentional zero.
    double executionMarginM = std::numeric_limits<double>::quiet_NaN();
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
