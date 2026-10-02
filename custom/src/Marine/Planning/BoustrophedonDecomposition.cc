#include "BoustrophedonDecomposition.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "Geometry/MarineGeometry.h"
#include "Geometry/PolygonRegion.h"

namespace {

using namespace Marine;

CoverageDecompositionResult failure(PlanningStatus status, CoveragePlanningError error, std::string message)
{
    return {.status = status, .error = error, .message = std::move(message)};
}

std::vector<double> eventLevels(const PolygonRegion2D& region)
{
    std::vector<double> levels;
    const auto append = [&levels](const Polygon2D& polygon) {
        for (const Point2D& vertex : polygon.vertices) {
            levels.push_back(std::round(vertex.yM * Geometry::CoordinateScalePerM) / Geometry::CoordinateScalePerM);
        }
    };
    append(region.outerBoundary);
    for (const Polygon2D& hole : region.holes) {
        append(hole);
    }
    std::sort(levels.begin(), levels.end());
    std::vector<double> merged;
    for (double level : levels) {
        // Distinct backend levels can carry a split or merge, even when only one lattice unit apart.
        constexpr double EventEpsilonM = 0.5 / Geometry::CoordinateScalePerM;
        if (merged.empty() || (level - merged.back() > EventEpsilonM)) {
            merged.push_back(level);
        }
    }
    return merged;
}

using CanonicalRing = std::vector<Point2D>;

bool pointLess(const Point2D& left, const Point2D& right)
{
    return (left.xM < right.xM) || ((left.xM == right.xM) && (left.yM < right.yM));
}

bool ringLess(const CanonicalRing& left, const CanonicalRing& right)
{
    return std::lexicographical_compare(left.begin(), left.end(), right.begin(), right.end(), pointLess);
}

CanonicalRing canonicalRing(const Polygon2D& polygon)
{
    CanonicalRing best;
    const std::size_t vertexCount = polygon.vertices.size();
    for (std::size_t start = 0; start < vertexCount; ++start) {
        CanonicalRing forward;
        CanonicalRing reverse;
        forward.reserve(vertexCount);
        reverse.reserve(vertexCount);
        for (std::size_t offset = 0; offset < vertexCount; ++offset) {
            forward.push_back(polygon.vertices[(start + offset) % vertexCount]);
            reverse.push_back(polygon.vertices[(start + vertexCount - offset) % vertexCount]);
        }
        if (best.empty() || ringLess(forward, best)) {
            best = std::move(forward);
        }
        if (ringLess(reverse, best)) {
            best = std::move(reverse);
        }
    }
    return best;
}

struct CanonicalRegionOrderingKey
{
    CanonicalRing outer;
    std::vector<CanonicalRing> holes;
};

CanonicalRegionOrderingKey canonicalOrderingKey(const PolygonRegion2D& region)
{
    CanonicalRegionOrderingKey key{.outer = canonicalRing(region.outerBoundary)};
    key.holes.reserve(region.holes.size());
    for (const Polygon2D& hole : region.holes) {
        key.holes.push_back(canonicalRing(hole));
    }
    std::sort(key.holes.begin(), key.holes.end(), ringLess);
    return key;
}

bool polygonLess(const PolygonRegion2D& left, const PolygonRegion2D& right)
{
    const CanonicalRegionOrderingKey leftKey = canonicalOrderingKey(left);
    const CanonicalRegionOrderingKey rightKey = canonicalOrderingKey(right);
    if (ringLess(leftKey.outer, rightKey.outer)) {
        return true;
    }
    if (ringLess(rightKey.outer, leftKey.outer)) {
        return false;
    }
    return std::lexicographical_compare(leftKey.holes.begin(), leftKey.holes.end(), rightKey.holes.begin(),
                                        rightKey.holes.end(), ringLess);
}

struct ActivePiece
{
    Polygon2D polygon;
    CoverageCellId owner = 0;
};

CoverageDecompositionResult decomposeSingleComponent(const PolygonRegion2D& input, double navigationAngleDeg)
{
    const double mathAngle = Geometry::navigationAngleToMathAngle(navigationAngleDeg);
    PolygonRegion2D sweepRegion{.outerBoundary = Geometry::toSweepFrame(input.outerBoundary, mathAngle)};
    for (const Polygon2D& hole : input.holes) {
        sweepRegion.holes.push_back(Geometry::toSweepFrame(hole, mathAngle));
    }
    const std::vector<double> levels = eventLevels(sweepRegion);
    if (levels.size() < 2) {
        return failure(PlanningStatus::Failed, CoveragePlanningError::DecompositionFailed,
                       "Sweep extent is below the event tolerance");
    }

    std::vector<PolygonRegionSet2D> ownedPieces;
    std::vector<ActivePiece> previous;
    CoverageDecompositionResult result;
    for (std::size_t slabIndex = 0; slabIndex + 1 < levels.size(); ++slabIndex) {
        auto slab = Geometry::clipPolygonRegionsToSlab({sweepRegion}, levels[slabIndex], levels[slabIndex + 1]);
        if ((slab.status != Geometry::PolygonRegionOperationStatus::Success) || slab.regions.empty()) {
            return failure(PlanningStatus::Failed, CoveragePlanningError::DecompositionFailed,
                           "Free-space slab clipping failed");
        }
        std::sort(slab.regions.begin(), slab.regions.end(), polygonLess);
        std::vector<std::vector<std::size_t>> predecessors(slab.regions.size());
        std::vector<std::size_t> successorCounts(previous.size(), 0);
        for (std::size_t next = 0; next < slab.regions.size(); ++next) {
            const PolygonRegion2D& piece = slab.regions[next];
            // Thin slab edges are artificial. Validate size and monotonicity after cell union,
            // otherwise close critical levels can reject a perfectly valid final cell.
            if (!piece.holes.empty()) {
                return failure(PlanningStatus::Failed, CoveragePlanningError::InvalidCoverageCell,
                               "Merged event slab contains an unresolved hole");
            }
            for (std::size_t prior = 0; prior < previous.size(); ++prior) {
                if (Geometry::shareSlabBoundary(previous[prior].polygon, piece.outerBoundary, levels[slabIndex])) {
                    predecessors[next].push_back(prior);
                    ++successorCounts[prior];
                }
            }
        }

        std::vector<ActivePiece> current;
        for (std::size_t next = 0; next < slab.regions.size(); ++next) {
            CoverageCellId owner = 0;
            const auto& prior = predecessors[next];
            if ((prior.size() == 1) && (successorCounts[prior.front()] == 1)) {
                owner = previous[prior.front()].owner;
            } else {
                if (ownedPieces.size() >= std::numeric_limits<CoverageCellId>::max()) {
                    return failure(PlanningStatus::Failed, CoveragePlanningError::DecompositionFailed,
                                   "Too many coverage cells");
                }
                owner = static_cast<CoverageCellId>(ownedPieces.size());
                ownedPieces.emplace_back();
                for (std::size_t predecessor : prior) {
                    result.adjacency.push_back({.first = previous[predecessor].owner, .second = owner});
                }
            }
            current.push_back({.polygon = slab.regions[next].outerBoundary, .owner = owner});
            ownedPieces[owner].push_back(std::move(slab.regions[next]));
        }
        previous = std::move(current);
    }

    for (std::size_t owner = 0; owner < ownedPieces.size(); ++owner) {
        auto cell = Geometry::unionPolygonRegions(ownedPieces[owner]);
        if ((cell.status != Geometry::PolygonRegionOperationStatus::Success) || (cell.regions.size() != 1) ||
            !cell.regions.front().holes.empty()) {
            return failure(PlanningStatus::Failed, CoveragePlanningError::InvalidCoverageCell,
                           "Cell " + std::to_string(owner) + " union is not connected and hole-free");
        }
        if (!Geometry::isMonotoneCellPolygon(cell.regions.front().outerBoundary, 0.0)) {
            return failure(PlanningStatus::Failed, CoveragePlanningError::InvalidCoverageCell,
                           "Cell " + std::to_string(owner) + " is not simple and sweep-monotone");
        }
        Polygon2D polygon = std::move(cell.regions.front().outerBoundary);
        for (Point2D& vertex : polygon.vertices) {
            vertex = Geometry::fromSweepFrame(vertex, mathAngle);
        }
        if (!polygon.isFinite() || !Geometry::isValidPolygonRegion({.outerBoundary = polygon}) ||
            !Geometry::isMonotoneCellPolygon(polygon, mathAngle)) {
            return failure(PlanningStatus::Failed, CoveragePlanningError::InvalidCoverageCell,
                           "Cell is invalid in the local coordinate frame");
        }
        result.cells.push_back({.id = static_cast<CoverageCellId>(owner), .polygon = std::move(polygon)});
    }
    if (result.cells.empty()) {
        return failure(PlanningStatus::Failed, CoveragePlanningError::DecompositionFailed,
                       "Slab clipping produced no coverage cells");
    }
    std::sort(result.adjacency.begin(), result.adjacency.end(), [](const CellAdjacency& a, const CellAdjacency& b) {
        return (a.first < b.first) || ((a.first == b.first) && (a.second < b.second));
    });
    result.adjacency.erase(std::unique(result.adjacency.begin(), result.adjacency.end()), result.adjacency.end());
    result.status = PlanningStatus::Success;
    result.error = CoveragePlanningError::None;
    return result;
}

}  // namespace

