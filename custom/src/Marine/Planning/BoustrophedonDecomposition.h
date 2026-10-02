#pragma once

#include "CoverageDecomposition.h"

namespace Marine {

/// Decompose a CoverageTarget region set into sweep-monotone cells. Navigation bearing uses the P1 convention.
/// IDs start at zero in deterministic component order and sweep progression. Polygons are returned in ENU.
/// Artificial cell boundaries impose no additional safety clearance.
[[nodiscard]] CoverageDecompositionResult decomposeBoustrophedon(const PolygonRegionSet2D& targetRegions,
                                                                 double navigationAngleDeg);

}  // namespace Marine
