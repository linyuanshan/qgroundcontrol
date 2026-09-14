#pragma once

#include <cstdint>

#include "CoveragePlanningProblem.h"

namespace Marine {

using CoverageCellId = std::uint32_t;

struct CoverageCell
{
    CoverageCellId id = 0;
    Polygon2D polygon;
};

struct CellAdjacency
{
    CoverageCellId first = 0;
    CoverageCellId second = 0;

    bool operator==(const CellAdjacency&) const = default;
};

struct CoverageDecompositionResult
{
    std::vector<CoverageCell> cells;
    std::vector<CellAdjacency> adjacency;
    PlanningStatus status = PlanningStatus::Failed;
    CoveragePlanningError error = CoveragePlanningError::DecompositionFailed;
    std::string message;
};

}  // namespace Marine
