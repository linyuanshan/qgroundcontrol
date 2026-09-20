#include "NominalCoverageValidator.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>
#include <vector>

#include "CoverageProblemValidator.h"
#include "Geometry/MarineGeometry.h"
#include "Geometry/PolygonRegion.h"

namespace {

constexpr double MinimumCoverageAreaToleranceM2 = 0.01;
constexpr double RelativeCoverageAreaTolerance = 1e-6;

Marine::CoverageCompletenessResult failure(Marine::CoveragePlanningError error, std::string message)
{
    Marine::CoverageCompletenessResult result;
    result.status = Marine::CoverageProblemValidator::statusForError(error);
    result.error = error;
    result.message = std::move(message);
    return result;
}

double coverageAreaTolerance(double coverageTargetAreaM2)
{
    return std::max(MinimumCoverageAreaToleranceM2, RelativeCoverageAreaTolerance * coverageTargetAreaM2);
}

bool validRole(Marine::PathLegRole role)
{
    return (role == Marine::PathLegRole::Coverage) || (role == Marine::PathLegRole::Transit);
}

}  // namespace

namespace Marine {

CoverageCompletenessResult validateNominalCoverage(const PolygonRegionSet2D& coverageTarget,
                                                   std::span<const Point2D> path, std::span<const PathLegRole> legRoles,
                                                   double swathWidthM)
{
    if (coverageTarget.empty() || !std::ranges::all_of(coverageTarget, [](const PolygonRegion2D& region) {
            return Geometry::isValidPolygonRegion(region);
        })) {
        return failure(CoveragePlanningError::InvalidGeneratedPath, "Coverage target is invalid");
    }
    if (!std::isfinite(swathWidthM) || (swathWidthM <= 0.0)) {
        return failure(CoveragePlanningError::InvalidSwathWidth,
                       CoverageProblemValidator::messageForError(CoveragePlanningError::InvalidSwathWidth));
    }
    if ((path.size() < 2) || (legRoles.size() != path.size() - 1)) {
        return failure(CoveragePlanningError::InvalidGeneratedPath, "Coverage path and leg roles are inconsistent");
    }
    for (const Point2D& point : path) {
        if (!point.isFinite()) {
            return failure(CoveragePlanningError::InvalidGeneratedPath, "Coverage path contains a non-finite point");
        }
    }

    const Geometry::PolygonRegionAreaResult targetArea = Geometry::polygonRegionArea(coverageTarget);
    if ((targetArea.status != Geometry::PolygonRegionOperationStatus::Success) || !std::isfinite(targetArea.areaM2) ||
        (targetArea.areaM2 <= 0.0)) {
        return failure(CoveragePlanningError::GeometryFailure, "Coverage target area calculation failed");
    }
    const double toleranceM2 = coverageAreaTolerance(targetArea.areaM2);

    std::vector<Geometry::LineSegment2D> coverageSegments;
    coverageSegments.reserve(legRoles.size());
    auto pointIterator = path.begin();
    for (const PathLegRole role : legRoles) {
        if (!validRole(role)) {
            return failure(CoveragePlanningError::InvalidGeneratedPath, "Coverage path contains an invalid leg role");
        }
        if (role != PathLegRole::Coverage) {
            ++pointIterator;
            continue;
        }
        const Point2D& start = *pointIterator;
        const Point2D& end = *std::next(pointIterator);
        const double lengthM = std::hypot(end.xM - start.xM, end.yM - start.yM);
        if (!std::isfinite(lengthM) || (lengthM <= Geometry::LengthEpsilonM)) {
            return failure(CoveragePlanningError::InvalidGeneratedPath,
                           "Coverage path contains a zero-length coverage leg");
        }
        coverageSegments.push_back({.start = start, .end = end});
        ++pointIterator;
    }

    if (coverageSegments.empty()) {
        CoverageCompletenessResult result;
        result.coverageTargetAreaM2 = targetArea.areaM2;
        result.uncoveredAreaM2 = targetArea.areaM2;
        result.toleranceM2 = toleranceM2;
        result.error = CoveragePlanningError::CoverageIncomplete;
        result.message = CoverageProblemValidator::messageForError(result.error);
        return result;
    }

    const Geometry::PolygonRegionOperationResult footprint =
        Geometry::bufferLineSegments(coverageSegments, swathWidthM / 2.0);
    if (footprint.status != Geometry::PolygonRegionOperationStatus::Success) {
        return failure(CoveragePlanningError::GeometryFailure, "Coverage footprint buffering failed");
    }
    const Geometry::PolygonRegionOperationResult uncovered =
        Geometry::differencePolygonRegions(coverageTarget, footprint.regions);
    if (uncovered.status != Geometry::PolygonRegionOperationStatus::Success) {
        return failure(CoveragePlanningError::GeometryFailure, "Coverage target difference calculation failed");
    }
    const Geometry::PolygonRegionAreaResult uncoveredArea = Geometry::polygonRegionArea(uncovered.regions);
    if ((uncoveredArea.status != Geometry::PolygonRegionOperationStatus::Success) ||
        !std::isfinite(uncoveredArea.areaM2) || (uncoveredArea.areaM2 < 0.0) ||
        (uncoveredArea.areaM2 > targetArea.areaM2 + toleranceM2)) {
        return failure(CoveragePlanningError::GeometryFailure, "Uncovered area calculation failed");
    }

    CoverageCompletenessResult result;
    result.coverageTargetAreaM2 = targetArea.areaM2;
    result.coveredTargetAreaM2 = targetArea.areaM2 - uncoveredArea.areaM2;
    result.uncoveredAreaM2 = uncoveredArea.areaM2;
    result.toleranceM2 = toleranceM2;
    if (result.uncoveredAreaM2 <= result.toleranceM2) {
        result.status = PlanningStatus::Success;
        result.error = CoveragePlanningError::None;
        result.message = "Nominal coverage footprint covers the coverage target";
    } else {
        result.error = CoveragePlanningError::CoverageIncomplete;
        result.message = CoverageProblemValidator::messageForError(result.error);
    }
    return result;
}

}  // namespace Marine
