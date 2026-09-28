#include "MarineTask.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <random>

namespace {

std::string generateUuid()
{
    std::array<std::uint8_t, 16> bytes{};
    std::random_device randomDevice;
    std::uniform_int_distribution<unsigned int> distribution(0, 255);

    for (std::uint8_t& byte : bytes) {
        byte = static_cast<std::uint8_t>(distribution(randomDevice));
    }

    bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0F) | 0x40);
    bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3F) | 0x80);

    static constexpr char hexDigits[] = "0123456789abcdef";
    std::string uuid;
    uuid.reserve(36);

    for (std::size_t index = 0; index < bytes.size(); ++index) {
        if ((index == 4) || (index == 6) || (index == 8) || (index == 10)) {
            uuid.push_back('-');
        }
        uuid.push_back(hexDigits[bytes[index] >> 4]);
        uuid.push_back(hexDigits[bytes[index] & 0x0F]);
    }

    return uuid;
}

}  // namespace

namespace Marine {

MarineTask::MarineTask() : id(generateUuid()) {}

bool MarineTask::isValid() const
{
    return !id.empty() && (region.coverageBoundary.vertices.size() >= 3) &&
           std::isfinite(planner.executionSafety.executionMarginM) && (planner.executionSafety.executionMarginM >= 0.0);
}

bool MarineTask::schemaValid() const
{
    const auto validPolygon = [](const GeoPolygon& polygon) {
        if (polygon.vertices.size() < 3) {
            return false;
        }
        for (const GeoPoint& point : polygon.vertices) {
            if (!std::isfinite(point.latitudeDeg) || !std::isfinite(point.longitudeDeg) ||
                !std::isfinite(point.altitudeM) || (point.latitudeDeg < -90.0) || (point.latitudeDeg > 90.0) ||
                (point.longitudeDeg < -180.0) || (point.longitudeDeg > 180.0)) {
                return false;
            }
        }
        return true;
    };

    if (!isValid() || (type != MarineTaskType::CoverageInspection) || planner.plannerId.empty() ||
        !validPolygon(region.coverageBoundary) || !validPolygon(region.navigationBoundary) ||
        !std::isfinite(coverage.swathWidthM) || (coverage.swathWidthM <= 0.0) ||
        !std::isfinite(coverage.sweepAngleDeg) ||
        ((coverage.coverageRequirement != CoverageRequirement::Standard) &&
         (coverage.coverageRequirement != CoverageRequirement::Strict)) ||
        ((coverage.sweepAngleMode != SweepAngleMode::Manual) && (coverage.sweepAngleMode != SweepAngleMode::Auto)) ||
        !std::isfinite(safety.hardSafetyMarginM) || (safety.hardSafetyMarginM < 0.0) ||
        !std::isfinite(safety.preferredSafetyMarginM) || (safety.preferredSafetyMarginM < safety.hardSafetyMarginM)) {
        return false;
    }
    for (const GeoPolygon& polygon : region.noGoRegions) {
        if (!validPolygon(polygon)) {
            return false;
        }
    }
    return true;
}

}  // namespace Marine
