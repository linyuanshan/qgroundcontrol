#include "LawnmowerCoveragePlanner.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

#include "CoverageProblemValidator.h"
#include "Geometry/MarineGeometry.h"
#include "MonotoneCoverage.h"

namespace {

Marine::CoveragePlanningSolution failureSolution(Marine::CoveragePlanningError error, std::string message = {})
{
    Marine::CoveragePlanningSolution solution;
    solution.status = Marine::CoverageProblemValidator::statusForError(error);
    solution.error = error;
    solution.message = message.empty() ? Marine::CoverageProblemValidator::messageForError(error) : std::move(message);
    return solution;
}

Marine::CoveragePlanningError errorForInsetStatus(Marine::Geometry::PolygonInsetStatus status)
{
    switch (status) {
        case Marine::Geometry::PolygonInsetStatus::Success:
            return Marine::CoveragePlanningError::None;
        case Marine::Geometry::PolygonInsetStatus::Empty:
            return Marine::CoveragePlanningError::SafetyInsetEmpty;
        case Marine::Geometry::PolygonInsetStatus::Disconnected:
            return Marine::CoveragePlanningError::SafetyInsetDisconnected;
        case Marine::Geometry::PolygonInsetStatus::InvalidInput:
        case Marine::Geometry::PolygonInsetStatus::GeometryFailure:
            return Marine::CoveragePlanningError::GeometryFailure;
    }
    return Marine::CoveragePlanningError::GeometryFailure;
}

bool equivalentSweepAngles(double firstDeg, double secondDeg)
{
    const double differenceDeg = std::abs(firstDeg - secondDeg);
    const double directionDifferenceDeg = std::min(differenceDeg, 180.0 - differenceDeg);
    return std::abs(std::sin(directionDifferenceDeg * std::numbers::pi / 180.0)) <= Marine::Geometry::LengthEpsilonM;
}

std::vector<double> edgeAngleCandidates(const Marine::Polygon2D& polygon)
{
    std::vector<double> candidates;
    candidates.reserve(polygon.vertices.size());
    Marine::Point2D first = polygon.vertices.back();
    for (const Marine::Point2D& second : polygon.vertices) {
        const double mathAngleDeg = std::atan2(second.yM - first.yM, second.xM - first.xM) * 180.0 / std::numbers::pi;
        candidates.push_back(Marine::Geometry::mathAngleToNavigationAngle(mathAngleDeg));
        first = second;
    }

    std::ranges::sort(candidates);
    std::vector<double> uniqueCandidates;
    uniqueCandidates.reserve(candidates.size());
    for (const double candidate : candidates) {
        if (uniqueCandidates.empty() || !equivalentSweepAngles(uniqueCandidates.back(), candidate)) {
            uniqueCandidates.push_back(candidate);
        }
    }
    if ((uniqueCandidates.size() > 1) && equivalentSweepAngles(uniqueCandidates.front(), uniqueCandidates.back())) {
        uniqueCandidates.pop_back();
    }
    return uniqueCandidates;
}

bool candidateIsBetter(const Marine::CoveragePlanningSolution& candidate,
                       const Marine::CoveragePlanningSolution& current)
{
    if (candidate.turnCount != current.turnCount) {
        return candidate.turnCount < current.turnCount;
    }
    if (candidate.pathLengthM < current.pathLengthM - Marine::Geometry::LengthEpsilonM) {
        return true;
    }
    if (std::abs(candidate.pathLengthM - current.pathLengthM) <= Marine::Geometry::LengthEpsilonM) {
        return candidate.selectedSweepAngleDeg < current.selectedSweepAngleDeg;
    }
    return false;
}

Marine::CoveragePlanningSolution generateCandidate(const Marine::CoveragePlanningProblem& problem,
                                                   const Marine::Polygon2D& navigablePolygon, double navigationAngleDeg)
{
    const double mathAngleDeg = Marine::Geometry::navigationAngleToMathAngle(navigationAngleDeg);
    if (!Marine::Geometry::isSweepMonotone(navigablePolygon, mathAngleDeg)) {
        return failureSolution(Marine::CoveragePlanningError::NonMonotoneSweep);
    }

    const Marine::MonotoneCoverageResult primitiveResult = Marine::generateMonotoneCoverage(
        problem.region.outerBoundary, navigablePolygon, problem.swathWidthM, navigationAngleDeg);

    Marine::CoveragePlanningError primitiveError = Marine::CoveragePlanningError::GeometryFailure;
    switch (primitiveResult.error) {
        case Marine::MonotoneCoverageError::None:
            primitiveError = Marine::CoveragePlanningError::None;
            break;
        case Marine::MonotoneCoverageError::InvalidSwathWidth:
            primitiveError = Marine::CoveragePlanningError::InvalidSwathWidth;
            break;
        case Marine::MonotoneCoverageError::InvalidSweepAngle:
            primitiveError = Marine::CoveragePlanningError::InvalidSweepAngle;
            break;
        case Marine::MonotoneCoverageError::CoverageImpossible:
            primitiveError = Marine::CoveragePlanningError::CoverageImpossibleWithSafetyMargin;
            break;
        case Marine::MonotoneCoverageError::NonMonotoneSweep:
        case Marine::MonotoneCoverageError::MultipleIntervals:
            primitiveError = Marine::CoveragePlanningError::NonMonotoneSweep;
            break;
        case Marine::MonotoneCoverageError::NoIntersection:
            primitiveError = Marine::CoveragePlanningError::CoverageImpossibleWithSafetyMargin;
            break;
        case Marine::MonotoneCoverageError::UnsafeConnector:
            primitiveError = Marine::CoveragePlanningError::UnsafeConnector;
            break;
        case Marine::MonotoneCoverageError::GeometryFailure:
            primitiveError = Marine::CoveragePlanningError::GeometryFailure;
            break;
        case Marine::MonotoneCoverageError::InvalidTargetPolygon:
        case Marine::MonotoneCoverageError::InvalidNavigablePolygon:
        case Marine::MonotoneCoverageError::InvalidLaneSchedule:
        case Marine::MonotoneCoverageError::InvalidGeneratedPath:
            primitiveError = Marine::CoveragePlanningError::InvalidGeneratedPath;
            break;
    }
    if (primitiveResult.status != Marine::PlanningStatus::Success) {
        return failureSolution(primitiveError);
    }

    Marine::CoveragePlanningSolution solution;
    solution.status = Marine::PlanningStatus::Success;
    solution.path = primitiveResult.path;
    solution.legRoles = primitiveResult.legRoles;
    solution.pathLengthM = primitiveResult.pathLengthM;
    solution.selectedSweepAngleDeg = navigationAngleDeg;
    solution.turnCount = primitiveResult.turnCount;
    return solution;
}

}  // namespace

