#include "LawnmowerCoveragePlanner.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <numbers>
#include <utility>

#include "CoverageProblemValidator.h"
#include "Geometry/MarineGeometry.h"

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

double pathLength(const std::vector<Marine::Point2D>& path)
{
    double lengthM = 0.0;
    if (path.empty()) {
        return lengthM;
    }

    Marine::Point2D previous = path.front();
    for (auto iterator = std::next(path.cbegin()); iterator != path.cend(); ++iterator) {
        lengthM += std::hypot(iterator->xM - previous.xM, iterator->yM - previous.yM);
        previous = *iterator;
    }
    return lengthM;
}

double normalizedSweepAngle(double angleDeg)
{
    double normalized = std::fmod(angleDeg, 180.0);
    if (normalized < 0.0) {
        normalized += 180.0;
    }
    return (normalized == 0.0) ? 0.0 : normalized;
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
        const double angleDeg = std::atan2(second.yM - first.yM, second.xM - first.xM) * 180.0 / std::numbers::pi;
        candidates.push_back(normalizedSweepAngle(angleDeg));
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
                                                   const Marine::Polygon2D& navigablePolygon, double sweepAngleDeg)
{
    if (!Marine::Geometry::isSweepMonotone(navigablePolygon, sweepAngleDeg)) {
        return failureSolution(Marine::CoveragePlanningError::NonMonotoneSweep);
    }

    const Marine::Polygon2D sweepPolygon = Marine::Geometry::toSweepFrame(navigablePolygon, sweepAngleDeg);
    double minimumY = std::numeric_limits<double>::max();
    double maximumY = std::numeric_limits<double>::lowest();
    for (const Marine::Point2D& vertex : sweepPolygon.vertices) {
        minimumY = std::min(minimumY, vertex.yM);
        maximumY = std::max(maximumY, vertex.yM);
    }

    const double heightM = maximumY - minimumY;
    const double laneCountValue = (heightM <= problem.swathWidthM) ? 1.0 : std::ceil(heightM / problem.swathWidthM);
    if (!std::isfinite(laneCountValue) || (laneCountValue < 1.0) ||
        (laneCountValue > static_cast<double>(std::numeric_limits<int>::max()))) {
        return failureSolution(Marine::CoveragePlanningError::GeometryFailure);
    }

    const auto laneCount = static_cast<std::size_t>(laneCountValue);
    const double spacingM = heightM / laneCountValue;
    std::vector<Marine::Point2D> path;
    path.reserve(laneCount * 2);
    std::size_t generatedLaneCount = 0;
    for (std::size_t laneIndex = 0; laneIndex < laneCount; ++laneIndex) {
        const double laneY = minimumY + ((static_cast<double>(laneIndex) + 0.5) * spacingM);
        const Marine::Geometry::ScanlineResult intersection = Marine::Geometry::intersectScanline(sweepPolygon, laneY);
        if (intersection.status == Marine::Geometry::ScanlineStatus::NoIntersection) {
            continue;
        }
        if (intersection.status != Marine::Geometry::ScanlineStatus::Success) {
            return failureSolution(Marine::CoveragePlanningError::GeometryFailure);
        }
        if (intersection.intervals.size() != 1) {
            return failureSolution(Marine::CoveragePlanningError::NonMonotoneSweep);
        }

        const Marine::Geometry::ScanlineInterval& interval = intersection.intervals.front();
        Marine::Point2D first{.xM = interval.minimumXM, .yM = laneY};
        Marine::Point2D second{.xM = interval.maximumXM, .yM = laneY};
        if ((generatedLaneCount % 2) != 0) {
            std::swap(first, second);
        }
        path.push_back(Marine::Geometry::fromSweepFrame(first, sweepAngleDeg));
        path.push_back(Marine::Geometry::fromSweepFrame(second, sweepAngleDeg));
        ++generatedLaneCount;
    }

    if ((path.size() < 2) || (generatedLaneCount == 0)) {
        return failureSolution(Marine::CoveragePlanningError::InvalidGeneratedPath);
    }
    if (!std::isfinite(spacingM) || (spacingM > problem.swathWidthM + Marine::Geometry::LengthEpsilonM)) {
        return failureSolution(Marine::CoveragePlanningError::InvalidGeneratedPath);
    }
    for (const Marine::Point2D& point : path) {
        if (!point.isFinite() || !Marine::Geometry::containsPoint(navigablePolygon, point)) {
            return failureSolution(Marine::CoveragePlanningError::InvalidGeneratedPath);
        }
    }
    Marine::Point2D previousPoint = path.front();
    std::size_t segmentEndIndex = 1;
    for (auto iterator = std::next(path.cbegin()); iterator != path.cend(); ++iterator, ++segmentEndIndex) {
        if (!Marine::Geometry::containsSegment(navigablePolygon, previousPoint, *iterator)) {
            const bool connector = (segmentEndIndex % 2) == 0;
            return failureSolution(connector ? Marine::CoveragePlanningError::UnsafeConnector
                                             : Marine::CoveragePlanningError::InvalidGeneratedPath);
        }
        previousPoint = *iterator;
    }

    const double candidatePathLengthM = pathLength(path);
    if (!std::isfinite(candidatePathLengthM) || (candidatePathLengthM <= 0.0)) {
        return failureSolution(Marine::CoveragePlanningError::InvalidGeneratedPath);
    }

    Marine::CoveragePlanningSolution solution;
    solution.status = Marine::PlanningStatus::Success;
    solution.path = std::move(path);
    solution.pathLengthM = candidatePathLengthM;
    solution.selectedSweepAngleDeg = sweepAngleDeg;
    solution.turnCount = static_cast<int>(generatedLaneCount - 1);
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
    for (const double sweepAngleDeg : edgeAngleCandidates(normalizedProblem.region.outerBoundary)) {
        if (!Geometry::isSweepMonotone(inset.polygon, sweepAngleDeg)) {
            continue;
        }
        foundMonotoneAngle = true;
        CoveragePlanningSolution candidate = generateCandidate(normalizedProblem, inset.polygon, sweepAngleDeg);
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
