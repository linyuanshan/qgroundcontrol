#pragma once

#include "GeometryTypes.h"

namespace Marine::Geometry {

inline constexpr double LengthEpsilonM = 1e-3;
inline constexpr double CoordinateScalePerM = 1000.0;

enum class PolygonInsetStatus
{
    Success,
    InvalidInput,
    Empty,
    Disconnected,
    GeometryFailure,
};

struct PolygonInsetResult
{
    PolygonInsetStatus status = PolygonInsetStatus::GeometryFailure;
    Polygon2D polygon;
};

[[nodiscard]] bool isSimpleNonDegeneratePolygon(const Polygon2D& polygon);
[[nodiscard]] PolygonInsetResult insetPolygon(const Polygon2D& polygon, double marginM);

}  // namespace Marine::Geometry
