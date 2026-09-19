#pragma once

#include <span>
#include <string>
#include <vector>

#include "CoverageDecomposition.h"
#include "PathLegRole.h"

namespace Marine {

enum class CellTraversalOrientation
{
    Forward,
    Reverse,
};

struct CellCoverage
{
    CoverageCellId cellId = 0;
    std::vector<Point2D> path;
    std::vector<PathLegRole> legRoles;
    double coverageLengthM = 0.0;
    double transitLengthM = 0.0;
    double pathLengthM = 0.0;
    int laneCount = 0;
    int turnCount = 0;
};

struct CellTraversalState
{
    CoverageCellId cellId = 0;
    CellTraversalOrientation orientation = CellTraversalOrientation::Forward;
    Point2D entry;
    Point2D exit;
};

struct CellCoverageGenerationResult
{
    std::vector<CellCoverage> cells;
    std::vector<CellTraversalState> traversalStates;
    PlanningStatus status = PlanningStatus::Failed;
    CoveragePlanningError error = CoveragePlanningError::CellCoverageFailed;
    std::string message;
};

/// Covers every supplied BCD cell with one shared cross-track lane lattice.
/// Cell polygons are already centerline-feasible and are not inset again.
[[nodiscard]] CellCoverageGenerationResult generateCellCoverage(std::span<const CoverageCell> cells, double swathWidthM,
                                                                double navigationAngleDeg);

}  // namespace Marine