namespace Marine {

CoverageDecompositionResult decomposeBoustrophedon(const PolygonRegionSet2D& targetRegions, double navigationAngleDeg)
{
    if (!std::isfinite(navigationAngleDeg)) {
        return failure(PlanningStatus::InvalidInput, CoveragePlanningError::InvalidSweepAngle,
                       "Decomposition sweep angle must be finite");
    }
    if (targetRegions.empty()) {
        return failure(PlanningStatus::InvalidInput, CoveragePlanningError::EmptyCoverageTarget,
                       "No coverage target to decompose");
    }
    for (const PolygonRegion2D& targetRegion : targetRegions) {
        if (!Geometry::isValidPolygonRegion(targetRegion)) {
            return failure(PlanningStatus::InvalidInput, CoveragePlanningError::InvalidCoverageTarget,
                           "Decomposition requires valid target polygon regions");
        }
    }

    PolygonRegionSet2D orderedTargetRegions = targetRegions;
    std::sort(orderedTargetRegions.begin(), orderedTargetRegions.end(), polygonLess);

    CoverageDecompositionResult result;
    for (const PolygonRegion2D& targetRegion : orderedTargetRegions) {
        const CoverageDecompositionResult component = decomposeSingleComponent(targetRegion, navigationAngleDeg);
        if (component.status != PlanningStatus::Success) {
            return failure(component.status, component.error, component.message);
        }

        const std::uint64_t cellOffset = static_cast<std::uint64_t>(result.cells.size());
        constexpr std::uint64_t MaximumCellId = std::numeric_limits<CoverageCellId>::max();
        for (const CoverageCell& cell : component.cells) {
            if ((cellOffset > MaximumCellId) || (static_cast<std::uint64_t>(cell.id) > MaximumCellId - cellOffset)) {
                return failure(PlanningStatus::Failed, CoveragePlanningError::DecompositionFailed,
                               "Too many coverage cells across target components");
            }
            result.cells.push_back({.id = static_cast<CoverageCellId>(cellOffset + cell.id), .polygon = cell.polygon});
        }
        for (const CellAdjacency& adjacency : component.adjacency) {
            if ((cellOffset > MaximumCellId) ||
                (static_cast<std::uint64_t>(adjacency.first) > MaximumCellId - cellOffset) ||
                (static_cast<std::uint64_t>(adjacency.second) > MaximumCellId - cellOffset)) {
                return failure(PlanningStatus::Failed, CoveragePlanningError::DecompositionFailed,
                               "Too many coverage cells across target components");
            }
            result.adjacency.push_back({.first = static_cast<CoverageCellId>(cellOffset + adjacency.first),
                                        .second = static_cast<CoverageCellId>(cellOffset + adjacency.second)});
        }
    }

    std::sort(result.adjacency.begin(), result.adjacency.end(), [](const CellAdjacency& a, const CellAdjacency& b) {
        return (a.first < b.first) || ((a.first == b.first) && (a.second < b.second));
    });
    result.adjacency.erase(std::unique(result.adjacency.begin(), result.adjacency.end()), result.adjacency.end());
    result.status = PlanningStatus::Success;
    result.error = CoveragePlanningError::None;
    return result;
}

}  // namespace Marine
