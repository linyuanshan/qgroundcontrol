#pragma once

#include "BoundaryCoverageSupport.h"

namespace Marine {

/// Complete canonical rings of T intersect active execution space, never the navigation perimeter.
/// Residual usefulness is decided by the unified evaluator on a fully assembled trial.
[[nodiscard]] BoundaryCoverageSupportResult generateCoverageRepairSupport(
    const PolygonRegionSet2D& coverageTarget, const PolygonRegionSet2D& activeExecutionRegion);

}  // namespace Marine
