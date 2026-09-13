#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "Geometry/GeometryTypes.h"
#include "MarineTypes.h"
#include "PathLegRole.h"

namespace Marine {

enum class MonotoneCoverageError
{
    None,
    InvalidTargetPolygon,
    InvalidNavigablePolygon,
    InvalidSwathWidth,
    InvalidSweepAngle,
    InvalidLaneSchedule,
    NonMonotoneSweep,
    NoIntersection,
    MultipleIntervals,
    UnsafeConnector,
    InvalidGeneratedPath,
    GeometryFailure,
};

struct MonotoneCoverageLane
{
    std::size_t startIndex = 0;
    std::size_t endIndex = 0;
    double positionYM = 0.0;
};

struct MonotoneCoverageResult
{
    PlanningStatus status = PlanningStatus::Failed;
    std::vector<Point2D> path;
    std::vector<PathLegRole> legRoles;
    std::vector<MonotoneCoverageLane> lanes;
    double laneSpacingM = 0.0;
    double coverageLengthM = 0.0;
    double transitLengthM = 0.0;
    double pathLengthM = 0.0;
    int laneCount = 0;
    int turnCount = 0;
    MonotoneCoverageError error = MonotoneCoverageError::None;
    std::string message;
};

/// Generates fixed-angle coverage for a supplied nominal target and navigable centerline polygon.
/// The lane schedule is expressed in the sweep frame and is never inset, reordered, or auto-selected here.
[[nodiscard]] MonotoneCoverageResult generateMonotoneCoverage(const Polygon2D& targetPolygon,
                                                              const Polygon2D& navigablePolygon, double swathWidthM,
                                                              double navigationAngleDeg,
                                                              std::span<const double> lanePositionsYM);

/// Generates a schedule from the nominal polygon and delegates to the same fixed-angle core.
[[nodiscard]] MonotoneCoverageResult generateMonotoneCoverage(const Polygon2D& targetPolygon,
                                                              const Polygon2D& navigablePolygon, double swathWidthM,
                                                              double navigationAngleDeg);

}  // namespace Marine
