pragma ComponentBehavior: Bound

import QGroundControl
import QGroundControl.Controls
import QtLocation
import QtPositioning
import QtQuick

Item {
    id: root

    readonly property var _missionItem: object
    property bool interactive: true
    property var map
    property var vehicle

    signal clicked(int sequenceNumber)

    Component.onCompleted: {
        objectManager.createObjects([outerBoundaryComponent, noGoRegionsComponent, generatedPathComponent], root.map, true);
    }
    Component.onDestruction: {
        objectManager.destroyObjects();
    }

    QGCPalette {
        id: qgcPal

        colorGroupEnabled: root.enabled
    }

    QGCDynamicObjectManager {
        id: objectManager
    }

    Component {
        id: outerBoundaryComponent

        MapPolygon {
            id: outerBoundaryVisual

            border.color: qgcPal.mapMissionTrajectory
            border.width: 2
            color: qgcPal.mapMissionTrajectory
            opacity: 0.18 * root.opacity
            path: root._missionItem.outerBoundary
            visible: outerBoundaryVisual.path.length >= 3
            z: QGroundControl.zOrderWaypointLines
        }
    }

    Component {
        id: noGoRegionsComponent

        MapItemView {
            model: root._missionItem.noGoRegions

            delegate: MapPolygon {
                id: noGoPolygon

                required property var modelData

                border.color: qgcPal.warningText
                border.width: 2
                color: qgcPal.warningText
                opacity: 0.25 * root.opacity
                path: modelData
                visible: noGoPolygon.path.length >= 3
                z: QGroundControl.zOrderWaypointLines + 1
            }
        }
    }

    Component {
        id: generatedPathComponent

        MapPolyline {
            id: generatedPathVisual

            line.color: qgcPal.mapMissionTrajectory
            line.width: 3
            opacity: root.opacity
            path: root._missionItem.generatedPath
            visible: generatedPathVisual.path.length >= 2
            z: QGroundControl.zOrderWaypointLines + 2
        }
    }
}
