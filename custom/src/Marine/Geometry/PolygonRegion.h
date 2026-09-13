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

}  // namespace Marine::Geometry
