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

enum class ScanlineStatus
{
    Success,
    NoIntersection,
    InvalidInput,
    GeometryFailure,
};

struct ScanlineInterval
{
    double minimumXM = 0.0;
    double maximumXM = 0.0;
};

struct ScanlineResult
{
    ScanlineStatus status = ScanlineStatus::GeometryFailure;
    std::vector<ScanlineInterval> intervals;
};

[[nodiscard]] bool isSimpleNonDegeneratePolygon(const Polygon2D& polygon);
[[nodiscard]] PolygonInsetResult insetPolygon(const Polygon2D& polygon, double marginM);
[[nodiscard]] Point2D toSweepFrame(const Point2D& point, double sweepAngleDeg);
[[nodiscard]] Polygon2D toSweepFrame(const Polygon2D& polygon, double sweepAngleDeg);
[[nodiscard]] Point2D fromSweepFrame(const Point2D& point, double sweepAngleDeg);
[[nodiscard]] bool isSweepMonotone(const Polygon2D& polygon, double sweepAngleDeg);
[[nodiscard]] ScanlineResult intersectScanline(const Polygon2D& sweepAlignedPolygon, double yM);
[[nodiscard]] bool containsPoint(const Polygon2D& polygon, const Point2D& point);
[[nodiscard]] bool containsSegment(const Polygon2D& polygon, const Point2D& first, const Point2D& second);

}  // namespace Marine::Geometry
