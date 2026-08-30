#include "ArduPilotMissionAdapter.h"

#include <cmath>
#include <limits>

#include "MissionItem.h"

namespace Marine {

bool ArduPilotMissionAdapter::appendWaypoints(const PlanningResult& result, QList<MissionItem*>& items, QObject* parent,
                                              int& sequenceNumber, QString& errorString)
{
    errorString.clear();
    if (result.status != PlanningStatus::Success) {
        errorString = QStringLiteral("Cannot create mission waypoints from an unsuccessful planning result");
        return false;
    }
    if (result.path.empty()) {
        errorString = QStringLiteral("Cannot create mission waypoints from an empty planning path");
        return false;
    }
    if (sequenceNumber < 0) {
        errorString = QStringLiteral("Mission sequence number cannot be negative");
        return false;
    }
    if (result.path.size() > static_cast<std::size_t>(std::numeric_limits<int>::max() - sequenceNumber)) {
        errorString = QStringLiteral("Mission sequence number exceeds the supported range");
        return false;
    }

    for (const GeoPoint& point : result.path) {
        if (!std::isfinite(point.latitudeDeg) || !std::isfinite(point.longitudeDeg) ||
            !std::isfinite(point.altitudeM) || (point.latitudeDeg < -90.0) || (point.latitudeDeg > 90.0) ||
            (point.longitudeDeg < -180.0) || (point.longitudeDeg > 180.0)) {
            errorString = QStringLiteral("Planning path contains an invalid coordinate");
            return false;
        }
    }

    for (const GeoPoint& point : result.path) {
        items.append(new MissionItem(sequenceNumber++, MAV_CMD_NAV_WAYPOINT, MAV_FRAME_GLOBAL_RELATIVE_ALT,
                                     0.0,                                       // Hold time
                                     0.0,                                       // Acceptance radius
                                     0.0,                                       // Pass through waypoint
                                     std::numeric_limits<double>::quiet_NaN(),  // Yaw unchanged
                                     point.latitudeDeg, point.longitudeDeg, point.altitudeM,
                                     true,                                      // autoContinue
                                     false,                                     // isCurrentItem
                                     parent));
    }

    return true;
}

}  // namespace Marine
