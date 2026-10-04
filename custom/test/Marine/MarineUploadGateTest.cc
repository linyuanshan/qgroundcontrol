#include "MarineUploadGateTest.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QPointer>
#include <QtCore/QRegularExpression>
#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>

#include "CoverageInspectionComplexItem.h"
#include "CoverageStrategySemantics.h"
#include "GeoFenceManager.h"
#include "Geometry/GeoReference.h"
#include "MarinePlanContext.h"
#include "MissionController.h"
#include "MissionManager.h"
#include "PlanMasterController.h"
#include "QmlObjectListModel.h"
#include "RallyPointManager.h"
#include "SimpleMissionItem.h"
#include "Vehicle.h"

using namespace Marine;

namespace {

CoverageInspectionComplexItem* coverageItem(PlanMasterController& controller)
{
    auto* items = controller.missionController()->visualItems();
    for (int i = 0; i < items->count(); ++i) {
        if (auto* item = items->value<CoverageInspectionComplexItem*>(i)) {
            return item;
        }
    }
    return nullptr;
}

// Every outcome is produced by the current planner. No readiness is forged in fixtures.
QJsonObject planFixture(int kind)
{
    PlanMasterController controller(MAV_AUTOPILOT_ARDUPILOTMEGA, MAV_TYPE_GROUND_ROVER);
    controller.setFlyView(false);
    controller.start();
    controller.missionController()->removeAll();
    if (kind == -4) {
        return controller.saveToJson().object();
    }
    controller.missionController()->insertSimpleMissionItem(QGeoCoordinate(38, 121), 1, false);
    if (kind == -3) {
        return controller.saveToJson().object();
    }
    auto* item = qobject_cast<CoverageInspectionComplexItem*>(controller.missionController()->insertComplexMissionItem(
        QString::fromUtf8(CoverageInspectionComplexItem::canonicalName), QGeoCoordinate(38, 121), -1, false));
    auto* context = controller.findChild<MarinePlanContext*>(QString(), Qt::FindDirectChildrenOnly);
    if (!item || !context) {
        return {};
    }
    MarineTask task;
    task.id = "upload-gate-fixture";
    task.coverage.swathWidthM = kind == 2 ? 1e-12 : kind == 3 ? 1e150 : 5;
    task.coverage.coverageRequirement = CoverageRequirement::Strict;
    task.coverage.sweepAngleMode = SweepAngleMode::Manual;
    task.coverage.sweepAngleDeg = 90;
    task.safety = {kind == 0 ? 2.0 : 0.0, kind == 0 ? 2.0 : kind == -2 ? 1.0 : 0.0};
    task.planner.plannerId = CoverageStrategySemantics::AutoPlannerId;
    task.planner.executionSafety.executionMarginM = kind == 1 ? 30 : 0;
    const auto reference = GeoReference::create(GeoPoint{38, 121, 0});
    const auto rectangle = [&](double low, double high) {
        GeoPolygon polygon;
        for (const Point2D point : {Point2D{low, low}, {high, low}, {high, high}, {low, high}}) {
            polygon.vertices.push_back(*reference->toGeo(point));
        }
        return polygon;
    };
    task.region.coverageBoundary = rectangle(0, 20);
    task.region.navigationBoundary = kind == 1 ? rectangle(-5, 25) : task.region.coverageBoundary;
    context->addTask(task);
    item->setTaskId(QString::fromStdString(task.id));
    if (kind != 5) {
        item->plan();
    }
    QJsonObject result = controller.saveToJson().object();
    if (kind == 4) {
        // Keep a valid artifact but change its serialized Task input: the loader must mark it stale.
        QJsonObject marine = result.value("marine").toObject();
        QJsonArray tasks = marine.value("tasks").toArray();
        QJsonObject changed = tasks.at(0).toObject();
        QJsonObject coverage = changed.value("coverage").toObject();
        coverage.insert("swathWidthM", 6);
        changed.insert("coverage", coverage);
        tasks[0] = changed;
        marine.insert("tasks", tasks);
        result.insert("marine", marine);
    }
    return result;
}

bool writePlan(const QString& path, const QJsonObject& json)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(json).toJson()) > 0;
}

QList<int> sequences(QmlObjectListModel* items)
{
    QList<int> result;
    for (int i = 0; i < items->count(); ++i) {
        result.append(items->value<VisualMissionItem*>(i)->sequenceNumber());
    }
    return result;
}

