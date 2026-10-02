#include "CellCoverage.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

#include "Geometry/MarineGeometry.h"
#include "Geometry/PolygonRegion.h"
#include "MonotoneCoverage.h"
#include "PlanningPathMetrics.h"
#include "StaticSafeRouter.h"

namespace {

using Marine::CellCoverageGenerationResult;
using Marine::CellCoverage;
using Marine::CellTraversalOrientation;
using Marine::CoverageCell;
using Marine::CoveragePlanningError;
using Marine::PathLegRole;
using Marine::Point2D;
using Marine::Polygon2D;
using Marine::PolygonRegion2D;
using Marine::PolygonRegionSet2D;

CellCoverageGenerationResult failure(Marine::PlanningStatus status, Marine::CoveragePlanningError error,
                                     std::string message)
{
    CellCoverageGenerationResult result;
    result.status = status;
    result.error = error;
    result.message = std::move(message);
    return result;
}

std::pair<double, double> crossTrackExtents(const Polygon2D& polygon)
{
    std::pair<double, double> extents{std::numeric_limits<double>::max(), std::numeric_limits<double>::lowest()};
    for (const Point2D& vertex : polygon.vertices) {
        extents.first = std::min(extents.first, vertex.yM);
        extents.second = std::max(extents.second, vertex.yM);
    }
    return extents;
}

bool appendGlobalLatticeLanes(std::vector<double>& lanes, double minimumY, double maximumY, double latticeOriginY,
                              double swathWidthM)
{
    const double firstIndexValue =
        std::ceil((minimumY - latticeOriginY + Marine::Geometry::LengthEpsilonM) / swathWidthM);
    const double lastIndexValue =
        std::floor((maximumY - latticeOriginY - Marine::Geometry::LengthEpsilonM) / swathWidthM);
    constexpr auto IndexLimit = static_cast<double>(std::numeric_limits<int>::max());
    if (!std::isfinite(firstIndexValue) || !std::isfinite(lastIndexValue) || (firstIndexValue < -IndexLimit) ||
        (lastIndexValue > IndexLimit)) {
        return false;
    }
    if (firstIndexValue > lastIndexValue) {
        return true;
    }

    const auto firstIndex = static_cast<long long>(firstIndexValue);
    const auto lastIndex = static_cast<long long>(lastIndexValue);
    lanes.reserve(static_cast<std::size_t>(lastIndex - firstIndex) + 1);
    for (long long index = firstIndex; index <= lastIndex; ++index) {
        lanes.push_back(latticeOriginY + (static_cast<double>(index) * swathWidthM));
    }
    return true;
}

std::vector<double> laneSchedule(double minimumY, double maximumY, double latticeOriginY, double swathWidthM,
                                 bool& representable)
{
    std::vector<double> lanes;
    representable = appendGlobalLatticeLanes(lanes, minimumY, maximumY, latticeOriginY, swathWidthM);
    if (!representable) {
        return {};
    }

    const double halfSwathM = swathWidthM / 2.0;
    if (lanes.empty()) {
        lanes.push_back((minimumY + maximumY) / 2.0);
    } else {
        if ((lanes.front() - minimumY) > halfSwathM + Marine::Geometry::LengthEpsilonM) {
            lanes.push_back(minimumY + halfSwathM);
        }
        if ((maximumY - lanes.back()) > halfSwathM + Marine::Geometry::LengthEpsilonM) {
            lanes.push_back(maximumY - halfSwathM);
        }
    }

    std::ranges::sort(lanes);
    const auto duplicates = std::ranges::unique(lanes, [](double first, double second) {
        return std::abs(first - second) <= Marine::Geometry::LengthEpsilonM;
    });
    lanes.erase(duplicates.begin(), duplicates.end());
    return lanes;
}

bool samePoint(const Point2D& first, const Point2D& second)
{
    return first.xM == second.xM && first.yM == second.yM;
}

PolygonRegionSet2D toSweepFrame(const PolygonRegionSet2D& regions, double mathAngleDeg)
{
    PolygonRegionSet2D transformedRegions;
    transformedRegions.reserve(regions.size());
    for (const PolygonRegion2D& region : regions) {
        PolygonRegion2D transformed{
            .outerBoundary = Marine::Geometry::toSweepFrame(region.outerBoundary, mathAngleDeg)};
        transformed.holes.reserve(region.holes.size());
        for (const Polygon2D& hole : region.holes) {
            transformed.holes.push_back(Marine::Geometry::toSweepFrame(hole, mathAngleDeg));
        }
        transformedRegions.push_back(std::move(transformed));
    }
    return transformedRegions;
}

bool appendPathPoint(std::vector<Point2D>& path, std::vector<PathLegRole>& legRoles, const Point2D& point,
                     PathLegRole role)
{
    if (!point.isFinite()) {
        return false;
    }
    if (path.empty()) {
        path.push_back(point);
        return true;
    }
    if (samePoint(path.back(), point)) {
        return true;
    }
    path.push_back(point);
    legRoles.push_back(role);
    return true;
}

struct TrackLaneSegment
{
    double minimumXM = 0.0;
    double maximumXM = 0.0;
    double laneYM = 0.0;
};

std::optional<std::vector<TrackLaneSegment>> buildTrackLaneSegments(const Polygon2D& targetSweepPolygon,
                                                                    const PolygonRegionSet2D& activeTrackSweep,
                                                                    const std::vector<double>& lanes,
                                                                    CoveragePlanningError& error)
{
    std::vector<TrackLaneSegment> segments;
    segments.reserve(lanes.size());
    for (const double laneY : lanes) {
        const Marine::Geometry::ScanlineResult targetScanline =
            Marine::Geometry::intersectScanlineForValidatedGeometry(targetSweepPolygon, laneY);
        if (targetScanline.status == Marine::Geometry::ScanlineStatus::InvalidInput ||
            targetScanline.status == Marine::Geometry::ScanlineStatus::GeometryFailure) {
            error = CoveragePlanningError::GeometryFailure;
            return std::nullopt;
        }
        if (targetScanline.status != Marine::Geometry::ScanlineStatus::Success ||
            targetScanline.intervals.size() != 1) {
            error = CoveragePlanningError::CellCoverageFailed;
            return std::nullopt;
        }

        const Marine::Geometry::ScanlineResult activeScanline =
            Marine::Geometry::intersectScanlineForValidatedGeometry(activeTrackSweep, laneY);
        if (activeScanline.status == Marine::Geometry::ScanlineStatus::InvalidInput ||
            activeScanline.status == Marine::Geometry::ScanlineStatus::GeometryFailure) {
            error = CoveragePlanningError::GeometryFailure;
            return std::nullopt;
        }
        if (activeScanline.status == Marine::Geometry::ScanlineStatus::NoIntersection) {
            error = CoveragePlanningError::CoverageImpossibleWithExecutionMargin;
            return std::nullopt;
        }
        if (activeScanline.status != Marine::Geometry::ScanlineStatus::Success || activeScanline.intervals.empty()) {
            error = CoveragePlanningError::GeometryFailure;
            return std::nullopt;
        }

        const Marine::Geometry::ScanlineInterval& target = targetScanline.intervals.front();
        if (!std::isfinite(target.minimumXM) || !std::isfinite(target.maximumXM) ||
            target.maximumXM < target.minimumXM) {
            error = CoveragePlanningError::GeometryFailure;
            return std::nullopt;
        }
        bool foundOverlap = false;
        TrackLaneSegment best;
        double bestOverlapM = 0.0;
        for (const Marine::Geometry::ScanlineInterval& active : activeScanline.intervals) {
            if (!std::isfinite(active.minimumXM) || !std::isfinite(active.maximumXM) ||
                active.maximumXM < active.minimumXM) {
                error = CoveragePlanningError::GeometryFailure;
                return std::nullopt;
            }
            const double minimumXM = std::max(target.minimumXM, active.minimumXM);
            const double maximumXM = std::min(target.maximumXM, active.maximumXM);
            const double overlapM = maximumXM - minimumXM;
            if (!std::isfinite(overlapM)) {
                error = CoveragePlanningError::GeometryFailure;
                return std::nullopt;
            }
            if (overlapM <= 0.0) {
                continue;
            }
            const bool longer = overlapM > bestOverlapM;
            const bool stableTie = overlapM == bestOverlapM &&
                                   (minimumXM < best.minimumXM ||
                                    (minimumXM == best.minimumXM && maximumXM < best.maximumXM));
            if (!foundOverlap || longer || stableTie) {
                best = {.minimumXM = minimumXM, .maximumXM = maximumXM, .laneYM = laneY};
                bestOverlapM = overlapM;
                foundOverlap = true;
            }
        }
        if (!foundOverlap) {
            error = CoveragePlanningError::CoverageImpossibleWithExecutionMargin;
            return std::nullopt;
        }
        segments.push_back(best);
    }
    error = CoveragePlanningError::None;
    return segments;
}

std::optional<CellCoverage> buildTrackConstrainedCellCoverage(const CoverageCell& cell,
                                                             const Polygon2D& targetSweepPolygon,
                                                             const std::vector<double>& lanes,
                                                             const PolygonRegionSet2D& activeTrackRegion,
                                                             const PolygonRegionSet2D& activeTrackSweep,
                                                             double mathAngleDeg,
                                                             CoveragePlanningError& error)
{
    const auto laneSegments = buildTrackLaneSegments(targetSweepPolygon, activeTrackSweep, lanes, error);
    if (!laneSegments) {
        return std::nullopt;
    }
    if (laneSegments->size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        error = CoveragePlanningError::CellCoverageFailed;
        return std::nullopt;
    }

    CellCoverage coverage;
    coverage.cellId = cell.id;
    coverage.laneCount = static_cast<int>(laneSegments->size());
    for (std::size_t laneIndex = 0; laneIndex < laneSegments->size(); ++laneIndex) {
        const TrackLaneSegment& lane = laneSegments->at(laneIndex);
        const bool minimumToMaximum = (laneIndex % 2) == 0;
        const Point2D entry = Marine::Geometry::fromSweepFrame(
            {.xM = minimumToMaximum ? lane.minimumXM : lane.maximumXM, .yM = lane.laneYM}, mathAngleDeg);
        const Point2D exit = Marine::Geometry::fromSweepFrame(
            {.xM = minimumToMaximum ? lane.maximumXM : lane.minimumXM, .yM = lane.laneYM}, mathAngleDeg);

        if (laneIndex > 0 && !samePoint(coverage.path.back(), entry)) {
            const Marine::StaticRoute route = routeStatic(activeTrackRegion, coverage.path.back(), entry);
            if (route.error != CoveragePlanningError::None) {
                error = route.error;
                return std::nullopt;
            }
            if (route.status != Marine::PlanningStatus::Success || route.path.size() < 2 ||
                !std::ranges::all_of(route.path, [](const Point2D& point) { return point.isFinite(); }) ||
                !samePoint(route.path.front(), coverage.path.back()) || !samePoint(route.path.back(), entry)) {
                error = CoveragePlanningError::InvalidGeneratedPath;
                return std::nullopt;
            }
            for (std::size_t routeIndex = 1; routeIndex < route.path.size(); ++routeIndex) {
                if (!appendPathPoint(coverage.path, coverage.legRoles, route.path[routeIndex], PathLegRole::Transit)) {
                    error = CoveragePlanningError::InvalidGeneratedPath;
                    return std::nullopt;
                }
            }
            if (!samePoint(coverage.path.back(), entry)) {
                error = CoveragePlanningError::InvalidGeneratedPath;
                return std::nullopt;
            }
        }
        if (!appendPathPoint(coverage.path, coverage.legRoles, entry, PathLegRole::Coverage) ||
            !appendPathPoint(coverage.path, coverage.legRoles, exit, PathLegRole::Coverage)) {
            error = CoveragePlanningError::InvalidGeneratedPath;
            return std::nullopt;
        }
    }

    const std::optional<Marine::PlanningPathMetrics> metrics =
        Marine::calculatePlanningPathMetrics(coverage.path, coverage.legRoles);
    if (!metrics) {
        error = CoveragePlanningError::InvalidGeneratedPath;
        return std::nullopt;
    }
    coverage.coverageLengthM = metrics->coverageLengthM;
    coverage.transitLengthM = metrics->transitLengthM;
    coverage.pathLengthM = metrics->pathLengthM;
    coverage.turnCount = metrics->turnCount;
    error = CoveragePlanningError::None;
    return coverage;
}

struct OrderedTargetCell
{
    const CoverageCell* cell = nullptr;
    Polygon2D sweepPolygon;
    std::pair<double, double> crossTrackExtents;
};

}  // namespace