namespace Marine {

std::string LawnmowerCoveragePlanner::id() const
{
    return "marine.coverage.lawnmower";
}

std::string LawnmowerCoveragePlanner::displayName() const
{
    return "Lawnmower Coverage Planner";
}

CoveragePlanningSolution LawnmowerCoveragePlanner::plan(const CoveragePlanningProblem& problem) const
{
    CoveragePlanningProblem normalizedProblem = problem;
    const CoveragePlanningError validationError = CoverageProblemValidator::validateAndNormalize(normalizedProblem);
    if (validationError != CoveragePlanningError::None) {
        return failureSolution(validationError);
    }
    if (!normalizedProblem.region.noGoRegions.empty()) {
        return failureSolution(CoveragePlanningError::UnsupportedNoGoRegion);
    }

    Geometry::PolygonInsetResult inset =
        Geometry::insetPolygon(normalizedProblem.region.outerBoundary, normalizedProblem.safetyMarginM);
    const CoveragePlanningError insetError = errorForInsetStatus(inset.status);
    if (insetError != CoveragePlanningError::None) {
        return failureSolution(insetError);
    }

    if (normalizedProblem.sweepAngleMode == SweepAngleMode::Manual) {
        CoveragePlanningSolution solution =
            generateCandidate(normalizedProblem, inset.polygon, normalizedProblem.requestedSweepAngleDeg);
        if (solution.status == PlanningStatus::Success) {
            solution.message = "Manual lawnmower coverage path generated";
        }
        return solution;
    }

    CoveragePlanningSolution bestSolution;
    bool foundCandidate = false;
    bool foundMonotoneAngle = false;
    CoveragePlanningError candidateFailure = CoveragePlanningError::InvalidGeneratedPath;
    for (const double navigationAngleDeg : edgeAngleCandidates(normalizedProblem.region.outerBoundary)) {
        const double mathAngleDeg = Geometry::navigationAngleToMathAngle(navigationAngleDeg);
        if (!Geometry::isSweepMonotone(inset.polygon, mathAngleDeg)) {
            continue;
        }
        foundMonotoneAngle = true;
        CoveragePlanningSolution candidate = generateCandidate(normalizedProblem, inset.polygon, navigationAngleDeg);
        if (candidate.status != PlanningStatus::Success) {
            if ((candidate.error == CoveragePlanningError::UnsafeConnector) ||
                (candidateFailure != CoveragePlanningError::UnsafeConnector)) {
                candidateFailure = candidate.error;
            }
            continue;
        }
        if (!foundCandidate || candidateIsBetter(candidate, bestSolution)) {
            bestSolution = std::move(candidate);
            foundCandidate = true;
        }
    }

    if (!foundCandidate) {
        return failureSolution(foundMonotoneAngle ? candidateFailure : CoveragePlanningError::NonMonotoneSweep,
                               "No edge-angle candidate produced a safe coverage path");
    }
    bestSolution.message = "Automatic lawnmower coverage path generated";
    return bestSolution;
}

}  // namespace Marine