class RefusingSimpleItem : public SimpleMissionItem
{
public:
    explicit RefusingSimpleItem(PlanMasterController* controller) : SimpleMissionItem(controller, false, true) {}

    bool appendMissionItemsForUpload(QList<MissionItem*>& items, QObject* parent, QString& reason) override
    {
        allocated = new MissionItem(parent);
        owner = parent;
        items.append(allocated);
        reason = QStringLiteral("Intentional conversion refusal");
        return false;
    }

    QPointer<MissionItem> allocated;
    QPointer<QObject> owner;
};

}  // namespace

void MarineUploadGateTest::_blockedResults_data()
{
    QTest::addColumn<int>("kind");
    QTest::newRow("ReviewRequired") << 0;
    QTest::newRow("D2") << 1;
    QTest::newRow("D3") << 2;
    QTest::newRow("AssessmentError") << 3;
    QTest::newRow("stale") << 4;
    QTest::newRow("InfrastructureOnly-unplanned") << 5;
}

void MarineUploadGateTest::_blockedResults()
{
    QFETCH(int, kind);
    QTemporaryDir directory(QDir::currentPath() + "/build/v05-09-upload-XXXXXX");
    QVERIFY(directory.isValid());
    const QString path = directory.filePath("blocked.plan");
    const auto fixture = planFixture(kind);
    QVERIFY(!fixture.isEmpty());
    QVERIFY(writePlan(path, fixture));
    PlanMasterController controller;
    controller.startStaticActiveVehicle(vehicle());
    controller.loadFromFile(path);
    auto* item = coverageItem(controller);
    QVERIFY(item);
    QVERIFY(item->planningArtifact());
    const auto& artifact = *item->planningArtifact();
    if (kind == 0) {
        QCOMPARE(artifact.result.outcome.readiness, MissionReadiness::ReviewRequired);
        QCOMPARE(artifact.result.status, PlanningStatus::Success);
        QVERIFY(!artifact.result.path.empty());
    } else if (kind == 1 || kind == 2) {
        QCOMPARE(artifact.result.outcome.tier,
                 std::optional<SafetySolutionTier>(kind == 1 ? SafetySolutionTier::D2 : SafetySolutionTier::D3));
        QCOMPARE(artifact.result.outcome.readiness, MissionReadiness::DiagnosticOnly);
        QVERIFY(artifact.result.path.empty());
    } else if (kind == 3) {
        QVERIFY(artifact.result.outcome.coverageQuality);
        QCOMPARE(artifact.result.outcome.coverageQuality->status, CoverageQualityStatus::AssessmentError);
        QCOMPARE(artifact.result.outcome.readiness, MissionReadiness::ReviewRequired);
    } else if (kind == 4) {
        QVERIFY(artifact.stale);
        QCOMPARE(item->planningResult().status, PlanningStatus::Failed);
        QVERIFY(item->planningResult().path.empty());
    } else {
        QCOMPARE(artifact.resultContract, PlanningResultContract::InfrastructureOnly);
    }
    auto* mission = controller.missionController();
    QVERIFY(!mission->uploadAllowed());
    QVERIFY(!mission->uploadBlockingReason().isEmpty());
    const auto beforeSequences = sequences(mission->visualItems());
    const bool saveDirty = controller.dirtyForSave();
    const bool uploadDirty = controller.dirtyForUpload();
    const bool missionDirty = mission->dirty();
    QSignalSpy missionProgress(vehicle()->missionManager(), &PlanManager::progressPctChanged);
    QSignalSpy missionActivity(vehicle()->missionManager(), &PlanManager::inProgressChanged);
    QSignalSpy fenceActivity(vehicle()->geoFenceManager(), &PlanManager::inProgressChanged);
    QSignalSpy rallyActivity(vehicle()->rallyPointManager(), &PlanManager::inProgressChanged);
    QString error;
    QVERIFY(!mission->sendToVehicleChecked(error));
    QVERIFY(!error.isEmpty());
    expectAppMessage(QRegularExpression(QRegularExpression::escape(mission->uploadBlockingReason())));
    mission->sendToVehicle();
    verifyExpectedLogMessage();
    expectAppMessage(QRegularExpression(QRegularExpression::escape(mission->uploadBlockingReason())));
    controller.sendToVehicle();
    verifyExpectedLogMessage();
    QVERIFY(!MissionController::sendItemsToVehicle(vehicle(), mission->visualItems(), &error));
    expectAppMessage(QRegularExpression(QRegularExpression::escape(mission->uploadBlockingReason())));
    PlanMasterController::sendPlanToVehicle(vehicle(), path);
    verifyExpectedLogMessage();
    const auto transients = vehicle()->findChildren<PlanMasterController*>(QString(), Qt::FindDirectChildrenOnly);
    QCOMPARE(transients.size(), 1);
    QPointer<PlanMasterController> transient = transients.front();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(transient.isNull());
    QCOMPARE(missionProgress.count(), 0);
    QCOMPARE(missionActivity.count(), 0);
    QCOMPARE(fenceActivity.count(), 0);
    QCOMPARE(rallyActivity.count(), 0);
    QCOMPARE(sequences(mission->visualItems()), beforeSequences);
    QCOMPARE(controller.dirtyForSave(), saveDirty);
    QCOMPARE(controller.dirtyForUpload(), uploadDirty);
    QCOMPARE(mission->dirty(), missionDirty);
    QVERIFY(!controller.syncInProgress());
    // A completion is an external manager event (fly-view may refresh the visual list).
    // Its separate oracle is that a refused master sequence cannot start fence/rally sends.
    emit vehicle() -> missionManager()->sendComplete(false);
    QCOMPARE(fenceActivity.count(), 0);
    QCOMPARE(rallyActivity.count(), 0);
}

