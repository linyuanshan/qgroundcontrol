#include "MockCoveragePlanner.h"

#include <cmath>
#include <numbers>

namespace {

constexpr double EarthRadiusM = 6371000.0;

double degreesToRadians(double degrees)
{
    return degrees * std::numbers::pi / 180.0;
}

double distanceM(const Marine::GeoPoint& first, const Marine::GeoPoint& second)
{
    const double firstLatitudeRad = degreesToRadians(first.latitudeDeg);
    const double secondLatitudeRad = degreesToRadians(second.latitudeDeg);
    const double latitudeDelta = secondLatitudeRad - firstLatitudeRad;
    const double longitudeDelta = degreesToRadians(second.longitudeDeg - first.longitudeDeg);
    const double x = longitudeDelta * std::cos((firstLatitudeRad + secondLatitudeRad) / 2.0);
    return EarthRadiusM * std::hypot(x, latitudeDelta);
}

Marine::GeoPoint polygonCenter(const Marine::GeoPolygon& polygon)
{
    Marine::GeoPoint center;
    for (const Marine::GeoPoint& point : polygon.vertices) {
        center.latitudeDeg += point.latitudeDeg;
        center.longitudeDeg += point.longitudeDeg;
        center.altitudeM += point.altitudeM;
    }

    const auto vertexCount = static_cast<double>(polygon.vertices.size());
    center.latitudeDeg /= vertexCount;
    center.longitudeDeg /= vertexCount;
    center.altitudeM /= vertexCount;
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

PlanningResult MockCoveragePlanner::plan(const MarineTask& task) const
{
    if (!task.isValid()) {
        PlanningResult result;
        result.status = PlanningStatus::InvalidInput;
        result.message = "Marine task is invalid";
        return result;
    }

    const GeoPolygon& boundary = task.region.outerBoundary;
    const GeoPoint& first = boundary.vertices.front();
    const GeoPoint center = polygonCenter(boundary);
    const GeoPoint& opposite = boundary.vertices.at(boundary.vertices.size() / 2);

    PlanningResult result;
    result.status = PlanningStatus::Success;
    result.path = {first, center, opposite};
    result.pathLengthM = distanceM(first, center) + distanceM(center, opposite);
    result.message = "Architecture test path generated. Not for field operation";
    return result;
}

}  // namespace Marine
