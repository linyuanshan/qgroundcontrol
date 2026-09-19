#include "GreedyCellOrdering.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "Geometry/MarineGeometry.h"
#include "Geometry/PolygonRegion.h"

namespace {

using Marine::CellCoverage;
using Marine::CellOrderingResult;
using Marine::CellTraversalOrientation;
using Marine::CellTraversalState;
using Marine::CoverageCellId;
using Marine::OrderedCellTraversal;
using Marine::StaticRoute;

struct OrientedCellStates
{
    CoverageCellId cellId = 0;
    const CellTraversalState* forward = nullptr;
    const CellTraversalState* reverse = nullptr;
    bool visited = false;
};

struct CandidateVisit
{
    std::size_t cellIndex = 0;
    const CellTraversalState* state = nullptr;
    StaticRoute transit;
};

CellOrderingResult failure(Marine::CoveragePlanningError error, std::string message)
{
    CellOrderingResult result;
    result.error = error;
    result.message = std::move(message);
    return result;
}

int orientationRank(CellTraversalOrientation orientation)
{
    return (orientation == CellTraversalOrientation::Forward) ? 0 : 1;
}

bool pointsEqual(const Marine::Point2D& first, const Marine::Point2D& second)
{
    return (first.xM == second.xM) && (first.yM == second.yM);
}

bool candidateLess(const CandidateVisit& first, const CandidateVisit& second)
{
    if (first.transit.lengthM != second.transit.lengthM) {
        return first.transit.lengthM < second.transit.lengthM;
    }
    if (first.state->cellId != second.state->cellId) {
        return first.state->cellId < second.state->cellId;
    }
    return orientationRank(first.state->orientation) < orientationRank(second.state->orientation);
}

}  // namespace

namespace Marine {

CellOrderingResult orderCellTraversals(const PolygonRegionSet2D& trackFeasibleRegion,
                                       std::span<const CellCoverage> cells,
                                       std::span<const CellTraversalState> traversalStates)
{
    if (cells.empty()) {
        return failure(CoveragePlanningError::CellCoverageFailed, "No cell coverage was supplied for ordering");
    }
    if (trackFeasibleRegion.empty() || !std::ranges::all_of(trackFeasibleRegion, [](const PolygonRegion2D& region) {
            return Geometry::isValidPolygonRegion(region);
        })) {
        return failure(CoveragePlanningError::GeometryFailure, "Track feasible region is invalid");
    }

    std::vector<CoverageCellId> cellIds;
    cellIds.reserve(cells.size());
    for (const CellCoverage& cell : cells) {
        cellIds.push_back(cell.cellId);
    }
    std::ranges::sort(cellIds);
    if (std::ranges::adjacent_find(cellIds) != cellIds.end()) {
        return failure(CoveragePlanningError::CellCoverageFailed, "Cell coverage IDs must be unique");
    }

    std::vector<const CellTraversalState*> orderedStates;
    orderedStates.reserve(traversalStates.size());
    for (const CellTraversalState& state : traversalStates) {
        if (((state.orientation != CellTraversalOrientation::Forward) &&
             (state.orientation != CellTraversalOrientation::Reverse)) ||
            !state.entry.isFinite() || !state.exit.isFinite() ||
            !Geometry::pointInsidePolygonRegion(trackFeasibleRegion, state.entry) ||
            !Geometry::pointInsidePolygonRegion(trackFeasibleRegion, state.exit)) {
            return failure(CoveragePlanningError::CellCoverageFailed, "A cell traversal state is invalid");
        }
        orderedStates.push_back(&state);
    }
    std::ranges::sort(orderedStates, [](const CellTraversalState* first, const CellTraversalState* second) {
        if (first->cellId != second->cellId) {
            return first->cellId < second->cellId;
        }
        return orientationRank(first->orientation) < orientationRank(second->orientation);
    });

    if (orderedStates.size() != (cellIds.size() * 2)) {
        return failure(CoveragePlanningError::CellCoverageFailed,
                       "Every covered cell must have one Forward and one Reverse traversal state");
    }

    std::vector<OrientedCellStates> orientedCells;
    orientedCells.reserve(cellIds.size());
    for (std::size_t index = 0; index < cellIds.size(); ++index) {
        const CellTraversalState* forward = orderedStates.at(index * 2);
        const CellTraversalState* reverse = orderedStates.at((index * 2) + 1);
        if ((forward->cellId != cellIds.at(index)) || (reverse->cellId != cellIds.at(index)) ||
            (forward->orientation != CellTraversalOrientation::Forward) ||
            (reverse->orientation != CellTraversalOrientation::Reverse) ||
            !pointsEqual(forward->entry, reverse->exit) || !pointsEqual(forward->exit, reverse->entry)) {
            return failure(CoveragePlanningError::CellCoverageFailed,
                           "Every covered cell must have matching Forward and Reverse traversal states");
        }
        orientedCells.push_back({.cellId = cellIds.at(index), .forward = forward, .reverse = reverse});
    }

    std::vector<OrderedCellTraversal> visits;
    visits.reserve(orientedCells.size());
    OrientedCellStates& firstCell = orientedCells.front();
    firstCell.visited = true;
    visits.push_back({.state = *firstCell.forward, .transitFromPrevious = std::nullopt});
    Point2D currentExit = firstCell.forward->exit;
    double totalTransitLengthM = 0.0;

    while (visits.size() < orientedCells.size()) {
        std::optional<CandidateVisit> best;
        for (std::size_t cellIndex = 0; cellIndex < orientedCells.size(); ++cellIndex) {
            const OrientedCellStates& cell = orientedCells.at(cellIndex);
            if (cell.visited) {
                continue;
            }
            for (const CellTraversalState* state : {cell.forward, cell.reverse}) {
                StaticRoute transit = routeStatic(trackFeasibleRegion, currentExit, state->entry);
                if (transit.status != PlanningStatus::Success) {
                    if (transit.error == CoveragePlanningError::SafeTransitNotFound) {
                        continue;
                    }
                    return failure(transit.error,
                                   "Static transit routing failed during cell ordering: " + transit.message);
                }
                CandidateVisit candidate{
                    .cellIndex = cellIndex,
                    .state = state,
                    .transit = std::move(transit),
                };
                if (!best.has_value() || candidateLess(candidate, *best)) {
                    best = std::move(candidate);
                }
            }
        }

        if (!best.has_value()) {
            return failure(CoveragePlanningError::SafeTransitNotFound,
                           "No safe static transit route reaches any remaining coverage cell");
        }

        totalTransitLengthM += best->transit.lengthM;
        if (!std::isfinite(totalTransitLengthM)) {
            return failure(CoveragePlanningError::InvalidGeneratedPath,
                           "Cell ordering produced an invalid total transit length");
        }
        currentExit = best->state->exit;
        orientedCells.at(best->cellIndex).visited = true;
        visits.push_back({.state = *best->state, .transitFromPrevious = std::move(best->transit)});
    }

    CellOrderingResult result;
    result.status = PlanningStatus::Success;
    result.visits = std::move(visits);
    result.totalTransitLengthM = totalTransitLengthM;
    result.error = CoveragePlanningError::None;
    result.message = "Coverage cells ordered by deterministic greedy safe-transit distance";
    return result;
}

}  // namespace Marine
