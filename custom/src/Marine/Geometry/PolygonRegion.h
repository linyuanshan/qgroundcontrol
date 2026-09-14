#pragma once

#include "GeometryTypes.h"

namespace Marine::Geometry {

enum class NoGoValidationStatus
{
    Success,
    InvalidOuterBoundary,
    InvalidNoGoRegion,
    OutsideOuterBoundary,
    BoundaryConflict,
    OverlapOrTouch,
};

enum class PolygonRegionOperationStatus
{
    Success,
    InvalidInput,
    GeometryFailure,
};

struct PolygonRegionOperationResult
{
    PolygonRegionOperationStatus status = PolygonRegionOperationStatus::GeometryFailure;
    PolygonRegionSet2D regions;
};

struct PolygonRegionContainmentResult
{
    PolygonRegionOperationStatus status = PolygonRegionOperationStatus::GeometryFailure;
    bool contained = false;
};

[[nodiscard]] NoGoValidationStatus validateNoGoRegions(const Polygon2D& outerBoundary,
                                                       const std::vector<Polygon2D>& noGoRegions);
[[nodiscard]] PolygonRegionOperationResult buildCoverageTarget(const Polygon2D& outerBoundary,
                                                               const std::vector<Polygon2D>& noGoRegions);
[[nodiscard]] PolygonRegionOperationResult buildTrackFeasibleRegion(const Polygon2D& outerBoundary,
                                                                    const std::vector<Polygon2D>& noGoRegions,
                                                                    double safetyMarginM);
[[nodiscard]] PolygonRegionOperationResult bufferPolygonRegions(const PolygonRegionSet2D& regions, double distanceM);
[[nodiscard]] PolygonRegionContainmentResult isRegionSetContained(const PolygonRegionSet2D& target,
                                                                  const PolygonRegionSet2D& container);

/// Clip each open-slab component separately, returning its closure. No vertex Y may lie inside the slab.
[[nodiscard]] PolygonRegionOperationResult clipPolygonRegionsToSlab(const PolygonRegionSet2D& regions, double minimumYM,
                                                                    double maximumYM);
[[nodiscard]] PolygonRegionOperationResult unionPolygonRegions(const PolygonRegionSet2D& regions);
/// True only for a shared boundary interval of positive, resolved length, never a point contact.
[[nodiscard]] bool shareSlabBoundary(const Polygon2D& below, const Polygon2D& above, double eventYM);
/// Predicates for boolean/offset output on the backend lattice, without the Task input proximity gate.
[[nodiscard]] bool isValidPolygonRegion(const PolygonRegion2D& region);
[[nodiscard]] bool isMonotoneCellPolygon(const Polygon2D& polygon, double mathAngleDeg);

}  // namespace Marine::Geometry
