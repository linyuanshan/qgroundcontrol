#include "ComplexCoverageAssembly.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <optional>
#include <utility>

#include "Geometry/MarineGeometry.h"
#include "Geometry/PolygonRegion.h"

namespace {

using Marine::BoundaryCoverageComponent;
using Marine::CellCoverage;
using Marine::ComplexCoverageAssemblyResult;
using Marine::OrderedCellTraversal;
using Marine::PathLegRole;
using Marine::Point2D;
using Marine::StaticRoute;

struct PathMetrics
{
    double coverageLengthM = 0.0;
    double transitLengthM = 0.0;
    double pathLengthM = 0.0;
};

ComplexCoverageAssemblyResult failure(
    std::string message, Marine::CoveragePlanningError error = Marine::CoveragePlanningError::InvalidGeneratedPath)
{
    ComplexCoverageAssemblyResult result;
    result.error = error;
    result.message = std::move(message);
    return result;
}

bool pointsEqual(const Point2D& first, const Point2D& second)
{
    return (first.xM == second.xM) && (first.yM == second.yM);
}

double distance(const Point2D& first, const Point2D& second)
{
    return std::hypot(second.xM - first.xM, second.yM - first.yM);
}

bool validRole(PathLegRole role)
{
    return (role == PathLegRole::Coverage) || (role == PathLegRole::Transit);
}

bool lengthsEqual(double first, double second)
{
    return std::isfinite(first) && std::isfinite(second) &&
           (std::abs(first - second) <= Marine::Geometry::LengthEpsilonM);
}

std::optional<PathMetrics> calculateMetrics(std::span<const Point2D> path, std::span<const PathLegRole> legRoles)
{
    if ((path.size() < 2) || (legRoles.size() != path.size() - 1)) {
        return std::nullopt;
    }

    PathMetrics metrics;
    Point2D previousPoint = path.front();
    auto roleIterator = legRoles.begin();
    for (auto pointIterator = std::next(path.begin()); pointIterator != path.end(); ++pointIterator, ++roleIterator) {
        const Point2D& point = *pointIterator;
        const PathLegRole role = *roleIterator;
        if (!previousPoint.isFinite() || !point.isFinite() || !validRole(role)) {
            return std::nullopt;
        }
        const double legLengthM = distance(previousPoint, point);
        if (!std::isfinite(legLengthM) || (legLengthM <= Marine::Geometry::LengthEpsilonM)) {
            return std::nullopt;
        }
        metrics.pathLengthM += legLengthM;
        if (role == PathLegRole::Coverage) {
            metrics.coverageLengthM += legLengthM;
        } else {
            metrics.transitLengthM += legLengthM;
        }
        previousPoint = point;
    }
    if (!std::isfinite(metrics.coverageLengthM) || !std::isfinite(metrics.transitLengthM) ||
        !std::isfinite(metrics.pathLengthM)) {
        return std::nullopt;
    }
    return metrics;
}

std::optional<double> routeLength(const StaticRoute& route)
{
    if ((route.status != Marine::PlanningStatus::Success) || (route.error != Marine::CoveragePlanningError::None) ||
        (route.path.size() < 2) || !std::isfinite(route.lengthM) || (route.lengthM < 0.0)) {
        return std::nullopt;
    }

    double lengthM = 0.0;
    for (std::size_t index = 1; index < route.path.size(); ++index) {
        const Point2D& previousPoint = route.path.at(index - 1);
        const Point2D& point = route.path.at(index);
        if (!previousPoint.isFinite() || !point.isFinite()) {
            return std::nullopt;
        }
        const double legLengthM = distance(previousPoint, point);
        if (!std::isfinite(legLengthM)) {
            return std::nullopt;
        }
        if (legLengthM <= Marine::Geometry::LengthEpsilonM) {
            const bool zeroMovementRoute = (route.path.size() == 2) &&
                                           pointsEqual(route.path.front(), route.path.back()) &&
                                           (route.lengthM <= Marine::Geometry::LengthEpsilonM);
            if (!zeroMovementRoute) {
                return std::nullopt;
            }
            continue;
        }
        lengthM += legLengthM;
    }
    if (!lengthsEqual(lengthM, route.lengthM)) {
        return std::nullopt;
    }
    return lengthM;
}

bool appendPolyline(std::vector<Point2D>& assembledPath, std::vector<PathLegRole>& assembledRoles,
                    std::span<const Point2D> path, std::span<const PathLegRole> roles,
                    std::optional<PathLegRole> overrideRole = std::nullopt)
{
    if ((path.size() < 2) || (!overrideRole.has_value() && (roles.size() != path.size() - 1))) {
        return false;
    }
    if (assembledPath.empty()) {
        assembledPath.push_back(path.front());
    } else if (!pointsEqual(assembledPath.back(), path.front())) {
        return false;
    }

    Point2D previousPoint = path.front();
    auto roleIterator = roles.begin();
    for (auto pointIterator = std::next(path.begin()); pointIterator != path.end(); ++pointIterator) {
        const Point2D& point = *pointIterator;
        const double legLengthM = distance(previousPoint, point);
        if (legLengthM <= Marine::Geometry::LengthEpsilonM) {
            const bool zeroMovementRoute =
                overrideRole.has_value() && (path.size() == 2) && pointsEqual(path.front(), path.back());
            if (!zeroMovementRoute) {
                return false;
            }
            continue;
        }
        assembledPath.push_back(point);
        if (overrideRole.has_value()) {
            assembledRoles.push_back(*overrideRole);
        } else {
            assembledRoles.push_back(*roleIterator);
            ++roleIterator;
        }
        previousPoint = point;
    }
    return true;
}

std::optional<int> countTurns(std::span<const Point2D> path)
{
    double previousDirectionX = 0.0;
    double previousDirectionY = 0.0;
    bool hasPreviousDirection = false;
    int turnCount = 0;
    Point2D previousPoint = path.front();
    for (auto pointIterator = std::next(path.begin()); pointIterator != path.end(); ++pointIterator) {
        const Point2D& point = *pointIterator;
        const double deltaX = point.xM - previousPoint.xM;
        const double deltaY = point.yM - previousPoint.yM;
        const double legLengthM = std::hypot(deltaX, deltaY);
        if (legLengthM <= Marine::Geometry::LengthEpsilonM) {
            previousPoint = point;
            continue;
        }
        const double directionX = deltaX / legLengthM;
        const double directionY = deltaY / legLengthM;
        if (hasPreviousDirection) {
            const double cross = std::abs((previousDirectionX * directionY) - (previousDirectionY * directionX));
            const double dot = (previousDirectionX * directionX) + (previousDirectionY * directionY);
            if ((cross > Marine::Geometry::LengthEpsilonM) || (dot < 0.0)) {
                if (turnCount == std::numeric_limits<int>::max()) {
                    return std::nullopt;
                }
                ++turnCount;
            }
        }
        previousDirectionX = directionX;
        previousDirectionY = directionY;
        hasPreviousDirection = true;
        previousPoint = point;
    }
    return turnCount;
}

}  // namespace

