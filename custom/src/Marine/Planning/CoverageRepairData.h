#pragma once

#include <cstdint>
#include <vector>

#include "CoverageQualityEvaluator.h"
#include "PlanningPathMetrics.h"

namespace Marine {

/// Pure planning runtime facts only; neither executable diagnostics nor an artifact persistence schema.
struct CoverageRepairCandidate
{
    std::vector<Point2D> path;
    std::vector<PathLegRole> legRoles;
    PlanningPathMetrics metrics;
    CoverageQualityEvaluation quality;
    bool preferredSafe = false;
};

struct CoverageRepairStep
{
    std::uint32_t componentId = 0;
    std::vector<Point2D> componentPath;
    std::size_t entryIndex = 0;
    bool reverse = false;
    double transitionCostM = 0.0;
    CoverageQualityEvaluation before;
    CoverageRepairCandidate after;
};

struct CoverageRepairResult
{
    bool attempted = false;
    std::size_t availableComponentCount = 0;
    CoverageRepairCandidate candidate;
    std::vector<CoverageRepairStep> steps;
};

}  // namespace Marine
