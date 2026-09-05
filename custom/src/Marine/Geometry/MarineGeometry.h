#pragma once

#include "GeometryTypes.h"

namespace Marine::Geometry {

inline constexpr double LengthEpsilonM = 1e-3;

[[nodiscard]] bool isSimpleNonDegeneratePolygon(const Polygon2D& polygon);

}  // namespace Marine::Geometry
