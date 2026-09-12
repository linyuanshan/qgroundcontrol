#include "GeoReference.h"

#include <QtPositioning/QGeoCoordinate>

#include <cmath>

#include "QGCGeo.h"

namespace {

bool isValidGeoPoint(const Marine::GeoPoint& point)
{
    return std::isfinite(point.latitudeDeg) && std::isfinite(point.longitudeDeg) && std::isfinite(point.altitudeM) &&
           (point.latitudeDeg >= -90.0) && (point.latitudeDeg <= 90.0) && (point.longitudeDeg >= -180.0) &&
           (point.longitudeDeg <= 180.0);
}

}  // namespace

namespace Marine {

std::optional<GeoReference> GeoReference::create(const GeoPoint& origin)
{
    if (!isValidGeoPoint(origin)) {
        return std::nullopt;
    }

    return GeoReference({origin.latitudeDeg, origin.longitudeDeg, 0.0});
}

std::optional<GeoReference> GeoReference::create(const GeoPolygon& region)
{
    if (region.vertices.empty()) {
        return std::nullopt;
    }

    double meanLatitudeDeg = 0.0;
    double meanLongitudeDeg = 0.0;
    std::size_t pointCount = 0;
    for (const GeoPoint& point : region.vertices) {
        if (!isValidGeoPoint(point)) {
            return std::nullopt;
        }

        ++pointCount;
        meanLatitudeDeg += (point.latitudeDeg - meanLatitudeDeg) / static_cast<double>(pointCount);
        meanLongitudeDeg += (point.longitudeDeg - meanLongitudeDeg) / static_cast<double>(pointCount);
    }

    return create({meanLatitudeDeg, meanLongitudeDeg, 0.0});
}

std::optional<Point2D> GeoReference::toLocal(const GeoPoint& point) const
{
    if (!isValidGeoPoint(point)) {
        return std::nullopt;
    }

    const QGeoCoordinate coordinate(point.latitudeDeg, point.longitudeDeg, 0.0);
    const QGeoCoordinate reference(_origin.latitudeDeg, _origin.longitudeDeg, _origin.altitudeM);
    double northM = 0.0;
    double eastM = 0.0;
    double downM = 0.0;
    QGCGeo::convertGeoToNed(coordinate, reference, northM, eastM, downM);

    const Point2D localPoint{eastM, northM};
    if (!localPoint.isFinite()) {
        return std::nullopt;
    }
    return localPoint;
}

std::optional<GeoPoint> GeoReference::toGeo(const Point2D& point) const
{
    if (!point.isFinite()) {
        return std::nullopt;
    }

    const QGeoCoordinate reference(_origin.latitudeDeg, _origin.longitudeDeg, _origin.altitudeM);
    QGeoCoordinate coordinate;
    QGCGeo::convertNedToGeo(point.yM, point.xM, 0.0, reference, coordinate);

    const GeoPoint geoPoint{coordinate.latitude(), coordinate.longitude(), 0.0};
    if (!isValidGeoPoint(geoPoint)) {
        return std::nullopt;
    }
    return geoPoint;
}

}  // namespace Marine
