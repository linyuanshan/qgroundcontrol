#include "StaticSafeRouter.h"

#include <queue>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <utility>

#include "Geometry/MarineGeometry.h"
#include "Geometry/PolygonRegion.h"

namespace {

using Marine::Point2D;
using Marine::StaticRoute;

struct VisibilityEdge
{
    std::size_t node = 0;
    double lengthM = 0.0;
};

using VisibilityGraph = std::vector<std::vector<VisibilityEdge>>;

StaticRoute failure(Marine::CoveragePlanningError error, std::string message)
{
    StaticRoute route;
    route.error = error;
    route.message = std::move(message);
    return route;
}

double distance(const Point2D& first, const Point2D& second)
{
    return std::hypot(second.xM - first.xM, second.yM - first.yM);
}

bool pointLess(const Point2D& first, const Point2D& second)
{
    return (first.xM < second.xM) || ((first.xM == second.xM) && (first.yM < second.yM));
}

bool pointsEqual(const Point2D& first, const Point2D& second)
{
    return (first.xM == second.xM) && (first.yM == second.yM);
}

std::vector<Point2D> visibilityNodes(const Marine::PolygonRegionSet2D& regions, const Point2D& start,
                                     const Point2D& goal)
{
    std::vector<Point2D> nodes;
    for (const Marine::PolygonRegion2D& region : regions) {
        nodes.insert(nodes.end(), region.outerBoundary.vertices.begin(), region.outerBoundary.vertices.end());
        for (const Marine::Polygon2D& hole : region.holes) {
            nodes.insert(nodes.end(), hole.vertices.begin(), hole.vertices.end());
        }
    }
    std::ranges::sort(nodes, pointLess);
    const auto duplicates = std::ranges::unique(nodes, pointsEqual);
    nodes.erase(duplicates.begin(), duplicates.end());
    std::erase_if(
        nodes, [&start, &goal](const Point2D& point) { return pointsEqual(point, start) || pointsEqual(point, goal); });
    nodes.push_back(start);
    nodes.push_back(goal);
    return nodes;
}

VisibilityGraph buildVisibilityGraph(const Marine::PolygonRegionSet2D& regions, const std::vector<Point2D>& nodes)
{
    VisibilityGraph graph(nodes.size());
    for (std::size_t first = 0; first < nodes.size(); ++first) {
        for (std::size_t second = first + 1; second < nodes.size(); ++second) {
            if (!Marine::Geometry::segmentInsidePolygonRegionForValidatedGeometry(regions, nodes.at(first),
                                                                                  nodes.at(second))) {
                continue;
            }
            const double lengthM = distance(nodes.at(first), nodes.at(second));
            graph.at(first).push_back({.node = second, .lengthM = lengthM});
            graph.at(second).push_back({.node = first, .lengthM = lengthM});
        }
    }
    return graph;
}

}  // namespace

namespace Marine {

StaticRoute routeStatic(const PolygonRegionSet2D& trackFeasibleRegion, const Point2D& start, const Point2D& goal)
{
    if (trackFeasibleRegion.empty() || !std::ranges::all_of(trackFeasibleRegion, [](const PolygonRegion2D& region) {
            return Geometry::isValidPolygonRegion(region);
        })) {
        return failure(CoveragePlanningError::GeometryFailure, "Track feasible region is invalid");
    }
    if (!Geometry::pointInsidePolygonRegionForValidatedGeometry(trackFeasibleRegion, start) ||
        !Geometry::pointInsidePolygonRegionForValidatedGeometry(trackFeasibleRegion, goal)) {
        return failure(CoveragePlanningError::SafeTransitNotFound,
                       "Static route start and goal must lie in the track feasible region");
    }

    if (Geometry::segmentInsidePolygonRegionForValidatedGeometry(trackFeasibleRegion, start, goal)) {
        StaticRoute route;
        route.status = PlanningStatus::Success;
        route.path = {start, goal};
        route.lengthM = distance(start, goal);
        route.error = CoveragePlanningError::None;
        route.message = "Direct safe static route generated";
        return route;
    }

    const std::vector<Point2D> nodes = visibilityNodes(trackFeasibleRegion, start, goal);
    const std::size_t startNode = nodes.size() - 2;
    const std::size_t goalNode = nodes.size() - 1;
    const VisibilityGraph graph = buildVisibilityGraph(trackFeasibleRegion, nodes);
    const double infinity = std::numeric_limits<double>::infinity();
    const std::size_t noNode = nodes.size();
    std::vector<double> distances(nodes.size(), infinity);
    std::vector<std::size_t> predecessors(nodes.size(), noNode);
    using QueueEntry = std::pair<double, std::size_t>;
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<>> frontier;
    distances.at(startNode) = 0.0;
    frontier.emplace(0.0, startNode);

    while (!frontier.empty()) {
        const auto [currentDistance, currentNode] = frontier.top();
        frontier.pop();
        if (currentDistance > distances.at(currentNode) + Geometry::LengthEpsilonM) {
            continue;
        }
        for (const VisibilityEdge& edge : graph.at(currentNode)) {
            const double candidateDistance = distances.at(currentNode) + edge.lengthM;
            const bool shorter = candidateDistance < distances.at(edge.node) - Geometry::LengthEpsilonM;
            const bool stableTie = std::abs(candidateDistance - distances.at(edge.node)) <= Geometry::LengthEpsilonM &&
                                   currentNode < predecessors.at(edge.node);
            if (shorter || stableTie) {
                distances.at(edge.node) = candidateDistance;
                predecessors.at(edge.node) = currentNode;
                frontier.emplace(candidateDistance, edge.node);
            }
        }
    }

    if (!std::isfinite(distances.at(goalNode))) {
        return failure(CoveragePlanningError::SafeTransitNotFound,
                       "No safe static transit route exists between the requested points");
    }

    std::vector<std::size_t> reversedPath;
    reversedPath.reserve(nodes.size());
    std::size_t currentNode = goalNode;
    while (currentNode != startNode) {
        if ((currentNode >= nodes.size()) || (reversedPath.size() >= nodes.size())) {
            return failure(CoveragePlanningError::InvalidGeneratedPath, "Static route predecessor chain is invalid");
        }
        reversedPath.push_back(currentNode);
        currentNode = predecessors.at(currentNode);
    }
    reversedPath.push_back(startNode);
    std::ranges::reverse(reversedPath);

    StaticRoute route;
    route.path.reserve(reversedPath.size());
    for (const std::size_t node : reversedPath) {
        route.path.push_back(nodes.at(node));
    }
    for (std::size_t index = 1; index < route.path.size(); ++index) {
        if (!Geometry::segmentInsidePolygonRegionForValidatedGeometry(trackFeasibleRegion, route.path.at(index - 1),
                                                                      route.path.at(index))) {
            return failure(CoveragePlanningError::InvalidGeneratedPath,
                           "Static route contains a leg outside the track feasible region");
        }
        route.lengthM += distance(route.path.at(index - 1), route.path.at(index));
    }
    const double directDistanceM = distance(start, goal);
    if ((route.path.size() < 2) || !std::isfinite(route.lengthM) ||
        (route.lengthM + Geometry::LengthEpsilonM < directDistanceM)) {
        return failure(CoveragePlanningError::InvalidGeneratedPath, "Static route is invalid");
    }
    route.status = PlanningStatus::Success;
    route.error = CoveragePlanningError::None;
    route.message = "Safe static visibility-graph route generated";
    return route;
}

}  // namespace Marine
