#pragma once

#include <string>

#include "MarineTypes.h"

namespace Marine {

enum class PlannerResolutionStatus
{
    Resolved,
    ResolvedStrategyUnavailable,
};

enum class PlannerResolutionReason
{
    None,
    TargetHasMultipleComponents,
    TargetHasHoles,
    TargetNonMonotoneForSelectedSweep,
};

struct PlannerStrategyIdentity
{
    std::string strategyId;
    std::string semanticVersion;

    bool operator==(const PlannerStrategyIdentity&) const = default;
};

struct PlannerSourceInfo
{
    std::string requestedPlannerId;
    PlannerStrategyIdentity resolvedStrategy;
    PlannerResolutionStatus resolutionStatus = PlannerResolutionStatus::Resolved;
    PlannerResolutionReason resolutionReason = PlannerResolutionReason::None;
    bool escalated = false;
    SweepAngleMode requestedSweepMode = SweepAngleMode::Auto;
    double selectedSweepAngleDeg = 0.0;
    std::string sweepSemanticVersion;

    bool operator==(const PlannerSourceInfo&) const = default;
};

}  // namespace Marine
