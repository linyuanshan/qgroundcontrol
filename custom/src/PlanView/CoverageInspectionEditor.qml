pragma ComponentBehavior: Bound
import QGroundControl
import QGroundControl.Controls
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    readonly property real _margin: ScreenTools.defaultFontPixelWidth / 2
    readonly property var _metrics: _result.metrics || ({})
    readonly property var _quality: _result.quality || ({})
    readonly property var _repair: _result.repair || ({})
    readonly property var _result: missionItem.planningPresentation
    readonly property var _source: _result.plannerSource || ({})
    required property real availableWidth
    required property var missionItem

    function canonicalMeasurement(field, unit) {
        return (_result.canonicalRuns || []).length ? measurement(field, unit) : qsTr("Not applicable");
    }

    function displayState(value) {
        const labels = {
            "Unplanned": qsTr("Unplanned"),
            "Success": qsTr("Success"),
            "InvalidInput": qsTr("Invalid input"),
            "Failed": qsTr("Failed"),
            "None": qsTr("None"),
            "Ready": qsTr("Ready"),
            "ReadyWithWarning": qsTr("Ready with warning"),
            "ReviewRequired": qsTr("Review required"),
            "DiagnosticOnly": qsTr("Diagnostic only"),
            "Complete": qsTr("Complete"),
            "Acceptable": qsTr("Acceptable"),
            "Insufficient": qsTr("Insufficient"),
            "AssessmentError": qsTr("Assessment failed — unavailable"),
            "Auto": qsTr("Auto"),
            "Manual": qsTr("Manual")
        };
        return labels[value] || value || qsTr("Not applicable");
    }

    function measurement(field, unit) {
        return field && field.available ? Number(field.value).toString() + unit : qsTr("Unavailable");
    }

    function numericInput(value) {
        return value.trim().length ? Number(value) : NaN;
    }

    function numericText(value) {
        return Number.isFinite(value) ? value.toString() : "";
    }

    function referenceText(reference) {
        if (!reference || !reference.kind)
            return "";
        if (reference.kind === "CanonicalPathLegRange" || reference.kind === "DiagnosticCandidateLegRange")
            return reference.kind + " / " + reference.index + " / " + reference.firstLeg + "+" + reference.legCount;
        if (reference.kind === "CoverageResidual")
            return reference.kind + " / " + reference.residual;
        if (reference.kind === "DiagnosticOverlay")
            return reference.kind + " / " + reference.index;
        return reference.kind;
    }

    color: qgcPal.windowShadeDark
    height: visible ? editorColumn.height + _margin * 2 : 0
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
            left: parent.left
            margins: root._margin
            right: parent.right
            top: parent.top
        }

        QGCLabel {
            Layout.fillWidth: true
            font.bold: true
            text: qsTr("Coverage Inspection")
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2

            QGCLabel {
                text: qsTr("Task name")
            }

            QGCTextField {
                Layout.fillWidth: true
                text: root.missionItem.taskName

                onEditingFinished: root.missionItem.taskName = text
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
                text: qsTr("Requested planner")
            }

            QGCLabel {
                Layout.fillWidth: true
                text: root.missionItem.plannerId
                wrapMode: Text.WrapAnywhere
            }

            QGCLabel {
                text: qsTr("Edit boundary")
            }

            QGCComboBox {
                Layout.fillWidth: true
                currentIndex: root.missionItem.editingRegion
                model: [qsTr("C — Coverage Area"), qsTr("N — Navigation Area"), qsTr("O — No-Go")]
                objectName: "marine_editRegion"

                onActivated: index => root.missionItem.setEditingRegion(index)
            }

            QGCLabel {
                text: qsTr("C / N vertices")
            }

            QGCLabel {
                text: qsTr("%1 / %2").arg(root.missionItem.coveragePolygon.count).arg(root.missionItem.navigationPolygon.count)
            }

            QGCLabel {
                text: qsTr("Coverage width (m)")
            }

            QGCTextField {
                Layout.fillWidth: true
                objectName: "marine_swathWidth"
                text: root.numericText(root.missionItem.swathWidthM)

                onEditingFinished: root.missionItem.swathWidthM = root.numericInput(text)
                onTextEdited: root.missionItem.invalidatePlan()
            }

            QGCLabel {
                text: qsTr("Hard clearance H (m)")
            }

            QGCTextField {
                Layout.fillWidth: true
                objectName: "marine_hardClearance"
                placeholderText: qsTr("Explicit value required")
                text: root.numericText(root.missionItem.hardSafetyMarginM)

                onEditingFinished: root.missionItem.hardSafetyMarginM = root.numericInput(text)
                onTextEdited: root.missionItem.invalidatePlan()
            }

            QGCLabel {
                text: qsTr("Coverage requirement")
            }

            QGCComboBox {
                Layout.fillWidth: true
                currentIndex: root.missionItem.coverageRequirement === "Strict" ? 1 : 0
                model: [qsTr("Standard"), qsTr("Strict")]
                objectName: "marine_requirement"

                onActivated: index => root.missionItem.coverageRequirement = index === 1 ? "Strict" : "Standard"
            }

            QGCLabel {
                text: qsTr("Sweep angle mode")
            }

            QGCComboBox {
                Layout.fillWidth: true
                currentIndex: root.missionItem.automaticSweepAngle ? 0 : 1
                model: [qsTr("Auto"), qsTr("Manual")]
                objectName: "marine_sweepMode"

                onActivated: index => root.missionItem.automaticSweepAngle = index === 0
            }

            QGCLabel {
                text: qsTr("Manual bearing (0° N, 90° E)")
                visible: !root.missionItem.automaticSweepAngle
            }

            QGCTextField {
                Layout.fillWidth: true
                objectName: "marine_manualBearing"
                text: root.numericText(root.missionItem.sweepAngleDeg)
                visible: !root.missionItem.automaticSweepAngle

                onEditingFinished: root.missionItem.sweepAngleDeg = root.numericInput(text)
                onTextEdited: root.missionItem.invalidatePlan()
            }
        }

        QGCLabel {
            Layout.fillWidth: true
            text: qsTr("C defines required observation. N permits navigation. Edit them independently using Polygon Tools.")
            wrapMode: Text.WordWrap
        }

        QGCLabel {
            font.bold: true
            text: qsTr("Advanced safety")
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2

            QGCLabel {
                text: qsTr("Preferred clearance P (m)")
            }

            QGCTextField {
                Layout.fillWidth: true
                objectName: "marine_preferredClearance"
                placeholderText: qsTr("Explicit value required")
                text: root.numericText(root.missionItem.preferredSafetyMarginM)

                onEditingFinished: root.missionItem.preferredSafetyMarginM = root.numericInput(text)
                onTextEdited: root.missionItem.invalidatePlan()
            }

            QGCLabel {
                text: qsTr("Execution reserve E (m)")
            }

            QGCTextField {
                Layout.fillWidth: true
                objectName: "marine_executionReserve"
                placeholderText: qsTr("Explicit value required")
                text: root.numericText(root.missionItem.executionMarginM)

                onEditingFinished: root.missionItem.executionMarginM = root.numericInput(text)
                onTextEdited: root.missionItem.invalidatePlan()
            }
        }

        QGCLabel {
            Layout.fillWidth: true
            text: qsTr("P includes H and must be at least H. E is an additional mandatory execution reserve. No safety value is adjusted automatically.")
            wrapMode: Text.WordWrap
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 3

            QGCLabel {
                text: qsTr("Camera")
            }

            QGCCheckBox {
                checked: root.missionItem.cameraEnabled
                text: qsTr("Enabled")

                onToggled: root.missionItem.cameraEnabled = checked
            }

            QGCCheckBox {
                checked: root.missionItem.cameraRecord
                enabled: root.missionItem.cameraEnabled
                text: qsTr("Record")

                onToggled: root.missionItem.cameraRecord = checked
            }

            QGCLabel {
                text: qsTr("Sonar")
            }

            QGCCheckBox {
                checked: root.missionItem.sonarEnabled
                text: qsTr("Enabled")

                onToggled: root.missionItem.sonarEnabled = checked
            }

            QGCCheckBox {
                checked: root.missionItem.sonarRecord
                enabled: root.missionItem.sonarEnabled
                text: qsTr("Record")

                onToggled: root.missionItem.sonarRecord = checked
            }
        }

        QGCLabel {
            font.bold: true
            text: qsTr("O — No-Go regions (%1 / %2)").arg(root.missionItem.noGoPolygons.count).arg(root.missionItem.maximumNoGoRegionCount)
        }

        Repeater {
            model: root.missionItem.noGoPolygons

            delegate: RowLayout {
                required property int index
                required property var object

                Layout.fillWidth: true

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("No-Go %1").arg(parent.index + 1)
                }

                QGCCheckBox {
                    checked: parent.object.interactive
                    text: qsTr("Edit")

                    onClicked: root.missionItem.setNoGoRegionInteractive(parent.index, checked)
                }

                QGCButton {
                    text: qsTr("Delete")

                    onClicked: root.missionItem.deleteNoGoRegion(parent.index)
                }
            }
        }

        QGCButton {
            Layout.fillWidth: true
            enabled: root.missionItem.noGoPolygons.count < root.missionItem.maximumNoGoRegionCount
            text: qsTr("Add No-Go region")

            onClicked: root.missionItem.addNoGoRegion()
        }

        QGCLabel {
            Layout.fillWidth: true
            color: qgcPal.warningText
            text: qsTr("Complete each No-Go region with at least three vertices.")
            visible: !root.missionItem.noGoRegionsReady
            wrapMode: Text.WordWrap
        }

        QGCLabel {
            Layout.fillWidth: true
            color: qgcPal.colorOrange
            objectName: "marine_staleNotice"
            text: qsTr("Result out of date — generate a new plan.")
            visible: root.missionItem.resultStale
            wrapMode: Text.WordWrap
        }

        QGCLabel {
            Layout.fillWidth: true
            font.bold: true
            objectName: "marine_readiness"
            text: qsTr("%1 / %2 / %3").arg(root.displayState(root._result.readiness)).arg(root.displayState(root._result.status)).arg(root._result.tier || qsTr("No safety tier"))
            wrapMode: Text.WordWrap
        }

        QGCLabel {
            Layout.fillWidth: true
            objectName: "marine_ingressNotice"
            text: root._result.ingressMessage || ""
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: !!root._result.hasCurrentResult

            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("Resolved strategy: %1 (%2)\nResolution: %3 / %4\nSweep: %5, %6° (%7)\nRequested: %8; escalated: %9").arg(root._source.resolvedStrategyId || qsTr("Not applicable")).arg(root._source.strategyVersion || "").arg(root._source.resolutionStatus || "").arg(root._source.resolutionReason || "").arg(root.displayState(root._source.requestedSweepMode)).arg(root._source.selectedSweepAngleDeg === undefined ? qsTr("Unavailable") : Number(root._source.selectedSweepAngleDeg).toFixed(1)).arg(root._source.sweepVersion || "").arg(root._source.requestedPlannerId || qsTr("Not applicable")).arg(root._source.escalated ? qsTr("Yes") : qsTr("No"))
                wrapMode: Text.WordWrap
            }

            QGCLabel {
                Layout.fillWidth: true
                objectName: "marine_quality"
                text: qsTr("Coverage: %1 — %2\nAssessment error: %3\nPolicy: %4\nRatio: %5\nTarget / covered / uncovered: %6 / %7 / %8\nCritical uncovered: %9\nNumerical tolerance: %10").arg(root.displayState(root._quality.status)).arg(root._quality.requirement || qsTr("Not applicable")).arg(root._quality.error || qsTr("Not applicable")).arg(root._quality.policyVersion || "").arg(root.measurement(root._quality.coverageRatio, "")).arg(root.measurement(root._quality.targetAreaM2, " m²")).arg(root.measurement(root._quality.coveredAreaM2, " m²")).arg(root.measurement(root._quality.uncoveredAreaM2, " m²")).arg(root.measurement(root._quality.criticalUncoveredAreaM2, " m²")).arg(root.measurement(root._quality.numericalToleranceM2, " m²"))
                wrapMode: Text.WordWrap
            }

            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("Residual availability — boundary: %1; critical: %2\nStrict fallback: %3; whole target: %4").arg(root._quality.boundaryShortfall && root._quality.boundaryShortfall.available ? qsTr("Available") : qsTr("Unavailable")).arg(root._quality.criticalUncovered && root._quality.criticalUncovered.available ? qsTr("Available") : qsTr("Unavailable")).arg(root._quality.strictFallbackTriggered ? qsTr("Yes") : qsTr("No")).arg(root._quality.wholeTargetStrictFallback ? qsTr("Yes") : qsTr("No"))
                visible: !!root._result.quality
                wrapMode: Text.WordWrap
            }

            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("Canonical length / coverage / transit: %1 / %2 / %3\nTurns / cells: %4 / %5").arg(root.canonicalMeasurement(root._metrics.pathLengthM, " m")).arg(root.canonicalMeasurement(root._metrics.coverageLengthM, " m")).arg(root.canonicalMeasurement(root._metrics.transitLengthM, " m")).arg(root.canonicalMeasurement(root._metrics.turnCount, "")).arg(root.canonicalMeasurement(root._metrics.cellCount, ""))
                wrapMode: Text.WordWrap
            }

            QGCLabel {
                Layout.fillWidth: true
                objectName: "marine_repairSummary"
                text: qsTr("Repair attempted: %1; applied: %2\nReason: %3").arg(root._repair.attempted ? qsTr("Yes") : qsTr("No")).arg(root._repair.applied ? qsTr("Yes") : qsTr("No")).arg(root._repair.reason || "")
                wrapMode: Text.WordWrap
            }

            Repeater {
                model: root._repair.components || []

                delegate: QGCLabel {
                    required property var modelData

                    Layout.fillWidth: true
                    objectName: "marine_repairComponent_" + modelData.componentId
                    text: qsTr("Component %1; entry %2; reverse %3; transit cost %4 m\nUncovered before / after: %5 / %6\nLength before / after: %7 / %8 m; turns: %9 / %10").arg(modelData.componentId).arg(modelData.entryIndex).arg(modelData.reverse ? qsTr("Yes") : qsTr("No")).arg(Number(modelData.transitionCostM).toFixed(2)).arg(root.measurement(modelData.before.uncoveredAreaM2, " m²")).arg(root.measurement(modelData.after.uncoveredAreaM2, " m²")).arg(modelData.pathLengthBeforeM).arg(modelData.pathLengthAfterM).arg(modelData.turnCountBefore).arg(modelData.turnCountAfter)
                    wrapMode: Text.WordWrap
                }
            }

            QGCLabel {
                font.bold: true
                text: qsTr("Issues")
            }

            Repeater {
                model: root._result.issues || []

                delegate: QGCLabel {
                    required property var modelData

                    Layout.fillWidth: true
                    objectName: "marine_issue_" + modelData.code
                    text: modelData.code + " [" + modelData.severity + "] — " + modelData.message + (modelData.sourceMessage ? "\n" + modelData.sourceMessage : "") + (modelData.reference.kind ? "\n" + root.referenceText(modelData.reference) : "")
                    wrapMode: Text.WordWrap
                }
            }

            QGCLabel {
                font.bold: true
                text: qsTr("Suggestions — review manually")
            }

            Repeater {
                model: root._result.suggestions || []

                delegate: QGCLabel {
                    required property var modelData

                    Layout.fillWidth: true
                    text: modelData.code + " [" + modelData.issue + "] — " + modelData.message + (modelData.sourceMessage ? "\n" + modelData.sourceMessage : "") + (modelData.reference.kind ? "\n" + root.referenceText(modelData.reference) : "")
                    wrapMode: Text.WordWrap
                }
            }
        }

        QGCLabel {
            Layout.fillWidth: true
            text: root.missionItem.planningMessage
            visible: text.length > 0
            wrapMode: Text.WordWrap
        }

        QGCButton {
            Layout.fillWidth: true
            enabled: root.missionItem.coveragePolygon.isValid && root.missionItem.navigationPolygon.isValid && root.missionItem.noGoRegionsReady
            objectName: "marine_generatePlan"
            text: qsTr("Generate plan")

            onClicked: root.missionItem.plan()
        }
    }
}
