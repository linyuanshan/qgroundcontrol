pragma ComponentBehavior: Bound

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlightMap
import QGroundControl.PlanView
import QtLocation
import QtPositioning
import QtQuick

Item {
    id: root

    readonly property var _missionItem: object
    readonly property bool _currentItem: root._missionItem.isCurrentItem
    readonly property var _generatedPath: root._missionItem.generatedPath
    readonly property bool _vertexDrag: root._missionItem.workRegionPolygon.vertexDrag
    property bool interactive: true
    property var map
    property var vehicle

    signal clicked(int sequenceNumber)

    Component.onCompleted: {
        objectManager.createObject(noGoRegionsComponent, root.map, false);
        objectManager.createObject(generatedPathComponent, root.map, true);
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

    QGCMapPolygonVisuals {
        mapControl: root.map
        mapPolygon: root._missionItem.workRegionPolygon
        interactive: root._currentItem && root.interactive
        borderColor: qgcPal.mapMissionTrajectory
        borderWidth: 2
        interiorColor: qgcPal.mapMissionTrajectory
        interiorOpacity: 0.18 * root.opacity
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
            line.color: "white"
            line.width: 3
            opacity: root.opacity
            path: root._generatedPath
            visible: root._currentItem && !root._vertexDrag && root._generatedPath.length >= 2
            z: QGroundControl.zOrderWaypointLines + 2
        }
    }
}
