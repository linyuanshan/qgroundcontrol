#include "GlobalSweepSelector.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>

#include "CoverageProblemValidator.h"
#include "Geometry/MarineGeometry.h"
#include "Geometry/PolygonRegion.h"

namespace {

bool equivalentAngles(double firstDeg, double secondDeg)
{
    const double differenceDeg = std::abs(firstDeg - secondDeg);
    const double directionDifferenceDeg = std::min(differenceDeg, 180.0 - differenceDeg);
    return std::abs(std::sin(directionDifferenceDeg * std::numbers::pi / 180.0)) <= Marine::Geometry::LengthEpsilonM;
}

std::vector<double> edgeCandidates(const Marine::Polygon2D& polygon)
{
    std::vector<double> candidates;
    candidates.reserve(polygon.vertices.size());
    Marine::Point2D previous = polygon.vertices.back();
    for (const Marine::Point2D& current : polygon.vertices) {
        const double mathAngleDeg =
            std::atan2(current.yM - previous.yM, current.xM - previous.xM) * 180.0 / std::numbers::pi;
        candidates.push_back(Marine::Geometry::mathAngleToNavigationAngle(mathAngleDeg));
        previous = current;
    }

    std::ranges::sort(candidates);
    std::vector<double> uniqueCandidates;
    uniqueCandidates.reserve(candidates.size());
    for (const double candidate : candidates) {
        if (uniqueCandidates.empty() || !equivalentAngles(uniqueCandidates.back(), candidate)) {
            uniqueCandidates.push_back(candidate);
        }
    }
    if ((uniqueCandidates.size() > 1) && equivalentAngles(uniqueCandidates.front(), uniqueCandidates.back())) {
        uniqueCandidates.pop_back();
    }
    return uniqueCandidates;
}

std::pair<double, double> crossTrackBounds(const Marine::PolygonRegionSet2D& regions, double navigationAngleDeg)
{
    const double mathAngleDeg = Marine::Geometry::navigationAngleToMathAngle(navigationAngleDeg);
    double minimumY = std::numeric_limits<double>::infinity();
    double maximumY = -std::numeric_limits<double>::infinity();
    const auto accumulatePolygon = [mathAngleDeg, &minimumY, &maximumY](const Marine::Polygon2D& polygon) {
        for (const Marine::Point2D& vertex : polygon.vertices) {
            const Marine::Point2D sweepPoint = Marine::Geometry::toSweepFrame(vertex, mathAngleDeg);
            minimumY = std::min(minimumY, sweepPoint.yM);
            maximumY = std::max(maximumY, sweepPoint.yM);
        }
    };
    for (const Marine::PolygonRegion2D& region : regions) {
        accumulatePolygon(region.outerBoundary);
        for (const Marine::Polygon2D& hole : region.holes) {
            accumulatePolygon(hole);
        }
    }
    return {minimumY, maximumY};
}

bool candidateBetter(const Marine::GlobalSweepSelectionResult& candidate,
                     const Marine::GlobalSweepSelectionResult& current)
{
    if (candidate.estimatedTurnCount != current.estimatedTurnCount) {
        return candidate.estimatedTurnCount < current.estimatedTurnCount;
    }
    if (candidate.crossTrackSpanM < current.crossTrackSpanM - Marine::Geometry::LengthEpsilonM) {
        return true;
    }
    return (std::abs(candidate.crossTrackSpanM - current.crossTrackSpanM) <= Marine::Geometry::LengthEpsilonM) &&
           (candidate.selectedSweepAngleDeg < current.selectedSweepAngleDeg);
}

Marine::GlobalSweepSelectionResult failure(Marine::CoveragePlanningError error, std::string message)
{
    Marine::GlobalSweepSelectionResult result;
    result.status = Marine::CoverageProblemValidator::statusForError(error);
    result.error = error;
    result.message = std::move(message);
    return result;
}

}  // namespace

namespace Marine {

GlobalSweepSelectionResult selectGlobalSweepAngle(const Polygon2D& originalOuterBoundary,
                                                  const PolygonRegionSet2D& trackFeasibleRegion, double swathWidthM)
{
    if (!Geometry::isSimpleNonDegeneratePolygon(originalOuterBoundary) || trackFeasibleRegion.empty() ||
        !std::ranges::all_of(trackFeasibleRegion,
                             [](const PolygonRegion2D& region) { return Geometry::isValidPolygonRegion(region); })) {
        return failure(CoveragePlanningError::InvalidOuterBoundary, "Global sweep selection geometry is invalid");
    }
    if (!std::isfinite(swathWidthM) || (swathWidthM <= 0.0)) {
        return failure(CoveragePlanningError::InvalidSwathWidth,
                       CoverageProblemValidator::messageForError(CoveragePlanningError::InvalidSwathWidth));
    }

    const std::vector<double> candidates = edgeCandidates(originalOuterBoundary);
    GlobalSweepSelectionResult best;
    bool found = false;
    for (const double candidateAngleDeg : candidates) {
        const auto [minimumY, maximumY] = crossTrackBounds(trackFeasibleRegion, candidateAngleDeg);
        const double spanM = maximumY - minimumY;
        const double estimatedLaneCount = std::max(1.0, std::ceil(spanM / swathWidthM));
        if (!std::isfinite(spanM) || (spanM < 0.0) || !std::isfinite(estimatedLaneCount) ||
            (estimatedLaneCount > static_cast<double>(std::numeric_limits<int>::max()))) {
            return failure(CoveragePlanningError::GeometryFailure, "Global sweep estimate is invalid");
        }

        GlobalSweepSelectionResult candidate;
        candidate.status = PlanningStatus::Success;
        candidate.selectedSweepAngleDeg = candidateAngleDeg;
        candidate.crossTrackSpanM = spanM;
        candidate.estimatedLaneCount = static_cast<int>(estimatedLaneCount);
        candidate.estimatedTurnCount = candidate.estimatedLaneCount - 1;
        candidate.error = CoveragePlanningError::None;
        if (!found || candidateBetter(candidate, best)) {
            best = candidate;
            found = true;
        }
    }

    if (!found) {
        return failure(CoveragePlanningError::InvalidSweepAngle, "No outer-edge sweep candidate is available");
    }
    best.message = "Global sweep angle selected from outer-boundary edge estimates";
    return best;
}

}  // namespace Marine
