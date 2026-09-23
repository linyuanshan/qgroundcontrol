#include "MonotoneCoverage.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <numbers>
#include <utility>

#include "Geometry/MarineGeometry.h"
#include "Geometry/PolygonRegion.h"

namespace {

using Marine::MonotoneCoverageError;
using Marine::MonotoneCoverageResult;
using Marine::Point2D;
using Marine::Polygon2D;

enum class GeometryValidationMode
{
    StrictInput,
    BackendDerived,
};

MonotoneCoverageResult failure(Marine::PlanningStatus status, MonotoneCoverageError error, std::string message)
{
    MonotoneCoverageResult result;
    result.status = status;
    result.error = error;
    result.message = std::move(message);
    return result;
}

std::pair<double, double> crossTrackExtents(const Polygon2D& polygon)
{
    std::pair<double, double> extents{std::numeric_limits<double>::max(), std::numeric_limits<double>::lowest()};
    for (const Point2D& vertex : polygon.vertices) {
        extents.first = std::min(extents.first, vertex.yM);
        extents.second = std::max(extents.second, vertex.yM);
    }
    return extents;
}

struct LaneScheduleResult
{
    std::vector<double> positionsYM;
    MonotoneCoverageError error = MonotoneCoverageError::None;
    std::string message;
};

LaneScheduleResult deriveLaneSchedule(const Polygon2D& targetPolygon, const Polygon2D& navigablePolygon,
                                      double swathWidthM, double mathAngleDeg)
{
    const Polygon2D targetSweepPolygon = Marine::Geometry::toSweepFrame(targetPolygon, mathAngleDeg);
    const Polygon2D navigableSweepPolygon = Marine::Geometry::toSweepFrame(navigablePolygon, mathAngleDeg);
    const auto [targetMinimumY, targetMaximumY] = crossTrackExtents(targetSweepPolygon);
    const auto [navigableMinimumY, navigableMaximumY] = crossTrackExtents(navigableSweepPolygon);
    const double halfSwathM = swathWidthM / 2.0;
    const double firstLaneMaximumY = targetMinimumY + halfSwathM;
    const double lastLaneMinimumY = targetMaximumY - halfSwathM;

    if ((navigableMinimumY > firstLaneMaximumY + Marine::Geometry::LengthEpsilonM) ||
        (navigableMaximumY < lastLaneMinimumY - Marine::Geometry::LengthEpsilonM)) {
        return {.error = MonotoneCoverageError::CoverageImpossible,
                .message = "Navigable polygon cannot cover the nominal target"};
    }

    if (lastLaneMinimumY <= firstLaneMaximumY + Marine::Geometry::LengthEpsilonM) {
        const double feasibleMinimumY = std::max(navigableMinimumY, lastLaneMinimumY);
        const double feasibleMaximumY = std::min(navigableMaximumY, firstLaneMaximumY);
        if (feasibleMinimumY > feasibleMaximumY + Marine::Geometry::LengthEpsilonM) {
            return {.error = MonotoneCoverageError::CoverageImpossible,
                    .message = "Navigable polygon cannot cover the nominal target"};
        }
        return {.positionsYM = {(feasibleMinimumY + feasibleMaximumY) / 2.0}};
    }

    const double firstLaneY = std::clamp(firstLaneMaximumY, navigableMinimumY, navigableMaximumY);
    const double lastLaneY = std::clamp(lastLaneMinimumY, navigableMinimumY, navigableMaximumY);
    const double laneSpanM = lastLaneY - firstLaneY;
    const double intervalCountValue = std::ceil(laneSpanM / swathWidthM);
    if (!std::isfinite(intervalCountValue) || (intervalCountValue < 1.0) ||
        (intervalCountValue >= static_cast<double>(std::numeric_limits<int>::max()))) {
        return {.error = MonotoneCoverageError::GeometryFailure,
                .message = "Coverage lane schedule cannot be represented"};
    }

    const auto intervalCount = static_cast<std::size_t>(intervalCountValue);
    const double spacingM = laneSpanM / intervalCountValue;
    std::vector<double> lanePositionsYM;
    lanePositionsYM.reserve(intervalCount + 1);
    for (std::size_t laneIndex = 0; laneIndex <= intervalCount; ++laneIndex) {
        lanePositionsYM.push_back(firstLaneY + (static_cast<double>(laneIndex) * spacingM));
    }
    return {.positionsYM = std::move(lanePositionsYM)};
}

MonotoneCoverageResult generateCore(const Polygon2D& targetPolygon, const Polygon2D& navigablePolygon,
                                    double swathWidthM, double navigationAngleDeg,
                                    std::span<const double> lanePositionsYM, GeometryValidationMode validationMode,
                                    bool startFromMaximumX = false)
{
    const bool backendDerived = validationMode == GeometryValidationMode::BackendDerived;
    const Marine::PolygonRegionSet2D targetRegions{{.outerBoundary = targetPolygon}};
    const Marine::PolygonRegionSet2D navigableRegions{{.outerBoundary = navigablePolygon}};
    const bool targetValid = backendDerived ? Marine::Geometry::isValidPolygonRegion(targetRegions.front())
                                            : Marine::Geometry::isSimpleNonDegeneratePolygon(targetPolygon);
    if (!targetValid) {
        return failure(Marine::PlanningStatus::InvalidInput, MonotoneCoverageError::InvalidTargetPolygon,
                       "Target polygon is invalid");
    }
    const bool navigableValid = backendDerived ? Marine::Geometry::isValidPolygonRegion(navigableRegions.front())
                                               : Marine::Geometry::isSimpleNonDegeneratePolygon(navigablePolygon);
    if (!navigableValid) {
        return failure(Marine::PlanningStatus::InvalidInput, MonotoneCoverageError::InvalidNavigablePolygon,
                       "Navigable polygon is invalid");
    }
    if (!std::isfinite(swathWidthM) || (swathWidthM <= 0.0)) {
        return failure(Marine::PlanningStatus::InvalidInput, MonotoneCoverageError::InvalidSwathWidth,
                       "Coverage swath width must be finite and greater than zero");
    }
    if (!std::isfinite(navigationAngleDeg)) {
        return failure(Marine::PlanningStatus::InvalidInput, MonotoneCoverageError::InvalidSweepAngle,
                       "Sweep angle must be finite");
    }
    if (lanePositionsYM.empty() || (lanePositionsYM.size() > (std::numeric_limits<std::size_t>::max() / 2))) {
        return failure(Marine::PlanningStatus::InvalidInput, MonotoneCoverageError::InvalidLaneSchedule,
                       "Coverage lane schedule must contain at least one lane");
    }

    const double mathAngleDeg = Marine::Geometry::navigationAngleToMathAngle(navigationAngleDeg);
    const bool targetMonotone = !backendDerived || Marine::Geometry::isMonotoneCellPolygon(targetPolygon, mathAngleDeg);
    const bool navigableMonotone = backendDerived
                                       ? Marine::Geometry::isMonotoneCellPolygon(navigablePolygon, mathAngleDeg)
                                       : Marine::Geometry::isSweepMonotone(navigablePolygon, mathAngleDeg);
    if (!targetMonotone || !navigableMonotone) {
        return failure(Marine::PlanningStatus::Failed, MonotoneCoverageError::NonMonotoneSweep,
                       "Navigable polygon is not monotone for the selected sweep angle");
    }

    double spacingM = 0.0;
    if (lanePositionsYM.size() > 1) {
        auto laneIterator = std::next(lanePositionsYM.begin());
        double previousLaneY = lanePositionsYM.front();
        spacingM = *laneIterator - previousLaneY;
        if (!std::isfinite(spacingM) || (spacingM <= 0.0)) {
            return failure(Marine::PlanningStatus::InvalidInput, MonotoneCoverageError::InvalidLaneSchedule,
                           "Coverage lane schedule must be strictly increasing");
        }
        for (; laneIterator != lanePositionsYM.end(); ++laneIterator) {
            const double laneY = *laneIterator;
            const double laneSpacingM = laneY - previousLaneY;
            if (!std::isfinite(laneY) || (laneSpacingM <= 0.0) ||
                (laneSpacingM > swathWidthM + Marine::Geometry::LengthEpsilonM)) {
                return failure(Marine::PlanningStatus::InvalidInput, MonotoneCoverageError::InvalidLaneSchedule,
                               "Coverage lane spacing is invalid");
            }
            previousLaneY = laneY;
        }
    } else if (!std::isfinite(lanePositionsYM.front())) {
        return failure(Marine::PlanningStatus::InvalidInput, MonotoneCoverageError::InvalidLaneSchedule,
                       "Coverage lane schedule contains a non-finite position");
    }

    const Polygon2D targetSweepPolygon = Marine::Geometry::toSweepFrame(targetPolygon, mathAngleDeg);
    const Polygon2D navigableSweepPolygon = Marine::Geometry::toSweepFrame(navigablePolygon, mathAngleDeg);
    const auto [targetMinimumY, targetMaximumY] = crossTrackExtents(targetSweepPolygon);
    const double halfSwathM = swathWidthM / 2.0;
    if ((lanePositionsYM.front() - halfSwathM > targetMinimumY + Marine::Geometry::LengthEpsilonM) ||
        (lanePositionsYM.back() + halfSwathM < targetMaximumY - Marine::Geometry::LengthEpsilonM)) {
        return failure(Marine::PlanningStatus::Failed, MonotoneCoverageError::InvalidGeneratedPath,
                       "Coverage lanes do not reach the nominal target");
    }

    MonotoneCoverageResult result;
    result.status = Marine::PlanningStatus::Success;
    result.path.reserve(lanePositionsYM.size() * 2);
    result.legRoles.reserve((lanePositionsYM.size() * 2) - 1);
    result.lanes.reserve(lanePositionsYM.size());
    std::size_t laneIndex = 0;
    for (const double laneY : lanePositionsYM) {
        const Marine::Geometry::ScanlineResult intersection =
            backendDerived ? Marine::Geometry::intersectScanlineForValidatedGeometry(navigableSweepPolygon, laneY)
                           : Marine::Geometry::intersectScanline(navigableSweepPolygon, laneY);
        if (intersection.status == Marine::Geometry::ScanlineStatus::NoIntersection) {
            return failure(Marine::PlanningStatus::Failed, MonotoneCoverageError::NoIntersection,
                           "Coverage lane does not intersect the navigable polygon");
        }
        if (intersection.status != Marine::Geometry::ScanlineStatus::Success) {
            return failure(Marine::PlanningStatus::Failed, MonotoneCoverageError::GeometryFailure,
                           "Coverage lane intersection failed");
        }
        if (intersection.intervals.size() != 1) {
            return failure(Marine::PlanningStatus::Failed, MonotoneCoverageError::MultipleIntervals,
                           "Coverage lane intersects more than one navigable interval");
        }

        const Marine::Geometry::ScanlineInterval& interval = intersection.intervals.front();
        Point2D first{.xM = interval.minimumXM, .yM = laneY};
        Point2D second{.xM = interval.maximumXM, .yM = laneY};
        if (((laneIndex % 2) != 0) != startFromMaximumX) {
            std::swap(first, second);
        }
        const std::size_t startIndex = result.path.size();
        result.path.push_back(Marine::Geometry::fromSweepFrame(first, mathAngleDeg));
        result.path.push_back(Marine::Geometry::fromSweepFrame(second, mathAngleDeg));
        result.lanes.push_back({.startIndex = startIndex, .endIndex = startIndex + 1, .positionYM = laneY});
        ++laneIndex;
    }

    if (result.path.size() < 2) {
        return failure(Marine::PlanningStatus::Failed, MonotoneCoverageError::InvalidGeneratedPath,
                       "Coverage planner generated an empty path");
    }
    for (const Point2D& point : result.path) {
        const bool contained = backendDerived ? Marine::Geometry::pointInsidePolygonRegion(navigableRegions, point)
                                              : Marine::Geometry::containsPoint(navigablePolygon, point);
        if (!point.isFinite() || !contained) {
            return failure(Marine::PlanningStatus::Failed, MonotoneCoverageError::InvalidGeneratedPath,
                           "Coverage path contains a point outside the navigable polygon");
        }
    }

    Point2D previousPoint = result.path.front();
    std::size_t segmentIndex = 0;
    for (auto pathIterator = std::next(result.path.cbegin()); pathIterator != result.path.cend();
         ++pathIterator, ++segmentIndex) {
        const Point2D& nextPoint = *pathIterator;
        const bool contained =
            backendDerived ? Marine::Geometry::segmentInsidePolygonRegion(navigableRegions, previousPoint, nextPoint)
                           : Marine::Geometry::containsSegment(navigablePolygon, previousPoint, nextPoint);
        if (!contained) {
            const bool connector = (segmentIndex % 2) == 1;
            return failure(
                Marine::PlanningStatus::Failed,
                connector ? MonotoneCoverageError::UnsafeConnector : MonotoneCoverageError::InvalidGeneratedPath,
                connector ? "Coverage path requires an unsafe lane connector"
                          : "Coverage lane is not contained in the navigable polygon");
        }
        const double segmentLengthM = std::hypot(nextPoint.xM - previousPoint.xM, nextPoint.yM - previousPoint.yM);
        if (!std::isfinite(segmentLengthM)) {
            return failure(Marine::PlanningStatus::Failed, MonotoneCoverageError::InvalidGeneratedPath,
                           "Coverage path contains a non-finite segment");
        }
        result.pathLengthM += segmentLengthM;
        if ((segmentIndex % 2) == 0) {
            result.coverageLengthM += segmentLengthM;
            result.legRoles.push_back(Marine::PathLegRole::Coverage);
        } else {
            result.transitLengthM += segmentLengthM;
            result.legRoles.push_back(Marine::PathLegRole::Transit);
        }
        previousPoint = nextPoint;
    }

    if (!std::isfinite(result.pathLengthM) || (result.pathLengthM <= 0.0) || !std::isfinite(result.coverageLengthM) ||
        !std::isfinite(result.transitLengthM) || (result.legRoles.size() != result.path.size() - 1)) {
        return failure(Marine::PlanningStatus::Failed, MonotoneCoverageError::InvalidGeneratedPath,
                       "Coverage planner generated an invalid path");
    }

    result.laneSpacingM = spacingM;
    result.laneCount = static_cast<int>(result.lanes.size());
    result.turnCount = result.laneCount - 1;
    result.message = "Fixed-angle monotone coverage path generated";
    return result;
}

}  // namespace

