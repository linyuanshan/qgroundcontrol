pragma ComponentBehavior: Bound
import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlightMap
import QtLocation
import QtQuick

Item {
    id: root

    readonly property bool _currentItem: _missionItem.isCurrentItem
    property var _labelRects: []
    readonly property var _missionItem: object
    readonly property var _result: _missionItem.planningPresentation
    property bool interactive: true
    property var map
    property var vehicle

    signal clicked(int sequenceNumber)

    function addRegions(regions, kind, explanation) {
        for (let i = 0; i < regions.length; ++i) {
            const area = objects.createObject(regionComponent, root.map, true);
            area.regionKind = kind;
            area.geoShape = regions[i].geoShape;
            area.objectName = "marine_region_" + kind + "_" + regions[i].componentIndex;
            if (regions[i].labelCoordinate) {
                labelAt(regions[i].labelCoordinate, explanation, true, kind === "BoundaryShortfall" ? 4 : kind === "CriticalUncovered" ? 5 : 6);
            }
        }
    }

    function addRuns(runs, diagnostic) {
        for (let i = 0; i < runs.length; ++i) {
            const run = runs[i];
            const visual = objects.createObject(routeComponent, root.map, true);
            visual.diagnostic = diagnostic;
            visual.role = run.role;
            visual.safetyClass = run.safetyClass;
            visual.path = run.path;
            visual.objectName = (diagnostic ? "marine_diagnosticRun_" : "marine_canonicalRun_") + i;
            if (run.path.length) {
                const warning = run.safetyClass === "HardSafeWarning";
                labelAt(run.path[0], diagnostic ? qsTr("Diagnostic — NOT EXECUTABLE") : (run.role === "Coverage" ? qsTr("Coverage") : qsTr("Transit")) + (warning ? qsTr(" ⚠ Hard-safe warning") : ""), diagnostic || warning, run.role === "Coverage" ? 2 : 3);
            }
        }
    }

    function labelAt(coordinate, text, warning, offset) {
        if (!root.map.mapReady) {
            return;
        }
        const point = root.map.fromCoordinate(coordinate, false);
        if (!Number.isFinite(point.x) || !Number.isFinite(point.y) || point.x < 0 || point.y < 0 || point.x > root.map.width || point.y > root.map.height) {
            return;
        }
        const label = objects.createObject(labelComponent, root.map, true);
        label.objectName = "marine_mapLabel";
        label.coordinate = coordinate;
        label.labelText = text;
        label.warning = warning;
        let slot = offset || 0;
        if (root.map.mapReady) {
            // Place text in screen space; this changes labels only, never geographic data.
            const spacing = ScreenTools.defaultFontPixelHeight;
            const height = label.sourceItem.height;
            const width = label.sourceItem.width;
            if (!Number.isFinite(width) || !Number.isFinite(height) || !Number.isFinite(spacing) || spacing <= 0) {
                label.visible = false;
                return;
            }
            let rectangle = Qt.rect(point.x, point.y + slot * spacing, width, height);
            const overlaps = function (candidate) {
                return root._labelRects.some(function (other) {
                    return candidate.x < other.x + other.width + 2 && candidate.x + candidate.width + 2 > other.x && candidate.y < other.y + other.height + 2 && candidate.y + candidate.height + 2 > other.y;
                });
            };
            const maximumSteps = Math.ceil(root.map.height / spacing) + 2;
            let steps = 0;
            while (overlaps(rectangle) && steps++ < maximumSteps) {
                ++slot;
                rectangle.y = point.y + slot * spacing;
            }
            if (rectangle.y + height > root.map.height) {
                slot = -1;
                rectangle.y = point.y - spacing;
                steps = 0;
                while (overlaps(rectangle) && steps++ < maximumSteps) {
                    --slot;
                    rectangle.y = point.y + slot * spacing;
                }
            }
            if (rectangle.y < 0 || rectangle.y + height > root.map.height || overlaps(rectangle)) {
                label.visible = false;
                return;
            }
            root._labelRects.push(rectangle);
        }
        label.verticalOffset = slot;
    }

    function rebuild() {
        objects.destroyObjects();
        root._labelRects = [];
        if (!root.map || !root._missionItem) {
            return;
        }
        if (root._missionItem.coveragePolygon.path.length) {
            labelAt(root._missionItem.coveragePolygon.path[0], qsTr("C — Coverage Area"), false, 0);
        }
        if (root._missionItem.navigationPolygon.path.length) {
            labelAt(root._missionItem.navigationPolygon.path[0], qsTr("N — Navigation Area"), false, -1);
        }
        for (let i = 0; i < root._missionItem.noGoPolygons.count; ++i) {
            const polygon = root._missionItem.noGoPolygons.get(i);
            if (polygon.path.length) {
                labelAt(polygon.path[0], qsTr("No-Go %1").arg(i + 1), true, 1);
            }
        }
        addRuns(root._result.canonicalRuns || [], false);
        if (root._result.diagnosticCandidate) {
            addRuns(root._result.diagnosticCandidate.runs || [], true);
        }
        const quality = root._result.quality || ({});
        if (quality.boundaryShortfall && quality.boundaryShortfall.available) {
            addRegions(quality.boundaryShortfall.value, "BoundaryShortfall", qsTr("Boundary shortfall"));
        }
        if (quality.criticalUncovered && quality.criticalUncovered.available) {
            addRegions(quality.criticalUncovered.value, "CriticalUncovered", qsTr("Critical gap"));
        }
        const overlays = root._result.diagnosticOverlays || [];
        for (let i = 0; i < overlays.length; ++i) {
            addRegions(overlays[i].regions, "DiagnosticOverlay", qsTr("Diagnostic overlay — NOT A ROUTE") + "\n" + overlays[i].explanation);
        }
    }

    function translucent(color) {
        return Qt.rgba(color.r, color.g, color.b, 0.25 * root.opacity);
    }

    Component.onCompleted: rebuild()
    Component.onDestruction: objects.destroyObjects()
    onMapChanged: rebuild()

    QGCPalette {
        id: palette

        colorGroupEnabled: root.enabled
    }

    QGCDynamicObjectManager {
        id: objects
    }

    Connections {
        function onNoGoRegionsChanged() {
            root.rebuild();
        }

        function onPlanningResultChanged() {
            root.rebuild();
        }

        function onTaskDataChanged() {
            root.rebuild();
        }

        target: root._missionItem
    }

    Connections {
        function onCenterChanged() {
            root.rebuild();
        }

        function onMapReadyChanged() {
            root.rebuild();
        }

        function onZoomLevelChanged() {
            root.rebuild();
        }

        target: root.map
    }

    QGCMapPolygonVisuals {
        borderColor: palette.colorBlue
        borderWidth: 3
        interactive: root._currentItem && root.interactive && root._missionItem.editingRegion === 1
        interiorColor: palette.colorBlue
        interiorOpacity: 0.06 * root.opacity
        mapControl: root.map
        mapPolygon: root._missionItem.navigationPolygon
    }

    QGCMapPolygonVisuals {
        borderColor: palette.mapMissionTrajectory
        borderWidth: 1
        interactive: root._currentItem && root.interactive && root._missionItem.editingRegion === 0
        interiorColor: palette.mapMissionTrajectory
        interiorOpacity: 0.10 * root.opacity
        mapControl: root.map
        mapPolygon: root._missionItem.coveragePolygon
    }

    Instantiator {
        model: root._missionItem.noGoPolygons

        delegate: QGCMapPolygonVisuals {
            required property var object

            borderColor: palette.colorRed
            borderWidth: 2
            interactive: root._currentItem && root.interactive && object.interactive
            interiorColor: palette.colorRed
            interiorOpacity: 0.20 * root.opacity
            mapControl: root.map
            mapPolygon: object
            parent: root
        }
    }

    Component {
        id: routeComponent

        MapPolyline {
            property bool diagnostic: false
            property string role
            property string safetyClass

            line.color: diagnostic ? palette.colorRed : safetyClass === "HardSafeWarning" ? palette.colorOrange : role === "Coverage" ? palette.mapMissionTrajectory : palette.colorGrey
            line.width: diagnostic ? 1 : role === "Coverage" ? 4 : 2
            opacity: root.opacity
            visible: root._currentItem
            z: QGroundControl.zOrderWaypointLines + 2
        }
    }

    Component {
        id: regionComponent

        MapPolygon {
            property string regionKind

            border.color: regionKind === "BoundaryShortfall" ? palette.colorOrange : regionKind === "CriticalUncovered" ? palette.colorRed : palette.colorGrey
            border.width: regionKind === "BoundaryShortfall" ? 1 : 3
            color: root.translucent(regionKind === "BoundaryShortfall" ? palette.colorOrange : regionKind === "CriticalUncovered" ? palette.colorRed : palette.colorGrey)
            visible: root._currentItem
            z: QGroundControl.zOrderWaypointLines
        }
    }

    Component {
        id: labelComponent

        MapQuickItem {
            id: mapLabel

            property string labelText
            property int verticalOffset: 0
            property bool warning: false

            anchorPoint.y: -verticalOffset * ScreenTools.defaultFontPixelHeight
            visible: root._currentItem
            z: QGroundControl.zOrderWaypointLines + 3

            sourceItem: Rectangle {
                color: palette.window
                height: label.implicitHeight
                opacity: 0.9
                width: label.implicitWidth + ScreenTools.defaultFontPixelWidth

                QGCLabel {
                    id: label

                    color: mapLabel.warning ? palette.colorOrange : palette.text
                    text: mapLabel.labelText
                }
            }
        }
    }
}
