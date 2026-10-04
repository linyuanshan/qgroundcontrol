#include "CoveragePlanViewUITest.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QScopeGuard>
#include <QtCore/QSet>
#include <QtCore/QTemporaryDir>
#include <QtGraphs/QXYSeries>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlIncubationController>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtTest/QSignalSpy>

#include "ComplexMissionItem.h"
#include "CoverageInspectionComplexItem.h"
#include "CoverageStrategySemantics.h"
#include "FlightPathSegment.h"
#include "GeoFenceManager.h"
#include "Geometry/GeoReference.h"
#include "LogManager.h"
#include "MarinePlanContext.h"
#include "MissionController.h"
#include "MissionManager.h"
#include "MockLink.h"
#include "PlanMasterController.h"
#include "QGCApplication.h"
#include "QGroundControlQmlGlobal.h"
#include "RallyPointManager.h"
#include "Vehicle.h"

using namespace Marine;

void CoveragePlanViewUITest::_testOrdinaryDialogLifecycle()
{
    startUI();
    if (QTest::currentTestFailed()) {
        return;
    }
    const auto cleanup = qScopeGuard([this] { stopUI(); });
    auto* global = _engine->singletonInstance<QGroundControlQmlGlobal*>("QGroundControl", "QGroundControl");
    QVERIFY(global);
    QQuickItem owner(_rootItem);
    const auto callback = _engine->evaluate("(function () {})");
    QVERIFY(callback.isCallable());
    global->showMessageDialog(&owner, "Ordinary dialog baseline", "No Marine item or Task is created", 1024, callback);
    QPointer<QObject> dialog;
    for (auto* object : owner.findChildren<QObject*>()) {
        if (object->metaObject()->indexOfProperty("acceptFunction") >= 0 &&
            object->property("acceptFunction").value<QJSValue>().strictlyEquals(callback)) {
            dialog = object;
            break;
        }
    }
    QVERIFY(dialog);
    QVERIFY(waitForCondition([dialog] { return dialog && dialog->property("opened").toBool(); }, 5000,
                             QStringLiteral("ordinary dialog finishes opening")));
    QVERIFY(QMetaObject::invokeMethod(dialog, "_accept"));
    QVERIFY(waitForCondition([dialog] { return !dialog; }, 5000, QStringLiteral("ordinary dialog is destroyed")));
    expectAppMessage(QRegularExpression("Ordinary application dialog baseline"));
    qgcApp()->showAppMessage("Ordinary application dialog baseline");
    verifyExpectedLogMessage();
    for (auto* object : _engine->rootObjects().front()->findChildren<QObject*>()) {
        if (object->metaObject()->indexOfProperty("acceptFunction") >= 0 && object->property("visible").toBool()) {
            dialog = object;
            break;
        }
    }
    QVERIFY(dialog);
    QVERIFY(waitForCondition([dialog] { return dialog && dialog->property("opened").toBool(); }, 5000,
                             QStringLiteral("ordinary application dialog finishes opening")));
    QVERIFY(QMetaObject::invokeMethod(dialog, "_accept"));
    QVERIFY(waitForCondition([dialog] { return !dialog; }, 5000,
                             QStringLiteral("ordinary application dialog is destroyed")));
}

void CoveragePlanViewUITest::_testActualUploadAndConfirmation_data()
{
    QTest::addColumn<bool>("review");
    QTest::addColumn<bool>("ordinary");
    QTest::newRow("ordinary-firmware-confirmation") << false << true;
    QTest::newRow("stale-after-firmware-confirmation") << false << false;
    QTest::newRow("ReviewRequired-after-firmware-confirmation") << true << false;
}