namespace Marine {

CellCoverageGenerationResult generateCellCoverage(std::span<const CoverageCell> cells, double swathWidthM,
                                                  double navigationAngleDeg)
{
    if (!std::isfinite(swathWidthM) || (swathWidthM <= 0.0)) {
        return failure(PlanningStatus::InvalidInput, CoveragePlanningError::InvalidSwathWidth,
                       "Coverage swath width must be finite and greater than zero");
    }
    if (!std::isfinite(navigationAngleDeg)) {
        return failure(PlanningStatus::InvalidInput, CoveragePlanningError::InvalidSweepAngle,
                       "Sweep angle must be finite");
    }
    if (cells.empty()) {
        return failure(PlanningStatus::Failed, CoveragePlanningError::CellCoverageFailed,
                       "No decomposition cells were supplied");
    }

    const double mathAngleDeg = Geometry::navigationAngleToMathAngle(navigationAngleDeg);
    std::vector<std::pair<const CoverageCell*, std::pair<double, double>>> orderedCells;
    orderedCells.reserve(cells.size());
    double globalMinimumY = std::numeric_limits<double>::max();
    for (const CoverageCell& cell : cells) {
        const PolygonRegion2D cellRegion{.outerBoundary = cell.polygon};
        if (!Geometry::isValidPolygonRegion(cellRegion) ||
            !Geometry::isMonotoneCellPolygon(cell.polygon, mathAngleDeg)) {
            return failure(PlanningStatus::Failed, CoveragePlanningError::CellCoverageFailed,
                           "A decomposition cell is invalid");
        }
        const Polygon2D sweepPolygon = Geometry::toSweepFrame(cell.polygon, mathAngleDeg);
        const auto extents = crossTrackExtents(sweepPolygon);
        globalMinimumY = std::min(globalMinimumY, extents.first);
        orderedCells.emplace_back(&cell, extents);
    }
    std::ranges::sort(orderedCells,
                      [](const auto& first, const auto& second) { return first.first->id < second.first->id; });
    if (std::ranges::adjacent_find(orderedCells, [](const auto& first, const auto& second) {
            return first.first->id == second.first->id;
        }) != orderedCells.end()) {
        return failure(PlanningStatus::Failed, CoveragePlanningError::CellCoverageFailed,
                       "Coverage cell IDs must be unique");
    }

    CellCoverageGenerationResult result;
    result.cells.reserve(orderedCells.size());
    result.traversalStates.reserve(orderedCells.size() * 2);
    const double latticeOriginY = globalMinimumY + (swathWidthM / 2.0);
    for (const auto& [cell, extents] : orderedCells) {
        bool representable = false;
        const std::vector<double> lanes =
            laneSchedule(extents.first, extents.second, latticeOriginY, swathWidthM, representable);
        if (!representable || lanes.empty()) {
            return failure(PlanningStatus::Failed, CoveragePlanningError::CellCoverageFailed,
                           "Coverage lane schedule cannot be represented for cell " + std::to_string(cell->id));
        }

        MonotoneCoverageResult primitive = generateMonotoneCoverageForValidatedGeometry(
            cell->polygon, cell->polygon, swathWidthM, navigationAngleDeg, lanes);
        if (primitive.error == MonotoneCoverageError::UnsafeConnector) {
            primitive = generateMonotoneCoverageForValidatedGeometry(cell->polygon, cell->polygon, swathWidthM,
                                                                     navigationAngleDeg, lanes, true);
        }
        if (primitive.status != PlanningStatus::Success) {
            return failure(
                PlanningStatus::Failed, CoveragePlanningError::CellCoverageFailed,
                "Coverage generation failed for cell " + std::to_string(cell->id) + ": " + primitive.message);
        }

        CellCoverage coverage;
        coverage.cellId = cell->id;
        coverage.path = std::move(primitive.path);
        coverage.legRoles = std::move(primitive.legRoles);
        coverage.coverageLengthM = primitive.coverageLengthM;
        coverage.transitLengthM = primitive.transitLengthM;
        coverage.pathLengthM = primitive.pathLengthM;
        coverage.laneCount = primitive.laneCount;
        coverage.turnCount = primitive.turnCount;
        const Point2D entry = coverage.path.front();
        const Point2D exit = coverage.path.back();
        result.cells.push_back(std::move(coverage));
        result.traversalStates.push_back(
            {.cellId = cell->id, .orientation = CellTraversalOrientation::Forward, .entry = entry, .exit = exit});
        result.traversalStates.push_back(
            {.cellId = cell->id, .orientation = CellTraversalOrientation::Reverse, .entry = exit, .exit = entry});
    }

    result.status = PlanningStatus::Success;
    result.error = CoveragePlanningError::None;
    result.message = "Coverage generated for every decomposition cell";
    return result;
}

CellCoverageGenerationResult generateCellCoverage(std::span<const CoverageCell> cells,
                                                  const PolygonRegionSet2D& activeTrackRegion, double swathWidthM,
                                                  double navigationAngleDeg)
{
    if (!std::isfinite(swathWidthM) || (swathWidthM <= 0.0)) {
        return failure(PlanningStatus::InvalidInput, CoveragePlanningError::InvalidSwathWidth,
                       "Coverage swath width must be finite and greater than zero");
    }
    if (!std::isfinite(navigationAngleDeg)) {
        return failure(PlanningStatus::InvalidInput, CoveragePlanningError::InvalidSweepAngle,
                       "Sweep angle must be finite");
    }
    if (cells.empty()) {
        return failure(PlanningStatus::Failed, CoveragePlanningError::CellCoverageFailed,
                       "No decomposition cells were supplied");
    }
    if (activeTrackRegion.empty()) {
        return failure(PlanningStatus::Failed, CoveragePlanningError::NoNavigableArea,
                       "No active execution track region was supplied");
    }
    if (!std::ranges::all_of(activeTrackRegion, [](const PolygonRegion2D& region) {
            return Marine::Geometry::isValidPolygonRegion(region);
        })) {
        return failure(PlanningStatus::Failed, CoveragePlanningError::GeometryFailure,
                       "Active execution track region is invalid");
    }

    const double mathAngleDeg = Geometry::navigationAngleToMathAngle(navigationAngleDeg);
    std::vector<OrderedTargetCell> orderedCells;
    orderedCells.reserve(cells.size());
    double globalMinimumY = std::numeric_limits<double>::max();
    for (const CoverageCell& cell : cells) {
        const PolygonRegion2D cellRegion{.outerBoundary = cell.polygon};
        if (!Geometry::isValidPolygonRegion(cellRegion) ||
            !Geometry::isMonotoneCellPolygon(cell.polygon, mathAngleDeg)) {
            return failure(PlanningStatus::Failed, CoveragePlanningError::CellCoverageFailed,
                           "A coverage target cell is invalid or non-monotone");
        }
        Polygon2D sweepPolygon = Geometry::toSweepFrame(cell.polygon, mathAngleDeg);
        const auto extents = crossTrackExtents(sweepPolygon);
        globalMinimumY = std::min(globalMinimumY, extents.first);
        orderedCells.push_back({.cell = &cell, .sweepPolygon = std::move(sweepPolygon), .crossTrackExtents = extents});
    }
    std::ranges::sort(orderedCells, [](const OrderedTargetCell& first, const OrderedTargetCell& second) {
        return first.cell->id < second.cell->id;
    });
    if (std::ranges::adjacent_find(orderedCells, [](const OrderedTargetCell& first, const OrderedTargetCell& second) {
            return first.cell->id == second.cell->id;
        }) != orderedCells.end()) {
        return failure(PlanningStatus::Failed, CoveragePlanningError::CellCoverageFailed,
                       "Coverage cell IDs must be unique");
    }

    const PolygonRegionSet2D activeTrackSweep = toSweepFrame(activeTrackRegion, mathAngleDeg);
    CellCoverageGenerationResult result;
    result.cells.reserve(orderedCells.size());
    const double latticeOriginY = globalMinimumY + (swathWidthM / 2.0);
    for (const OrderedTargetCell& targetCell : orderedCells) {
        bool representable = false;
        const std::vector<double> lanes = laneSchedule(targetCell.crossTrackExtents.first,
                                                      targetCell.crossTrackExtents.second, latticeOriginY, swathWidthM,
                                                      representable);
        if (!representable || lanes.empty()) {
            return failure(PlanningStatus::Failed, CoveragePlanningError::CellCoverageFailed,
                           "Coverage lane schedule cannot be represented for cell " +
                               std::to_string(targetCell.cell->id));
        }

        CoveragePlanningError error = CoveragePlanningError::None;
        std::optional<CellCoverage> coverage = buildTrackConstrainedCellCoverage(
            *targetCell.cell, targetCell.sweepPolygon, lanes, activeTrackRegion, activeTrackSweep, mathAngleDeg, error);
        if (!coverage) {
            return failure(PlanningStatus::Failed, error, "Coverage generation failed for cell " +
                                                               std::to_string(targetCell.cell->id));
        }
        const Point2D entry = coverage->path.front();
        const Point2D exit = coverage->path.back();
        result.cells.push_back(std::move(*coverage));
        result.traversalStates.push_back({.cellId = targetCell.cell->id,
                                          .orientation = CellTraversalOrientation::Forward,
                                          .entry = entry,
                                          .exit = exit});
        result.traversalStates.push_back({.cellId = targetCell.cell->id,
                                          .orientation = CellTraversalOrientation::Reverse,
                                          .entry = exit,
                                          .exit = entry});
    }

    result.status = PlanningStatus::Success;
    result.error = CoveragePlanningError::None;
    result.message = "Target coverage lanes generated within the active execution track region";
    return result;
}

}  // namespace Marine
