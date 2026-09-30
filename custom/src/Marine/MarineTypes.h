#pragma once

#include <vector>

namespace Marine {

enum class SweepAngleMode
{
    Manual,
    Auto,
};

enum class PlanningStatus
{
    Success,
    InvalidInput,
    Failed,
};

struct GeoPoint
{
    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;
    double altitudeM = 0.0;
};

struct GeoPolygon
{
    std::vector<GeoPoint> vertices;
};

/// A single repeated first 2D vertex is an optional ring terminator, not another edge.
[[nodiscard]] inline std::size_t openRingVertexCount(const GeoPolygon& polygon)
{
    const auto& vertices = polygon.vertices;
    if (vertices.size() > 1 && vertices.front().latitudeDeg == vertices.back().latitudeDeg &&
        vertices.front().longitudeDeg == vertices.back().longitudeDeg) {
        return vertices.size() - 1;
    }
    return vertices.size();
}

struct WorkRegion
{
    GeoPolygon coverageBoundary;
    GeoPolygon navigationBoundary;
    std::vector<GeoPolygon> noGoRegions;
};

}  // namespace Marine
