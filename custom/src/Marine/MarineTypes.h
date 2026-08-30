#pragma once

#include <vector>

namespace Marine {

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

struct WorkRegion
{
    GeoPolygon outerBoundary;
    std::vector<GeoPolygon> noGoRegions;
};

}  // namespace Marine
