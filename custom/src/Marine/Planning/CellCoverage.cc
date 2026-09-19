#include "CellCoverage.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "Geometry/MarineGeometry.h"
#include "MonotoneCoverage.h"

namespace {

using Marine::CellCoverageGenerationResult;
using Marine::CoverageCell;
using Marine::Point2D;
using Marine::Polygon2D;

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
        if (!Geometry::isSimpleNonDegeneratePolygon(cell.polygon)) {
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

        MonotoneCoverageResult primitive =
            generateMonotoneCoverage(cell->polygon, cell->polygon, swathWidthM, navigationAngleDeg, lanes);
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

}  // namespace Marine