namespace Marine {

ComplexCoverageAssemblyResult assembleComplexCoverage(const PolygonRegionSet2D& trackFeasibleRegion,
                                                      std::span<const CellCoverage> cells,
                                                      std::span<const OrderedCellTraversal> visits)
{
    return assembleComplexCoverage(trackFeasibleRegion, {}, cells, visits);
}

ComplexCoverageAssemblyResult assembleComplexCoverage(const PolygonRegionSet2D& trackFeasibleRegion,
                                                      std::span<const BoundaryCoverageComponent> boundaryComponents,
                                                      std::span<const CellCoverage> cells,
                                                      std::span<const OrderedCellTraversal> visits)
{
    if (cells.empty() || (cells.size() != visits.size()) ||
        (visits.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) || trackFeasibleRegion.empty() ||
        !std::ranges::all_of(trackFeasibleRegion,
                             [](const PolygonRegion2D& region) { return Geometry::isValidPolygonRegion(region); })) {
        return failure("Complex coverage assembly inputs are invalid");
    }

    std::vector<const BoundaryCoverageComponent*> orderedBoundaries;
    orderedBoundaries.reserve(boundaryComponents.size());
    for (const BoundaryCoverageComponent& component : boundaryComponents) {
        const std::optional<PathMetrics> metrics = calculateMetrics(component.path, component.legRoles);
        if (!metrics.has_value() || !pointsEqual(component.path.front(), component.path.back()) ||
            !std::ranges::all_of(component.legRoles, [](PathLegRole role) { return role == PathLegRole::Coverage; }) ||
            !lengthsEqual(metrics->coverageLengthM, component.pathLengthM) ||
            !lengthsEqual(metrics->transitLengthM, 0.0)) {
            return failure("A boundary coverage component is invalid");
        }
        orderedBoundaries.push_back(&component);
    }
    std::ranges::sort(orderedBoundaries,
                      [](const BoundaryCoverageComponent* first, const BoundaryCoverageComponent* second) {
                          return first->id < second->id;
                      });
    if (std::ranges::adjacent_find(orderedBoundaries,
                                   [](const BoundaryCoverageComponent* first, const BoundaryCoverageComponent* second) {
                                       return first->id == second->id;
                                   }) != orderedBoundaries.end()) {
        return failure("Boundary coverage component IDs must be unique");
    }

    std::vector<const CellCoverage*> orderedCoverages;
    orderedCoverages.reserve(cells.size());
    for (const CellCoverage& coverage : cells) {
        const std::optional<PathMetrics> metrics = calculateMetrics(coverage.path, coverage.legRoles);
        if (!metrics.has_value() || !lengthsEqual(metrics->coverageLengthM, coverage.coverageLengthM) ||
            !lengthsEqual(metrics->transitLengthM, coverage.transitLengthM) ||
            !lengthsEqual(metrics->pathLengthM, coverage.pathLengthM)) {
            return failure("A cell coverage path or metric is invalid");
        }
        orderedCoverages.push_back(&coverage);
    }
    std::ranges::sort(orderedCoverages, [](const CellCoverage* first, const CellCoverage* second) {
        return first->cellId < second->cellId;
    });
    if (std::ranges::adjacent_find(orderedCoverages, [](const CellCoverage* first, const CellCoverage* second) {
            return first->cellId == second->cellId;
        }) != orderedCoverages.end()) {
        return failure("Cell coverage IDs must be unique");
    }

    std::vector<CoverageCellId> visitIds;
    visitIds.reserve(visits.size());
    for (const OrderedCellTraversal& visit : visits) {
        visitIds.push_back(visit.state.cellId);
    }
    std::ranges::sort(visitIds);
    for (std::size_t index = 0; index < visitIds.size(); ++index) {
        if ((visitIds.at(index) != orderedCoverages.at(index)->cellId) ||
            ((index > 0) && (visitIds.at(index) == visitIds.at(index - 1)))) {
            return failure("Ordered visits must contain every covered cell exactly once");
        }
    }

    ComplexCoverageAssemblyResult assembled;
    for (const BoundaryCoverageComponent* component : orderedBoundaries) {
        if (!assembled.path.empty()) {
            const StaticRoute transit =
                routeStatic(trackFeasibleRegion, assembled.path.back(), component->path.front());
            if (!routeLength(transit).has_value() || !pointsEqual(transit.path.front(), assembled.path.back()) ||
                !pointsEqual(transit.path.back(), component->path.front()) ||
                !appendPolyline(assembled.path, assembled.legRoles, transit.path, {}, PathLegRole::Transit)) {
                const CoveragePlanningError error = transit.error == CoveragePlanningError::None
                                                        ? CoveragePlanningError::InvalidGeneratedPath
                                                        : transit.error;
                return failure("A boundary-component transit route is invalid", error);
            }
        }
        if (!appendPolyline(assembled.path, assembled.legRoles, component->path, component->legRoles)) {
            return failure("A boundary coverage component cannot be joined to the canonical path");
        }
    }

    const Point2D* previousExit = nullptr;
    std::size_t visitIndex = 0;
    for (const OrderedCellTraversal& visit : visits) {
        const auto coverageIterator = std::ranges::lower_bound(
            orderedCoverages, visit.state.cellId, {}, [](const CellCoverage* coverage) { return coverage->cellId; });
        if ((coverageIterator == orderedCoverages.end()) || ((*coverageIterator)->cellId != visit.state.cellId)) {
            return failure("Ordered visit references missing cell coverage");
        }
        const CellCoverage& coverage = **coverageIterator;

        const bool forward = visit.state.orientation == CellTraversalOrientation::Forward;
        const bool reverse = visit.state.orientation == CellTraversalOrientation::Reverse;
        const Point2D& expectedEntry = forward ? coverage.path.front() : coverage.path.back();
        const Point2D& expectedExit = forward ? coverage.path.back() : coverage.path.front();
        if ((!forward && !reverse) || !pointsEqual(visit.state.entry, expectedEntry) ||
            !pointsEqual(visit.state.exit, expectedExit)) {
            return failure("Traversal state endpoints do not match the canonical cell coverage path");
        }

        if (visitIndex == 0) {
            if (visit.transitFromPrevious.has_value()) {
                return failure("The first ordered cell must not have a preceding transit route");
            }
            if (!assembled.path.empty()) {
                const StaticRoute transit = routeStatic(trackFeasibleRegion, assembled.path.back(), visit.state.entry);
                if (!routeLength(transit).has_value() || !pointsEqual(transit.path.front(), assembled.path.back()) ||
                    !pointsEqual(transit.path.back(), visit.state.entry) ||
                    !appendPolyline(assembled.path, assembled.legRoles, transit.path, {}, PathLegRole::Transit)) {
                    const CoveragePlanningError error = transit.error == CoveragePlanningError::None
                                                            ? CoveragePlanningError::InvalidGeneratedPath
                                                            : transit.error;
                    return failure("The boundary-to-cell transit route is invalid", error);
                }
            }
        } else {
            if (!visit.transitFromPrevious.has_value() || (previousExit == nullptr)) {
                return failure("A non-first ordered cell is missing its preceding transit route");
            }
            const StaticRoute& transit = *visit.transitFromPrevious;
            if (!routeLength(transit).has_value() || !pointsEqual(transit.path.front(), *previousExit) ||
                !pointsEqual(transit.path.back(), visit.state.entry) ||
                !appendPolyline(assembled.path, assembled.legRoles, transit.path, {}, PathLegRole::Transit)) {
                return failure("An inter-cell transit route is invalid or has inconsistent endpoints");
            }
        }

        std::vector<Point2D> orientedPath = coverage.path;
        std::vector<PathLegRole> orientedRoles = coverage.legRoles;
        if (reverse) {
            std::ranges::reverse(orientedPath);
            std::ranges::reverse(orientedRoles);
        }
        if (!appendPolyline(assembled.path, assembled.legRoles, orientedPath, orientedRoles)) {
            return failure("A cell coverage path cannot be joined to the canonical path");
        }
        previousExit = &visit.state.exit;
        ++visitIndex;
    }

    const std::optional<PathMetrics> roleMetrics = calculateMetrics(assembled.path, assembled.legRoles);
    if (!roleMetrics.has_value()) {
        return failure("The assembled canonical path or leg roles are invalid");
    }
    for (std::size_t index = 1; index < assembled.path.size(); ++index) {
        if (!Geometry::segmentInsidePolygonRegionForValidatedGeometry(trackFeasibleRegion, assembled.path.at(index - 1),
                                                                      assembled.path.at(index))) {
            return failure("The assembled canonical path contains an unsafe centerline leg");
        }
    }

    double geometricLengthM = 0.0;
    for (std::size_t index = 1; index < assembled.path.size(); ++index) {
        geometricLengthM += distance(assembled.path.at(index - 1), assembled.path.at(index));
    }
    const std::optional<int> turnCount = countTurns(assembled.path);
    if (!std::isfinite(geometricLengthM) ||
        !lengthsEqual(geometricLengthM, roleMetrics->coverageLengthM + roleMetrics->transitLengthM) ||
        !turnCount.has_value()) {
        return failure("The assembled canonical path metrics are inconsistent");
    }

    assembled.status = PlanningStatus::Success;
    assembled.coverageLengthM = roleMetrics->coverageLengthM;
    assembled.transitLengthM = roleMetrics->transitLengthM;
    assembled.pathLengthM = geometricLengthM;
    assembled.cellCount = static_cast<int>(visits.size());
    assembled.turnCount = *turnCount;
    assembled.error = CoveragePlanningError::None;
    assembled.message = "Complex coverage canonical path assembled";
    return assembled;
}

}  // namespace Marine
