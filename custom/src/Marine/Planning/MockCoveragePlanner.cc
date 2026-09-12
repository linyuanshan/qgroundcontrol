#include "MockCoveragePlanner.h"

#include <cmath>

#include "CoverageProblemValidator.h"

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
    return "marine.coverage.mock";
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

    const Polygon2D& boundary = normalizedProblem.region.outerBoundary;
    const Point2D& first = boundary.vertices.front();
    const Point2D center = polygonCenter(boundary);
    const Point2D& opposite = boundary.vertices.at(boundary.vertices.size() / 2);

    CoveragePlanningSolution solution;
    solution.status = PlanningStatus::Success;
    solution.path = {first, center, opposite};
    solution.pathLengthM = distanceM(first, center) + distanceM(center, opposite);
    solution.selectedSweepAngleDeg = normalizedProblem.requestedSweepAngleDeg;
    solution.turnCount = 1;
    solution.message = "Architecture test path generated. Not for field operation";
    return solution;
}

}  // namespace Marine
