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

    readonly property bool _currentItem: root._missionItem.isCurrentItem
    readonly property var _generatedPath: root._missionItem.generatedPath
    readonly property var _generatedPathRoleRuns: root._missionItem.generatedPathRoleRuns
    readonly property var _missionItem: object
    readonly property bool _vertexDrag: root._missionItem.workRegionPolygon.vertexDrag
    property bool interactive: true
    property var map
    property var vehicle

    signal clicked(int sequenceNumber)

    function _rebuildGeneratedPathVisuals() {
        objectManager.destroyObjects();
        if (!root.map) {
            return;
        }
        if (root._generatedPathRoleRuns.length > 0) {
            for (let index = 0; index < root._generatedPathRoleRuns.length; ++index) {
                const run = root._generatedPathRoleRuns[index];
                const visual = objectManager.createObject(rolePathComponent, root.map, true);
                visual.legRole = run.role;
                visual.path = run.path;
            }
        } else {
            objectManager.createObject(neutralPathComponent, root.map, true);
        }
    }

    Component.onCompleted: {
        root._rebuildGeneratedPathVisuals();
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

    Connections {
        function onPlanningResultChanged() {
            root._rebuildGeneratedPathVisuals();
        }

        target: root._missionItem
    }

    QGCMapPolygonVisuals {
        borderColor: qgcPal.mapMissionTrajectory
        borderWidth: 2
        interactive: root._currentItem && root.interactive && !root._missionItem.noGoRegionEditing
        interiorColor: qgcPal.mapMissionTrajectory
        interiorOpacity: 0.18 * root.opacity
        mapControl: root.map
        mapPolygon: root._missionItem.workRegionPolygon
    }

    Instantiator {
        model: root._missionItem.noGoPolygons

        delegate: QGCMapPolygonVisuals {
            required property var object

            borderColor: qgcPal.warningText
            borderWidth: 2
            interactive: root._currentItem && root.interactive && object.interactive
            interiorColor: qgcPal.warningText
            interiorOpacity: 0.25 * root.opacity
            mapControl: root.map
            mapPolygon: object
            parent: root
        }
    }

    Component {
        id: rolePathComponent

        MapPolyline {
            property string legRole

            line.color: legRole === "coverage" ? qgcPal.mapMissionTrajectory : qgcPal.colorGrey
            line.width: legRole === "coverage" ? 4 : 2
            opacity: root.opacity
            visible: root._currentItem && !root._vertexDrag
            z: QGroundControl.zOrderWaypointLines + 2
        }
    }

    Component {
        id: neutralPathComponent

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
