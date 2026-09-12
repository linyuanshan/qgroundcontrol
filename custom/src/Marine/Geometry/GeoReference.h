#pragma once

#include <optional>

#include "GeometryTypes.h"
#include "MarineTypes.h"

namespace Marine {

class GeoReference
{
public:
    [[nodiscard]] static std::optional<GeoReference> create(const GeoPoint& origin);
    [[nodiscard]] static std::optional<GeoReference> create(const GeoPolygon& region);

    [[nodiscard]] std::optional<Point2D> toLocal(const GeoPoint& point) const;
    [[nodiscard]] std::optional<GeoPoint> toGeo(const Point2D& point) const;

    [[nodiscard]] const GeoPoint& origin() const { return _origin; }

private:
    explicit GeoReference(const GeoPoint& origin) : _origin(origin) {}

    GeoPoint _origin;
};

}  // namespace Marine