void CoveragePlanViewUITest::_testActualUploadAndConfirmation()
{
    QFETCH(bool, review);
    QFETCH(bool, ordinary);
    // MockLink accepts SET_MESSAGE_INTERVAL only for its six PID tuning message IDs.
    // The real window's initial stream requests exhaust retries for these two IDs.
    for (const auto* message : {"EXTENDED_SYS_STATE", "HOME_POSITION"}) {
        expectLogMessage("Vehicle.MavCommandQueue", QtWarningMsg,
                         QRegularExpression(QString("^\"?Giving up sending command after max retries: "
                                                    "MAV_CMD_SET_MESSAGE_INTERVAL message: %1\"?$")
                                                .arg(message)));
    }
    runWithMockLink(
        [] { return MockLink::startAPMArduRoverMockLink(); },
        [this, review, ordinary](QPointer<MockLink>, Vehicle* vehicle) {
            const auto waitForVisuals = [this] {
                const bool complete = waitForCondition(
                    [this] {
                        _window->update();
                        auto* controller = _engine->incubationController();
                        if (controller) {
                            controller->incubateFor(5);
                            if (controller->incubatingObjectCount() != 0) {
                                return false;
                            }
                        }
                        QList<QObject*> pending{_engine, _window};
                        QSet<QObject*> visited;
                        while (!pending.isEmpty()) {
                            auto* object = pending.takeLast();
                            if (!object || visited.contains(object)) {
                                continue;
                            }
                            visited.insert(object);
                            pending.append(object->children());
                            if (auto* visual = qobject_cast<QQuickItem*>(object)) {
                                for (auto* child : visual->childItems()) {
                                    pending.append(child);
                                }
                            }
                            if (QString::fromLatin1(object->metaObject()->className()).contains("Loader") &&
                                object->property("status").toInt() == 2) {
                                return false;
                            }
                        }
                        return true;
                    },
                    TestTimeout::mediumMs(), QStringLiteral("actual map visual loading and incubation complete"));
                return complete;
            };
            QVERIFY(waitForCondition(
                [] {
                    const auto entries = LogManager::capturedMessages("Vehicle.MavCommandQueue");
                    int count = 0;
                    for (const auto& entry : entries) {
                        if (entry.level == LogEntry::Warning &&
                            (entry.message.contains("message: EXTENDED_SYS_STATE") ||
                             entry.message.contains("message: HOME_POSITION"))) {
                            ++count;
                        }
                    }
                    return count == 2;
                },
                10000, QStringLiteral("MockLink's two unsupported initial message intervals complete")));
            verifyExpectedLogMessage();
            verifyExpectedLogMessage();
            const auto closeDialogs = [this, &waitForVisuals] {
                QList<QPointer<QObject>> dialogs;
                for (auto* object : _engine->rootObjects().front()->findChildren<QObject*>()) {
                    if (object->metaObject()->indexOfProperty("acceptFunction") >= 0 &&
                        object->property("visible").toBool()) {
                        dialogs.append(object);
                    }
                }
                for (const auto& dialog : dialogs) {
                    if (!waitForCondition([dialog] { return dialog && dialog->property("opened").toBool(); }, 5000,
                                          QStringLiteral("actual message dialog finishes opening")) ||
                        !QMetaObject::invokeMethod(dialog, "_accept") ||
                        !waitForCondition([dialog] { return !dialog; }, 5000,
                                          QStringLiteral("actual message dialog is destroyed"))) {
                        return false;
                    }
                }
                const bool complete = waitForVisuals();
                return complete;
            };
            QVERIFY(clickToolSelectDropdownButton("toolbar_viewPlan"));
            auto* view = findVisibleItem(_rootItem, "mainView_plan", 5000);
            QVERIFY(view);
            auto* controller =
                qobject_cast<PlanMasterController*>(view->property("_planMasterController").value<QObject*>());
            QVERIFY(controller);
            auto* mission = controller->missionController();
            const auto validHeightProfile = [this, mission] {
                auto* chart = findVisibleItem(_rootItem, "terrainStatusChart", TestTimeout::mediumMs());
                auto* profile = findItem(_rootItem, "terrainStatusProfile");
                if (!chart || !profile || chart->width() <= 0 || chart->height() <= 0 ||
                    !qIsFinite(profile->property("pixelsPerMeter").toDouble())) {
                    return false;
                }
                QJsonObject trace;
                for (const auto* name : {"axisX", "axisY"}) {
                    auto* axis = chart->property(name).value<QObject*>();
                    if (!axis) {
                        return false;
                    }
                    const double minimum = axis->property("min").toDouble();
                    const double maximum = axis->property("max").toDouble();
                    if (!qIsFinite(minimum) || !qIsFinite(maximum) || maximum <= minimum) {
                        return false;
                    }
                    trace.insert(name, QJsonArray{minimum, maximum});
                }
                const auto validSegment = [](FlightPathSegment* segment) {
                    if (!segment->coordinate1().isValid() || !segment->coordinate2().isValid() ||
                        !qIsFinite(segment->totalDistance()) || segment->totalDistance() < 0 ||
                        qIsInf(segment->coord1AMSLAlt()) || qIsInf(segment->coord2AMSLAlt()) ||
                        !qIsFinite(segment->distanceBetween()) || !qIsFinite(segment->finalDistanceBetween())) {
                        return false;
                    }
                    for (const auto& height : segment->amslTerrainHeights()) {
                        if (!qIsFinite(height.toDouble())) {
                            return false;
                        }
                    }
                    return true;
                };
                for (int index = 0; index < mission->visualItems()->count(); ++index) {
                    auto* visual = mission->visualItems()->value<VisualMissionItem*>(index);
                    if (!qIsFinite(visual->distanceFromStart()) ||
                        (visual->simpleFlightPathSegment() && !validSegment(visual->simpleFlightPathSegment()))) {
                        return false;
                    }
                    if (auto* complex = qobject_cast<ComplexMissionItem*>(visual)) {
                        if (!qIsFinite(complex->complexDistance())) {
                            return false;
                        }
                        for (int segmentIndex = 0; segmentIndex < complex->flightPathSegments()->count();
                             ++segmentIndex) {
                            if (!validSegment(complex->flightPathSegments()->value<FlightPathSegment*>(segmentIndex))) {
                                return false;
                            }
                        }
                    }
                }
                trace.insert("distance", mission->missionTotalDistance());
                trace.insert("minAltitude", mission->minAMSLAltitude());
                trace.insert("maxAltitude", mission->maxAMSLAltitude());
                QJsonArray series;
                for (auto* curve : chart->findChildren<QXYSeries*>()) {
                    QJsonArray points;
                    for (const auto& point : curve->points()) {
                        // NaN denotes an unavailable interval; infinity is never a valid sample or separator.
                        if (qIsInf(point.x()) || qIsInf(point.y())) {
                            return false;
                        }
                        points.append(QJsonArray{point.x(), point.y()});
                    }
                    series.append(points);
                }
                trace.insert("series", series);
                const auto filename = qEnvironmentVariable("V05_09_PROFILE_TRACE");
                if (!filename.isEmpty()) {
                    QFile output(filename);
                    if (!output.open(QIODevice::WriteOnly | QIODevice::Append)) {
                        return false;
                    }
                    trace.insert("case", QString::fromLatin1(QTest::currentDataTag()));
                    output.write(QJsonDocument(trace).toJson(QJsonDocument::Compact) + '\n');
                }
                return true;
            };
            QVERIFY(waitForVisuals());
            QVERIFY(waitForCondition(validHeightProfile, TestTimeout::mediumMs(),
                                     QStringLiteral("height profile settles")));
            controller->removeAll();
            QVERIFY(waitForVisuals());
            CoverageInspectionComplexItem* item = nullptr;
            if (ordinary) {
                QVERIFY(mission->insertSimpleMissionItem(QGeoCoordinate(38, 121), 1, true));
            } else {
                item = qobject_cast<CoverageInspectionComplexItem*>(mission->insertComplexMissionItem(
                    CoverageInspectionComplexItem::canonicalName, QGeoCoordinate(38, 121), -1, true));
                QVERIFY(item);
                auto* context = controller->findChild<MarinePlanContext*>(QString(), Qt::FindDirectChildrenOnly);
                QVERIFY(context);
                MarineTask task;
                task.planner.plannerId = CoverageStrategySemantics::AutoPlannerId;
                task.safety = {0, 0};
                task.planner.executionSafety.executionMarginM = 0;
                task.coverage = {5, CoverageRequirement::Strict, SweepAngleMode::Manual, 90};
                const auto reference = GeoReference::create(GeoPoint{38, 121, 0});
                QVERIFY(reference);
                for (const Point2D point : {Point2D{0, 0}, Point2D{20, 0}, Point2D{20, 20}, Point2D{0, 20}}) {
                    task.region.coverageBoundary.vertices.push_back(*reference->toGeo(point));
                }
                task.region.navigationBoundary = task.region.coverageBoundary;
                context->addTask(task);
                item->setTaskId(QString::fromStdString(task.id));
                QVERIFY(item->plan());
            }
            QVERIFY(mission->uploadAllowed());
            QVERIFY(waitForVisuals());
            QVERIFY(waitForCondition(validHeightProfile, TestTimeout::mediumMs(),
                                     QStringLiteral("height profile settles")));
            QTRY_COMPARE(mission->minAMSLAltitude(), ordinary ? 50.0 : 0.0);
            QTRY_COMPARE(mission->maxAMSLAltitude(), ordinary ? 50.0 : 0.0);
            if (ordinary) {
                QCOMPARE(mission->missionTotalDistance(), 0.0);
            }

            // Loaded plans intentionally stop tracking the offline setting. Load a real supported
            // plan with different firmware metadata to reach the product confirmation branch.
            QTemporaryDir directory(QDir::currentPath() + "/build/v05-09-ui-XXXXXX");
            QVERIFY(directory.isValid());
            const auto filename = directory.filePath("mismatch.plan");
            QVERIFY(controller->saveToFile(filename));
            QFile file(filename);
            QVERIFY(file.open(QIODevice::ReadOnly));
            auto saved = QJsonDocument::fromJson(file.readAll()).object();
            file.close();
            auto savedMission = saved.value("mission").toObject();
            savedMission.insert("firmwareType", MAV_AUTOPILOT_PX4);
            saved.insert("mission", savedMission);
            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
            const auto bytes = QJsonDocument(saved).toJson();
            QCOMPARE(file.write(bytes), bytes.size());
            file.close();
            controller->loadFromFile(filename);
            QVERIFY(waitForVisuals());
            QVERIFY(waitForCondition(validHeightProfile, TestTimeout::mediumMs(),
                                     QStringLiteral("height profile settles")));
            item = qobject_cast<CoverageInspectionComplexItem*>(mission->visualItems()->value<VisualMissionItem*>(1));
            QCOMPARE(item != nullptr, !ordinary);
            QVERIFY(mission->uploadAllowed());
            QTRY_COMPARE(mission->sendToVehiclePreCheck(),
                         MissionController::SendToVehiclePreCheckStateFirwmareVehicleMismatch);

            auto* global = _engine->singletonInstance<QGroundControlQmlGlobal*>("QGroundControl", "QGroundControl");
            QVERIFY(global);
            QSignalSpy dialogs(global, &QGroundControlQmlGlobal::showMessageDialogRequested);
            QSignalSpy missionActivity(vehicle->missionManager(), &PlanManager::inProgressChanged);
            QSignalSpy missionProgress(vehicle->missionManager(), &PlanManager::progressPctChanged);
            QSignalSpy fenceActivity(vehicle->geoFenceManager(), &PlanManager::inProgressChanged);
            QSignalSpy rallyActivity(vehicle->rallyPointManager(), &PlanManager::inProgressChanged);
            QVERIFY(QMetaObject::invokeMethod(controller, "upload"));
            QTRY_COMPARE(dialogs.count(), 1);
            auto callback = dialogs.front().at(4).value<QJSValue>();
            QVERIFY(callback.isCallable());
            QPointer<QObject> confirmation;
            for (auto* object : _engine->rootObjects().front()->findChildren<QObject*>()) {
                if (object->metaObject()->indexOfProperty("acceptFunction") >= 0 &&
                    object->property("acceptFunction").value<QJSValue>().strictlyEquals(callback)) {
                    confirmation = object;
                    break;
                }
            }
            QVERIFY(confirmation);
            QVERIFY(
                waitForCondition([confirmation] { return confirmation && confirmation->property("opened").toBool(); },
                                 5000, QStringLiteral("actual firmware confirmation finishes opening")));
            QCOMPARE(missionActivity.count(), 0);
            QVERIFY(waitForVisuals());

            if (ordinary) {
                QVERIFY(QMetaObject::invokeMethod(confirmation, "_accept"));
                QTRY_VERIFY(missionActivity.count() >= 2);
                QTRY_VERIFY(!controller->syncInProgress());
                QVERIFY(waitForCondition([confirmation] { return !confirmation; }, 5000,
                                         QStringLiteral("ordinary firmware confirmation is destroyed")));
                QVERIFY(closeDialogs());
                QVERIFY(mission->uploadAllowed());
                QVERIFY(!controller->dirtyForSave());
                return;
            }

            if (review) {
                item->setPreferredSafetyMarginM(2);
                item->setSafetyMarginM(2);
                QVERIFY(item->plan());
                QCOMPARE(item->planningResult().outcome.readiness, MissionReadiness::ReviewRequired);
                QVERIFY(!item->generatedPath().isEmpty());
                QCOMPARE(item->readyForSaveState(), VisualMissionItem::ReadyForSave);
            } else {
                item->setSwathWidthM(6);
                QVERIFY(item->resultStale());
            }
            QVERIFY(!mission->uploadAllowed());
            QVERIFY(waitForVisuals());
            QVERIFY(waitForCondition(validHeightProfile, TestTimeout::mediumMs(),
                                     QStringLiteral("height profile settles")));
            auto* button = findItem(_rootItem, "planToolbar_uploadButton");
            QVERIFY(button);
            QTRY_VERIFY(!button->property("enabled").toBool());
            const bool saveDirty = controller->dirtyForSave();
            const bool uploadDirty = controller->dirtyForUpload();
            const int count = mission->visualItems()->count();
            QList<int> sequences;
            for (int i = 0; i < count; ++i) {
                sequences.append(mission->visualItems()->value<VisualMissionItem*>(i)->sequenceNumber());
            }
            expectAppMessage(QRegularExpression(QRegularExpression::escape(mission->uploadBlockingReason())));
            // The actual popup executes exactly the captured product callback and closes.
            QVERIFY(QMetaObject::invokeMethod(confirmation, "_accept"));
            verifyExpectedLogMessage();
            QVERIFY(waitForCondition([confirmation] { return !confirmation; }, 5000,
                                     QStringLiteral("actual firmware confirmation is destroyed")));
            QVERIFY(closeDialogs());
            QVERIFY(!controller->syncInProgress());
            QCOMPARE(controller->dirtyForSave(), saveDirty);
            QCOMPARE(controller->dirtyForUpload(), uploadDirty);
            QCOMPARE(mission->visualItems()->count(), count);
            for (int i = 0; i < count; ++i) {
                QCOMPARE(mission->visualItems()->value<VisualMissionItem*>(i)->sequenceNumber(), sequences[i]);
            }

            // Invoke the actual product function and toolbar after forcing presentation enabled.
            QVERIFY(QMetaObject::invokeMethod(controller, "upload"));
            QTRY_COMPARE(dialogs.count(), 2);
            QCOMPARE(dialogs.back().at(2).toString(), mission->uploadBlockingReason());
            QVERIFY(closeDialogs());
            button->setProperty("enabled", true);
            auto* toolbar = button->parentItem();
            QVERIFY(toolbar);
            QVERIFY(QMetaObject::invokeMethod(toolbar, "_uploadClicked"));
            QTRY_COMPARE(dialogs.count(), 3);
            QCOMPARE(dialogs.back().at(2).toString(), mission->uploadBlockingReason());
            QVERIFY(closeDialogs());
            QCOMPARE(missionActivity.count(), 0);
            QCOMPARE(missionProgress.count(), 0);
            QCOMPARE(fenceActivity.count(), 0);
            QCOMPARE(rallyActivity.count(), 0);
            QVERIFY(!mission->uploadAllowed());
            QVERIFY(!controller->syncInProgress());
            // Keep the saved Review path and close the real window without leaving an
            // unsaved-mission confirmation to be created during context destruction.
            const auto retainedPath = item->generatedPath();
            QVERIFY(controller->saveToFile(directory.filePath("after-rejection.plan")));
            QVERIFY(!controller->dirtyForSave());
            QCOMPARE(item->generatedPath(), retainedPath);
        });
}

UT_REGISTER_TEST(CoveragePlanViewUITest, TestLabel::Integration, TestLabel::MissionManager)
