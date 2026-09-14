#pragma once

#include "CoverageDecomposition.h"

namespace Marine {

/// Decompose one connected, already safety-inset region. Navigation bearing uses the P1 convention.
/// IDs start at zero in sweep progression / left-to-right creation order. Polygons are returned in ENU.
/// Artificial cell boundaries impose no additional safety clearance.
[[nodiscard]] CoverageDecompositionResult decomposeBoustrophedon(const PolygonRegionSet2D& trackFeasibleRegion,
                                                                 double navigationAngleDeg);

}  // namespace Marine
