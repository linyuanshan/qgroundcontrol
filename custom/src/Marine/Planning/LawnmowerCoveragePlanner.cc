#include "LawnmowerCoveragePlanner.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
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
    if (normalizedProblem.sweepAngleMode != SweepAngleMode::Manual) {
        return failureSolution(CoveragePlanningError::GeometryFailure,
                               "Automatic sweep angle selection is reserved for P1-08");
    }

    Geometry::PolygonInsetResult inset =
        Geometry::insetPolygon(normalizedProblem.region.outerBoundary, normalizedProblem.safetyMarginM);
    const CoveragePlanningError insetError = errorForInsetStatus(inset.status);
    if (insetError != CoveragePlanningError::None) {
        return failureSolution(insetError);
    }

    const double sweepAngleDeg = normalizedProblem.requestedSweepAngleDeg;
    if (!Geometry::isSweepMonotone(inset.polygon, sweepAngleDeg)) {
        return failureSolution(CoveragePlanningError::NonMonotoneSweep);
    }

    const Polygon2D sweepPolygon = Geometry::toSweepFrame(inset.polygon, sweepAngleDeg);
    double minimumY = std::numeric_limits<double>::max();
    double maximumY = std::numeric_limits<double>::lowest();
    for (const Point2D& vertex : sweepPolygon.vertices) {
        minimumY = std::min(minimumY, vertex.yM);
        maximumY = std::max(maximumY, vertex.yM);
    }

    const double heightM = maximumY - minimumY;
    const double laneCountValue =
        (heightM <= normalizedProblem.swathWidthM) ? 1.0 : std::ceil(heightM / normalizedProblem.swathWidthM);
    if (!std::isfinite(laneCountValue) || (laneCountValue < 1.0) ||
        (laneCountValue > static_cast<double>(std::numeric_limits<int>::max()))) {
        return failureSolution(CoveragePlanningError::GeometryFailure);
    }

    const auto laneCount = static_cast<std::size_t>(laneCountValue);
    const double spacingM = heightM / laneCountValue;
    std::vector<Point2D> path;
    path.reserve(laneCount * 2);
    std::size_t generatedLaneCount = 0;
    for (std::size_t laneIndex = 0; laneIndex < laneCount; ++laneIndex) {
        const double laneY = minimumY + ((static_cast<double>(laneIndex) + 0.5) * spacingM);
        const Geometry::ScanlineResult intersection = Geometry::intersectScanline(sweepPolygon, laneY);
        if (intersection.status == Geometry::ScanlineStatus::NoIntersection) {
            continue;
        }
        if (intersection.status != Geometry::ScanlineStatus::Success) {
            return failureSolution(CoveragePlanningError::GeometryFailure);
        }
        if (intersection.intervals.size() != 1) {
            return failureSolution(CoveragePlanningError::NonMonotoneSweep);
        }

        const Geometry::ScanlineInterval& interval = intersection.intervals.front();
        Point2D first{.xM = interval.minimumXM, .yM = laneY};
        Point2D second{.xM = interval.maximumXM, .yM = laneY};
        if ((generatedLaneCount % 2) != 0) {
            std::swap(first, second);
        }
        path.push_back(Geometry::fromSweepFrame(first, sweepAngleDeg));
        path.push_back(Geometry::fromSweepFrame(second, sweepAngleDeg));
        ++generatedLaneCount;
    }

    if ((path.size() < 2) || (generatedLaneCount == 0)) {
        return failureSolution(CoveragePlanningError::InvalidGeneratedPath);
    }
    if (!std::isfinite(spacingM) || (spacingM > normalizedProblem.swathWidthM + Geometry::LengthEpsilonM)) {
        return failureSolution(CoveragePlanningError::InvalidGeneratedPath);
    }
    for (const Point2D& point : path) {
        if (!point.isFinite() || !Geometry::containsPoint(inset.polygon, point)) {
            return failureSolution(CoveragePlanningError::InvalidGeneratedPath);
        }
    }
    Point2D previousPoint = path.front();
    std::size_t segmentEndIndex = 1;
    for (auto iterator = std::next(path.cbegin()); iterator != path.cend(); ++iterator, ++segmentEndIndex) {
        if (!Geometry::containsSegment(inset.polygon, previousPoint, *iterator)) {
            const bool connector = (segmentEndIndex % 2) == 0;
            return failureSolution(connector ? CoveragePlanningError::UnsafeConnector
                                             : CoveragePlanningError::InvalidGeneratedPath);
        }
        previousPoint = *iterator;
    }

    const double pathLengthM = pathLength(path);
    if (!std::isfinite(pathLengthM) || (pathLengthM <= 0.0)) {
        return failureSolution(CoveragePlanningError::InvalidGeneratedPath);
    }

    CoveragePlanningSolution solution;
    solution.status = PlanningStatus::Success;
    solution.path = std::move(path);
    solution.pathLengthM = pathLengthM;
    solution.selectedSweepAngleDeg = sweepAngleDeg;
    solution.turnCount = static_cast<int>(generatedLaneCount - 1);
    solution.message = "Manual lawnmower coverage path generated";
    return solution;
}

}  // namespace Marine
