#include "PlanningPathMetrics.h"

#include <cmath>
#include <limits>

#include "Geometry/MarineGeometry.h"

namespace Marine {

std::optional<PlanningPathMetrics> calculatePlanningPathMetrics(std::span<const Point2D> path,
                                                                std::span<const PathLegRole> roles)
{
    if (path.size() < 2 || roles.size() != path.size() - 1) {
        return std::nullopt;
    }
    PlanningPathMetrics metrics;
    double previousDirectionX = 0.0;
    double previousDirectionY = 0.0;
    bool hasPreviousDirection = false;
    for (std::size_t index = 1; index < path.size(); ++index) {
        const Point2D& first = path[index - 1];
        const Point2D& second = path[index];
        if (!first.isFinite() || !second.isFinite()) {
            return std::nullopt;
        }
        const double lengthM = std::hypot(second.xM - first.xM, second.yM - first.yM);
        if (!std::isfinite(lengthM) || lengthM <= 0.0) {
            return std::nullopt;
        }
        const double directionX = (second.xM - first.xM) / lengthM;
        const double directionY = (second.yM - first.yM) / lengthM;
        if (hasPreviousDirection) {
            const double cross = std::abs((previousDirectionX * directionY) - (previousDirectionY * directionX));
            const double dot = (previousDirectionX * directionX) + (previousDirectionY * directionY);
            if ((cross > Geometry::LengthEpsilonM) || (dot < 0.0)) {
                if (metrics.turnCount == std::numeric_limits<int>::max()) {
                    return std::nullopt;
                }
                ++metrics.turnCount;
            }
        }
        previousDirectionX = directionX;
        previousDirectionY = directionY;
        hasPreviousDirection = true;
        switch (roles[index - 1]) {
            case PathLegRole::Coverage:
                metrics.coverageLengthM += lengthM;
                break;
            case PathLegRole::Transit:
                metrics.transitLengthM += lengthM;
                break;
            default:
                return std::nullopt;
        }
        metrics.pathLengthM += lengthM;
    }
    if (!std::isfinite(metrics.coverageLengthM) || !std::isfinite(metrics.transitLengthM) ||
        !std::isfinite(metrics.pathLengthM)) {
        return std::nullopt;
    }
    return metrics;
}

bool planningPathMetricsMatch(const PlanningPathMetrics& actual, double coverageLengthM, double transitLengthM,
                              double pathLengthM)
{
    return std::isfinite(coverageLengthM) && coverageLengthM >= 0.0 && std::isfinite(transitLengthM) &&
           transitLengthM >= 0.0 && std::isfinite(pathLengthM) && pathLengthM >= 0.0 &&
           std::abs(actual.coverageLengthM - coverageLengthM) <= PathMetricsConsistencyToleranceM &&
           std::abs(actual.transitLengthM - transitLengthM) <= PathMetricsConsistencyToleranceM &&
           std::abs(actual.pathLengthM - pathLengthM) <= PathMetricsConsistencyToleranceM;
}

}  // namespace Marine
