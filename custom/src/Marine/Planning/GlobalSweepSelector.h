#pragma once

#include <string>

#include "CoveragePlanningProblem.h"

namespace Marine {

struct GlobalSweepSelectionResult
{
    PlanningStatus status = PlanningStatus::Failed;
    double selectedSweepAngleDeg = 0.0;
    double crossTrackSpanM = 0.0;
    int estimatedLaneCount = 0;
    int estimatedTurnCount = 0;
    CoveragePlanningError error = CoveragePlanningError::InvalidSweepAngle;
    std::string message;
};

/// Selects one global sweep from original outer-edge bearings using only span and lane-count estimates.
[[nodiscard]] GlobalSweepSelectionResult selectGlobalSweepAngle(const Polygon2D& originalOuterBoundary,
                                                                const PolygonRegionSet2D& trackFeasibleRegion,
                                                                double swathWidthM);

}  // namespace Marine
