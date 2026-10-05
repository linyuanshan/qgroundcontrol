#pragma once

#include "Planning/CoveragePlanningProblem.h"

namespace V05ExecutionFixtures {

// 按 V05_10_R1_EXECUTION_CALIBRATION.md 的预先声明规则一次固定。
inline constexpr double ExecutionMarginM = 4.5;
inline constexpr double SideM = 90.0;
inline constexpr double SwathM = 67.5;
inline constexpr auto M00Canonical =
    "M00|C=rect(0,0,90,90)|N=C|O=[]|swath=67.5|H=0|P=0|E=4.5|req=Standard|sweep=Manual90|planner=Auto";
inline constexpr auto M00Sha256 = "b2c40fb313785b91467859e1342b409e4f94679e7dcd47f78451ba06e3701526";
inline constexpr auto M04Canonical =
    "M04|C=rect(0,0,90,90)|N=rect(-5.5,-5.5,95.5,95.5)|O=[]|swath=67.5|H=0|P=8|E=4.5|req=Strict|sweep=Manual90|planner="
    "Auto";
inline constexpr auto M04Sha256 = "e3acd31f0445498dfb722f1ed84c630d8b09c18c1e4f5b6776c213b42dd4d141";

inline Marine::CoveragePlanningProblem problem(bool preferredWarning)
{
    Marine::CoveragePlanningProblem result;
    result.region.coverageBoundary = {.vertices = {{0, 0}, {SideM, 0}, {SideM, SideM}, {0, SideM}}};
    result.region.navigationBoundary = result.region.coverageBoundary;
    if (preferredWarning) {
        constexpr double room = ExecutionMarginM + 1.0;
        result.region.navigationBoundary = {
            .vertices = {{-room, -room}, {SideM + room, -room}, {SideM + room, SideM + room}, {-room, SideM + room}}};
    }
    result.swathWidthM = SwathM;
    result.safety.hardSafetyMarginM = 0.0;
    result.safety.preferredSafetyMarginM = preferredWarning ? 8.0 : 0.0;
    result.executionSafety.executionMarginM = ExecutionMarginM;
    result.coverageRequirement =
        preferredWarning ? Marine::CoverageRequirement::Strict : Marine::CoverageRequirement::Standard;
    result.sweepAngleMode = Marine::SweepAngleMode::Manual;
    result.requestedSweepAngleDeg = 90.0;
    return result;
}

}  // namespace V05ExecutionFixtures