void MarineUploadGateTest::_failedFileLoad_data()
{
    QTest::addColumn<QString>("defect");
    for (const auto* name :
         {"empty", "missing", "text", "json", "structure", "task", "artifact", "mission", "fence", "rally"}) {
        QTest::newRow(name) << QString::fromUtf8(name);
    }
}

void MarineUploadGateTest::_failedFileLoad()
{
    QFETCH(QString, defect);
    QTemporaryDir directory(QDir::currentPath() + "/build/v05-09-upload-XXXXXX");
    QVERIFY(directory.isValid());
    QString path = directory.filePath("invalid.plan");
    auto fixture = planFixture(-1);
    QVERIFY(!fixture.isEmpty());
    if (defect == "empty") {
        path.clear();
    } else if (defect == "text") {
        path = directory.filePath("invalid.txt");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("Invalid waypoint file\n");
    } else if (defect == "json") {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{");
    } else if (defect == "structure") {
        fixture.remove("mission");
        QVERIFY(writePlan(path, fixture));
    } else if (defect == "mission" || defect == "fence" || defect == "rally") {
        const QString section = defect == "mission" ? "mission" : defect == "fence" ? "geoFence" : "rallyPoints";
        auto object = fixture.value(section).toObject();
        if (defect == "mission") {
            object.insert("items", "InvalidItems");
        } else {
            object.insert("version", 99);
        }
        fixture.insert(section, object);
        QVERIFY(writePlan(path, fixture));
    } else if (defect == "task") {
        auto marine = fixture.value("marine").toObject();
        auto tasks = marine.value("tasks").toArray();
        auto task = tasks.at(0).toObject();
        task.insert("version", 99);
        tasks[0] = task;
        marine.insert("tasks", tasks);
        fixture.insert("marine", marine);
        QVERIFY(writePlan(path, fixture));
    } else if (defect == "artifact") {
        auto mission = fixture.value("mission").toObject();
        auto items = mission.value("items").toArray();
        bool changed = false;
        for (int i = 0; i < items.size(); ++i) {
            auto item = items.at(i).toObject();
            if (item.contains("resultContract")) {
                auto outcome = item.value("outcome").toObject();
                outcome.insert("readiness", "InvalidReadiness");
                item.insert("outcome", outcome);
                items[i] = item;
                changed = true;
            }
        }
        QVERIFY(changed);
        mission.insert("items", items);
        fixture.insert("mission", mission);
        QVERIFY(writePlan(path, fixture));
    }
    const auto beforeControllers =
        vehicle()->findChildren<PlanMasterController*>(QString(), Qt::FindDirectChildrenOnly);
    QSignalSpy missionActivity(vehicle()->missionManager(), &PlanManager::inProgressChanged);
    QSignalSpy missionProgress(vehicle()->missionManager(), &PlanManager::progressPctChanged);
    QSignalSpy fenceActivity(vehicle()->geoFenceManager(), &PlanManager::inProgressChanged);
    QSignalSpy rallyActivity(vehicle()->rallyPointManager(), &PlanManager::inProgressChanged);
    if (defect != "empty") {
        expectAppMessage(QRegularExpression(".+"));
    }
    PlanMasterController::sendPlanToVehicle(vehicle(), path);
    if (defect != "empty") {
        verifyExpectedLogMessage();
    }
    QPointer<PlanMasterController> transient;
    for (auto* controller : vehicle()->findChildren<PlanMasterController*>(QString(), Qt::FindDirectChildrenOnly)) {
        if (!beforeControllers.contains(controller)) {
            transient = controller;
        }
    }
    QVERIFY(transient);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(transient.isNull());
    QCOMPARE(missionActivity.count(), 0);
    QCOMPARE(missionProgress.count(), 0);
    QCOMPARE(fenceActivity.count(), 0);
    QCOMPARE(rallyActivity.count(), 0);
}

void MarineUploadGateTest::_acceptedFileUpload_data()
{
    QTest::addColumn<int>("kind");
    QTest::newRow("Ready") << -1;
    QTest::newRow("ReadyWithWarning") << -2;
    QTest::newRow("ordinary-QGC") << -3;
    QTest::newRow("intentional-empty-plan") << -4;
}

void MarineUploadGateTest::_acceptedFileUpload()
{
    QFETCH(int, kind);
    QTemporaryDir directory(QDir::currentPath() + "/build/v05-09-upload-XXXXXX");
    QVERIFY(directory.isValid());
    const QString path = directory.filePath("accepted.plan");
    QList<QGeoCoordinate> expectedCoordinates;
    const auto fixture = planFixture(kind);
    QVERIFY(!fixture.isEmpty());
    QVERIFY(writePlan(path, fixture));
    {
        PlanMasterController inspect;
        inspect.startStaticActiveVehicle(vehicle());
        inspect.loadFromFile(path);
        QVERIFY(inspect.missionController()->uploadAllowed());
        if (kind != -4) {
            auto* visualItems = inspect.missionController()->visualItems();
            for (int i = 0; i < visualItems->count(); ++i) {
                if (auto* marine = visualItems->value<CoverageInspectionComplexItem*>(i)) {
                    QVERIFY(marine->planningArtifact());
                    for (const auto& point : marine->planningArtifact()->result.path) {
                        expectedCoordinates.append(QGeoCoordinate(point.latitudeDeg, point.longitudeDeg, 0));
                    }
                } else if (auto* simple = visualItems->value<SimpleMissionItem*>(i)) {
                    expectedCoordinates.append(simple->missionItem().coordinate());
                } else {
                    expectedCoordinates.append(visualItems->value<VisualMissionItem*>(i)->coordinate());
                }
            }
        }
        if (kind == -1 || kind == -2) {
            auto* item = coverageItem(inspect);
            QVERIFY(item && item->planningArtifact());
            QCOMPARE(item->planningArtifact()->result.outcome.readiness,
                     kind == -1 ? MissionReadiness::Ready : MissionReadiness::ReadyWithWarning);
        }
    }
    if (kind == -4) {
        PlanMasterController seed;
        seed.startStaticActiveVehicle(vehicle());
        seed.missionController()->insertSimpleMissionItem(QGeoCoordinate(38, 121), 1, false);
        QSignalSpy seeded(vehicle()->missionManager(), &PlanManager::sendComplete);
        QString error;
        QVERIFY2(seed.missionController()->sendToVehicleChecked(error), qPrintable(error));
        QTRY_COMPARE_WITH_TIMEOUT(seeded.count(), 1, 10000);
        QCOMPARE(seeded.at(0).at(0).toBool(), false);
        QVERIFY(!vehicle()->missionManager()->missionItems().isEmpty());
    }
    QSignalSpy complete(vehicle()->missionManager(), &PlanManager::sendComplete);
    QSignalSpy activity(vehicle()->missionManager(), &PlanManager::inProgressChanged);
    const auto beforeControllers =
        vehicle()->findChildren<PlanMasterController*>(QString(), Qt::FindDirectChildrenOnly);
    PlanMasterController::sendPlanToVehicle(vehicle(), path);
    QPointer<PlanMasterController> transient;
    for (auto* controller : vehicle()->findChildren<PlanMasterController*>(QString(), Qt::FindDirectChildrenOnly)) {
        if (!beforeControllers.contains(controller)) {
            transient = controller;
        }
    }
    QVERIFY(transient);
    QTRY_COMPARE_WITH_TIMEOUT(complete.count(), 1, 10000);
    QCOMPARE(complete.at(0).at(0).toBool(), false);
    QVERIFY(activity.count() >= 2);
    QTRY_VERIFY_WITH_TIMEOUT(transient.isNull(), 10000);
    // Download confirms the actual remote mission, including a deliberate zero-item clear.
    QSignalSpy downloaded(vehicle()->missionManager(), &PlanManager::newMissionItemsAvailable);
    vehicle()->missionManager()->loadFromVehicle();
    QTRY_COMPARE_WITH_TIMEOUT(downloaded.count(), 1, 10000);
    if (kind == -4) {
        QCOMPARE(vehicle()->missionManager()->missionItems().size(), 0);
    } else {
        const auto& received = vehicle()->missionManager()->missionItems();
        QCOMPARE(received.size(), expectedCoordinates.size());
        for (int i = 0; i < received.size(); ++i) {
            auto* item = received.at(i);
            QCOMPARE(item->command(), MAV_CMD_NAV_WAYPOINT);
            QCOMPARE(item->sequenceNumber(), i);
            // MISSION_ITEM_INT quantizes latitude/longitude to 1e-7 degrees.
            QVERIFY(qAbs(item->param5() - expectedCoordinates.at(i).latitude()) <= 2e-7);
            QVERIFY(qAbs(item->param6() - expectedCoordinates.at(i).longitude()) <= 2e-7);
            QVERIFY(qAbs(item->param7() - expectedCoordinates.at(i).altitude()) <= 1e-4);
        }
    }
}

void MarineUploadGateTest::_conversionRefusalIsAtomic()
{
    PlanMasterController controller;
    controller.startStaticActiveVehicle(vehicle());
    auto* items = controller.missionController()->visualItems();
    controller.missionController()->insertSimpleMissionItem(QGeoCoordinate(38, 121), 1, false);
    auto* refusing = new RefusingSimpleItem(&controller);
    refusing->setParent(&controller);
    items->append(refusing);
    QVERIFY(controller.missionController()->uploadAllowed());
    const auto beforeSequences = sequences(items);
    const auto beforeChildren = vehicle()->findChildren<MissionItem*>(QString(), Qt::FindDirectChildrenOnly).size();
    const bool beforeDirty = controller.dirtyForUpload();
    QSignalSpy progress(vehicle()->missionManager(), &PlanManager::progressPctChanged);
    QSignalSpy fenceActivity(vehicle()->geoFenceManager(), &PlanManager::inProgressChanged);
    QSignalSpy rallyActivity(vehicle()->rallyPointManager(), &PlanManager::inProgressChanged);
    expectAppMessage(QRegularExpression("Intentional conversion refusal"));
    controller.sendToVehicle();
    verifyExpectedLogMessage();
    QVERIFY(refusing->allocated.isNull());
    QVERIFY(refusing->owner.isNull());
    QCOMPARE(progress.count(), 0);
    QCOMPARE(vehicle()->findChildren<MissionItem*>(QString(), Qt::FindDirectChildrenOnly).size(), beforeChildren);
    QCOMPARE(sequences(items), beforeSequences);
    QCOMPARE(controller.dirtyForUpload(), beforeDirty);
    emit vehicle() -> missionManager()->sendComplete(false);
    QCOMPARE(fenceActivity.count(), 0);
    QCOMPARE(rallyActivity.count(), 0);
    QCOMPARE(controller.dirtyForUpload(), beforeDirty);
}

void MarineUploadGateTest::_admissionNotifications()
{
    QTemporaryDir directory(QDir::currentPath() + "/build/v05-09-upload-XXXXXX");
    QVERIFY(directory.isValid());
    const QString path = directory.filePath("ready.plan");
    QVERIFY(writePlan(path, planFixture(-1)));
    PlanMasterController controller;
    controller.startStaticActiveVehicle(vehicle());
    auto* mission = controller.missionController();
    QSignalSpy notifications(mission, &MissionController::uploadAllowedChanged);
    controller.loadFromFile(path);
    QVERIFY(notifications.count() > 0);
    QVERIFY(mission->uploadAllowed());
    auto* item = coverageItem(controller);
    QVERIFY(item);
    item->setTaskName("renamed task");
    QVERIFY(mission->uploadAllowed());
    notifications.clear();
    item->setSwathWidthM(6);
    QVERIFY(notifications.count() > 0);
    QVERIFY(!mission->uploadAllowed());
    notifications.clear();
    mission->removeAll();
    QVERIFY(notifications.count() > 0);
    QVERIFY(mission->uploadAllowed());
}

UT_REGISTER_TEST(MarineUploadGateTest, TestLabel::Integration, TestLabel::Vehicle, TestLabel::MissionManager,
                 TestLabel::Serial)
