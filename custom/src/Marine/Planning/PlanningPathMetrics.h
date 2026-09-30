#pragma once

#include <optional>
#include <span>

#include "Geometry/GeometryTypes.h"
#include "PathLegRole.h"

namespace Marine {

/// Absolute tolerance for persisted path/metric consistency, not safety clearance or product accuracy.
inline constexpr double PathMetricsConsistencyToleranceM = 0.01;

struct PlanningPathMetrics
{
    double coverageLengthM = 0.0;
    double transitLengthM = 0.0;
    double pathLengthM = 0.0;
};

/// Validates the complete canonical path and derives lengths from its actual legs.
[[nodiscard]] std::optional<PlanningPathMetrics> calculatePlanningPathMetrics(std::span<const Point2D> path,
                                                                              std::span<const PathLegRole> roles);
[[nodiscard]] bool planningPathMetricsMatch(const PlanningPathMetrics& actual, double coverageLengthM,
                                            double transitLengthM, double pathLengthM);

}  // namespace Marine
