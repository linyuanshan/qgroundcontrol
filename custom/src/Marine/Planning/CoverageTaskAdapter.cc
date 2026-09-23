#include "CoverageTaskAdapter.h"

#include <cmath>
#include <utility>

#include "CoverageProblemValidator.h"

namespace {

bool toLocalPolygon(const Marine::GeoPolygon& geoPolygon, const Marine::GeoReference& geoReference,
                    Marine::Polygon2D& localPolygon)
{
    Marine::Polygon2D converted;
    converted.vertices.reserve(geoPolygon.vertices.size());
    for (const Marine::GeoPoint& point : geoPolygon.vertices) {
        const std::optional<Marine::Point2D> localPoint = geoReference.toLocal(point);
        if (!localPoint.has_value()) {
            return false;
        }
        converted.vertices.push_back(*localPoint);
    }

    localPolygon = std::move(converted);
    return true;
}

}  // namespace

namespace Marine {

bool CoverageTaskAdapter::buildProblem(const MarineTask& task, CoveragePlanningProblem& problem,
                                       std::optional<GeoReference>& geoReference, CoveragePlanningError& error)
{
    problem = {};
    geoReference.reset();
    error = CoveragePlanningError::None;
    if (!std::isfinite(task.planner.executionSafety.executionMarginM) ||
        (task.planner.executionSafety.executionMarginM < 0.0)) {
        error = CoveragePlanningError::InvalidExecutionMargin;
        return false;
    }
    if (!task.isValid()) {
        error = CoveragePlanningError::InvalidOuterBoundary;
        return false;
    }

    std::optional<GeoReference> reference = GeoReference::create(task.region.outerBoundary);
    if (!reference.has_value()) {
        error = CoveragePlanningError::InvalidOuterBoundary;
        return false;
    }

    CoveragePlanningProblem converted;
    if (!toLocalPolygon(task.region.outerBoundary, *reference, converted.region.outerBoundary)) {
        error = CoveragePlanningError::InvalidOuterBoundary;
        return false;
    }

    converted.region.noGoRegions.reserve(task.region.noGoRegions.size());
    for (const GeoPolygon& noGoRegion : task.region.noGoRegions) {
        Polygon2D localNoGo;
        if (!toLocalPolygon(noGoRegion, *reference, localNoGo)) {
            error = CoveragePlanningError::InvalidNoGoRegion;
            return false;
        }
        converted.region.noGoRegions.push_back(std::move(localNoGo));
    }

    converted.swathWidthM = task.coverage.swathWidthM;
    converted.safetyMarginM = task.coverage.safetyMarginM;
    converted.executionSafety = task.planner.executionSafety;
    converted.sweepAngleMode = task.coverage.sweepAngleMode;
    converted.requestedSweepAngleDeg = task.coverage.sweepAngleDeg;

    error = CoverageProblemValidator::validateAndNormalize(converted);
    if (error != CoveragePlanningError::None) {
        return false;
    }
    problem = std::move(converted);
    geoReference = std::move(reference);
    return true;
}

PlanningResult CoverageTaskAdapter::toPlanningResult(const CoveragePlanningSolution& solution,
                                                     const GeoReference& geoReference)
{
    PlanningResult result;
    result.status = solution.status;
    result.message = solution.message;
    if (solution.status != PlanningStatus::Success) {
        return result;
    }
    if ((solution.path.empty() && !solution.legRoles.empty()) ||
        (!solution.path.empty() && (solution.legRoles.size() != (solution.path.size() - 1)))) {
        result = {};
        result.message = "Coverage solution contains invalid path leg roles";
        return result;
    }

    result.path.reserve(solution.path.size());
    for (const Point2D& point : solution.path) {
        const std::optional<GeoPoint> geoPoint = geoReference.toGeo(point);
        if (!geoPoint.has_value()) {
            result = {};
            result.message = "Coverage solution contains an invalid local coordinate";
            return result;
        }
        result.path.push_back(*geoPoint);
    }

    result.legRoles = solution.legRoles;
    result.coverageLengthM = solution.coverageLengthM;
    result.transitLengthM = solution.transitLengthM;
    result.pathLengthM = solution.pathLengthM;
    result.selectedSweepAngleDeg = solution.selectedSweepAngleDeg;
    result.cellCount = solution.cellCount;
    result.turnCount = solution.turnCount;
    return result;
}

}  // namespace Marine
