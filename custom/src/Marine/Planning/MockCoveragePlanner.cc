#include "MockCoveragePlanner.h"

#include <cmath>

#include "CoverageProblemValidator.h"
#include "CoverageStrategySemantics.h"

namespace {

double distanceM(const Marine::Point2D& first, const Marine::Point2D& second)
{
    return std::hypot(second.xM - first.xM, second.yM - first.yM);
}

Marine::Point2D polygonCenter(const Marine::Polygon2D& polygon)
{
    Marine::Point2D center;
    for (const Marine::Point2D& point : polygon.vertices) {
        center.xM += point.xM;
        center.yM += point.yM;
    }

    const auto vertexCount = static_cast<double>(polygon.vertices.size());
    center.xM /= vertexCount;
    center.yM /= vertexCount;
    return center;
}

}  // namespace

namespace Marine {

std::string MockCoveragePlanner::id() const
{
    return CoverageStrategySemantics::MockPlannerId;
}

std::string MockCoveragePlanner::semanticVersion() const
{
    return CoverageStrategySemantics::MockVersion;
}

std::string MockCoveragePlanner::displayName() const
{
    return "Architecture Test Planner";
}

CoveragePlanningSolution MockCoveragePlanner::plan(const CoveragePlanningProblem& problem) const
{
    CoveragePlanningProblem normalizedProblem = problem;
    const CoveragePlanningError validationError = CoverageProblemValidator::validateAndNormalize(normalizedProblem);
    if (validationError != CoveragePlanningError::None) {
        CoveragePlanningSolution solution;
        solution.status = CoverageProblemValidator::statusForError(validationError);
        solution.error = validationError;
        solution.message = CoverageProblemValidator::messageForError(validationError);
        return solution;
    }
    const auto boundaryError = CoverageProblemValidator::validateLegacyCoincidentBoundaries(normalizedProblem.region);
    if (boundaryError != CoveragePlanningError::None) {
        CoveragePlanningSolution solution;
        solution.status = CoverageProblemValidator::statusForError(boundaryError);
        solution.error = boundaryError;
        solution.message = CoverageProblemValidator::messageForError(boundaryError);
        return solution;
    }
    if (!normalizedProblem.region.noGoRegions.empty()) {
        CoveragePlanningSolution solution;
        solution.status = CoverageProblemValidator::statusForError(CoveragePlanningError::UnsupportedNoGoRegion);
        solution.error = CoveragePlanningError::UnsupportedNoGoRegion;
        solution.message = CoverageProblemValidator::messageForError(CoveragePlanningError::UnsupportedNoGoRegion);
        return solution;
    }

    const Polygon2D& boundary = normalizedProblem.region.coverageBoundary;
    const Point2D& first = boundary.vertices.front();
    const Point2D center = polygonCenter(boundary);
    const Point2D& opposite = boundary.vertices.at(boundary.vertices.size() / 2);

    CoveragePlanningSolution solution;
    solution.status = PlanningStatus::Success;
    solution.path = {first, center, opposite};
    solution.legRoles = {PathLegRole::Coverage, PathLegRole::Transit};
    solution.coverageLengthM = distanceM(first, center);
    solution.transitLengthM = distanceM(center, opposite);
    solution.pathLengthM = solution.coverageLengthM + solution.transitLengthM;
    solution.selectedSweepAngleDeg = normalizedProblem.requestedSweepAngleDeg;
    solution.cellCount = 1;
    solution.turnCount = 1;
    solution.message = "Architecture test path generated. Not for field operation";
    solution.plannerSource =
        PlannerSourceInfo{.requestedPlannerId = id(),
                          .resolvedStrategy = {.strategyId = id(), .semanticVersion = semanticVersion()},
                          .requestedSweepMode = problem.sweepAngleMode,
                          .selectedSweepAngleDeg = solution.selectedSweepAngleDeg};
    return solution;
}

}  // namespace Marine
