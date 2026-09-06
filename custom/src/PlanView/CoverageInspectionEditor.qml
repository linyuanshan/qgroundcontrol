import QGroundControl
import QGroundControl.Controls
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    readonly property real _fieldWidth: ScreenTools.defaultFontPixelWidth * 11
    readonly property real _margin: ScreenTools.defaultFontPixelWidth / 2
    readonly property bool _hasNoGoRegions: root.missionItem.noGoRegions.length > 0
    required property real availableWidth
    required property var missionItem

    color: qgcPal.windowShadeDark
    height: root.visible ? editorColumn.height + (root._margin * 2) : 0
    radius: _margin
    width: availableWidth

    QGCPalette {
        id: qgcPal

        colorGroupEnabled: root.enabled
    }

    ColumnLayout {
        id: editorColumn

        spacing: root._margin

        anchors {
            left: root.left
            margins: root._margin
            right: root.right
            top: root.top
        }

        QGCLabel {
            Layout.fillWidth: true
            font.bold: true
            text: qsTr("Coverage Inspection")
        }

        GridLayout {
            Layout.fillWidth: true
            columnSpacing: root._margin
            columns: 2
            rowSpacing: root._margin

            QGCLabel {
                text: qsTr("Task Name")
            }

            QGCTextField {
                id: taskNameField

                Layout.fillWidth: true
                text: root.missionItem.taskName

                onEditingFinished: root.missionItem.taskName = taskNameField.text
            }

            QGCLabel {
                text: qsTr("Task ID")
            }

            QGCLabel {
                Layout.fillWidth: true
                text: root.missionItem.taskId
                wrapMode: Text.WrapAnywhere
            }

            QGCLabel {
                text: qsTr("Planner")
            }

            QGCLabel {
                Layout.fillWidth: true
                text: root.missionItem.plannerId
                wrapMode: Text.WordWrap
            }

            QGCLabel {
                text: qsTr("Work Region")
            }

            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("%1 vertices — edit with Polygon Tools on the map").arg(root.missionItem.workRegionPolygon.count)
                wrapMode: Text.WordWrap
            }

            QGCLabel {
                text: qsTr("Coverage Width")
            }

            QGCTextField {
                id: swathWidthField

                Layout.preferredWidth: root._fieldWidth
                text: root.missionItem.swathWidthM.toString()

                validator: DoubleValidator {
                    bottom: 0
                }

                onEditingFinished: root.missionItem.swathWidthM = Number(swathWidthField.text)
            }

            QGCLabel {
                text: qsTr("Safety Margin")
            }

            QGCTextField {
                id: safetyMarginField

                Layout.preferredWidth: root._fieldWidth
                text: root.missionItem.safetyMarginM.toString()

                validator: DoubleValidator {
                    bottom: 0
                }

                onEditingFinished: root.missionItem.safetyMarginM = Number(safetyMarginField.text)
            }

            QGCLabel {
                text: qsTr("Planning State")
            }

            QGCLabel {
                text: root.missionItem.planningState === 1 ? qsTr("Planned") : qsTr("Unplanned")
            }

            QGCLabel {
                text: qsTr("Sweep Angle")
            }

            QGCComboBox {
                Layout.preferredWidth: root._fieldWidth
                model: [qsTr("Auto"), qsTr("Manual")]
                currentIndex: root.missionItem.automaticSweepAngle ? 0 : 1

                onActivated: (index) => { root.missionItem.automaticSweepAngle = index === 0 }
            }

            QGCLabel {
                text: qsTr("Manual Angle")
                visible: !root.missionItem.automaticSweepAngle
            }

            QGCTextField {
                id: sweepAngleField

                Layout.preferredWidth: root._fieldWidth
                text: root.missionItem.sweepAngleDeg.toString()
                visible: !root.missionItem.automaticSweepAngle

                validator: DoubleValidator {
                    bottom: 0
                    top: 179.999
                }

                onEditingFinished: root.missionItem.sweepAngleDeg = Number(sweepAngleField.text)
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columnSpacing: root._margin
            columns: 3
            rowSpacing: root._margin

            QGCLabel {
                text: qsTr("Camera")
            }

            QGCCheckBox {
                id: cameraEnabledCheck

                checked: root.missionItem.cameraEnabled
                text: qsTr("Enabled")

                onToggled: root.missionItem.cameraEnabled = cameraEnabledCheck.checked
            }

            QGCCheckBox {
                id: cameraRecordCheck

                checked: root.missionItem.cameraRecord
                enabled: root.missionItem.cameraEnabled
                text: qsTr("Record")

                onToggled: root.missionItem.cameraRecord = cameraRecordCheck.checked
            }

            QGCLabel {
                text: qsTr("Sonar")
            }

            QGCCheckBox {
                id: sonarEnabledCheck

                checked: root.missionItem.sonarEnabled
                text: qsTr("Enabled")

                onToggled: root.missionItem.sonarEnabled = sonarEnabledCheck.checked
            }

            QGCCheckBox {
                id: sonarRecordCheck

                checked: root.missionItem.sonarRecord
                enabled: root.missionItem.sonarEnabled
                text: qsTr("Record")

                onToggled: root.missionItem.sonarRecord = sonarRecordCheck.checked
            }
        }

        QGCLabel {
            Layout.fillWidth: true
            color: qgcPal.warningText
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("No-Go regions are read-only in P1. The LawnMower planner does not route around them and will reject generation.")
            visible: root._hasNoGoRegions
            wrapMode: Text.WordWrap
        }

        GridLayout {
            Layout.fillWidth: true
            columnSpacing: root._margin
            columns: 2
            rowSpacing: root._margin
            visible: root.missionItem.planningState === 1

            QGCLabel { text: qsTr("Selected Angle") }
            QGCLabel { text: qsTr("%1°").arg(root.missionItem.selectedSweepAngleDeg.toFixed(1)) }
            QGCLabel { text: qsTr("Path Length") }
            QGCLabel { text: qsTr("%1 m").arg(root.missionItem.complexDistance.toFixed(1)) }
            QGCLabel { text: qsTr("Turns") }
            QGCLabel { text: root.missionItem.turnCount.toString() }
        }

        QGCLabel {
            id: planningMessageLabel

            Layout.fillWidth: true
            text: root.missionItem.planningMessage
            visible: planningMessageLabel.text.length > 0
            wrapMode: Text.WordWrap
        }

        QGCButton {
            Layout.fillWidth: true
            enabled: root.missionItem.workRegionPolygon.isValid
            text: root.missionItem.planningState === 1 ? qsTr("Replan") : qsTr("Generate Plan")

            onClicked: root.missionItem.plan()
        }
    }
}