namespace Marine {

MonotoneCoverageResult generateMonotoneCoverage(const Polygon2D& targetPolygon, const Polygon2D& navigablePolygon,
                                                double swathWidthM, double navigationAngleDeg,
                                                std::span<const double> lanePositionsYM)
{
    return generateCore(targetPolygon, navigablePolygon, swathWidthM, navigationAngleDeg, lanePositionsYM,
                        GeometryValidationMode::StrictInput);
}

MonotoneCoverageResult generateMonotoneCoverageForValidatedGeometry(const Polygon2D& targetPolygon,
                                                                    const Polygon2D& navigablePolygon,
                                                                    double swathWidthM, double navigationAngleDeg,
                                                                    std::span<const double> lanePositionsYM,
                                                                    bool startFromMaximumX)
{
    return generateCore(targetPolygon, navigablePolygon, swathWidthM, navigationAngleDeg, lanePositionsYM,
                        GeometryValidationMode::BackendDerived, startFromMaximumX);
}

MonotoneCoverageResult generateMonotoneCoverage(const Polygon2D& targetPolygon, const Polygon2D& navigablePolygon,
                                                double swathWidthM, double navigationAngleDeg)
{
    if (!Geometry::isSimpleNonDegeneratePolygon(targetPolygon) ||
        !Geometry::isSimpleNonDegeneratePolygon(navigablePolygon) || !std::isfinite(swathWidthM) ||
        (swathWidthM <= 0.0) || !std::isfinite(navigationAngleDeg)) {
        return generateCore(targetPolygon, navigablePolygon, swathWidthM, navigationAngleDeg, {},
                            GeometryValidationMode::StrictInput);
    }
    const double mathAngleDeg = Geometry::navigationAngleToMathAngle(navigationAngleDeg);
    const LaneScheduleResult schedule = deriveLaneSchedule(targetPolygon, navigablePolygon, swathWidthM, mathAngleDeg);
    if (schedule.error != MonotoneCoverageError::None) {
        return failure(PlanningStatus::Failed, schedule.error, schedule.message);
    }
    return generateCore(targetPolygon, navigablePolygon, swathWidthM, navigationAngleDeg, schedule.positionsYM,
                        GeometryValidationMode::StrictInput);
}

}  // namespace Marine
