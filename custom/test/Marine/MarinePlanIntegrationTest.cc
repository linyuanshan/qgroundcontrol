#include "MarinePlanIntegrationTest.h"

#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QPointer>
#include <QtCore/QTemporaryDir>
#include <QtCore/QVariantList>

#include "CoverageInspectionComplexItem.h"
#include "CoverageInspectionPlanCreator.h"
#include "MarinePlanContext.h"
#include "MissionController.h"
#include "MissionItem.h"
#include "PlanMasterController.h"
#include "QGCCorePlugin.h"
#include "QGCMapPolygon.h"
#include "QmlObjectListModel.h"

using namespace Marine;

namespace {

constexpr double CoordinateTolerance = 1e-7;
constexpr double DistanceToleranceM = 1e-3;

CoverageInspectionPlanCreator* coverageCreator(PlanMasterController& controller)
{
    QmlObjectListModel* creators = controller.planCreators();
    if (creators == nullptr) {
        return nullptr;
    }
    for (int index = 0; index < creators->count(); ++index) {
        if (auto* creator = creators->value<CoverageInspectionPlanCreator*>(index)) {
            return creator;
        }
    }
    return nullptr;
}

CoverageInspectionComplexItem* coverageItem(PlanMasterController& controller)
{
    QmlObjectListModel* visualItems = controller.missionController()->visualItems();
    if (visualItems == nullptr) {
        return nullptr;
    }
    for (int index = 0; index < visualItems->count(); ++index) {
        if (auto* item = visualItems->value<CoverageInspectionComplexItem*>(index)) {
            return item;
        }
    }
    return nullptr;
}

void comparePoint(const GeoPoint& actual, const GeoPoint& expected)
{
    QVERIFY(qAbs(actual.latitudeDeg - expected.latitudeDeg) < CoordinateTolerance);
    QVERIFY(qAbs(actual.longitudeDeg - expected.longitudeDeg) < CoordinateTolerance);
    QVERIFY(qAbs(actual.altitudeM - expected.altitudeM) < CoordinateTolerance);
}

void compareMissionItems(const QList<MissionItem*>& actual, const QList<MissionItem*>& expected)
{
    QCOMPARE(actual.size(), expected.size());
    for (int index = 0; index < actual.size(); ++index) {
        QCOMPARE(actual.at(index)->sequenceNumber(), expected.at(index)->sequenceNumber());
        QCOMPARE(actual.at(index)->command(), MAV_CMD_NAV_WAYPOINT);
        QCOMPARE(actual.at(index)->frame(), expected.at(index)->frame());
        QCOMPARE(actual.at(index)->autoContinue(), expected.at(index)->autoContinue());
        QVERIFY(qAbs(actual.at(index)->param5() - expected.at(index)->param5()) < CoordinateTolerance);
        QVERIFY(qAbs(actual.at(index)->param6() - expected.at(index)->param6()) < CoordinateTolerance);
        QVERIFY(qAbs(actual.at(index)->param7() - expected.at(index)->param7()) < CoordinateTolerance);
    }
}

}  // namespace

void MarinePlanIntegrationTest::_testPlanFileRoundTrip()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString planPath = temporaryDirectory.filePath(QStringLiteral("marine-round-trip.plan"));

    const GeoPolygon expectedBoundary{.vertices = {
                                          {.latitudeDeg = 47.3977, .longitudeDeg = 8.5455, .altitudeM = 0.0},
                                          {.latitudeDeg = 47.3977, .longitudeDeg = 8.5465, .altitudeM = 0.0},
                                          {.latitudeDeg = 47.3987, .longitudeDeg = 8.5465, .altitudeM = 0.0},
                                          {.latitudeDeg = 47.3987, .longitudeDeg = 8.5455, .altitudeM = 0.0},
                                      }};
    const GeoPolygon expectedNoGo{.vertices = {
                                      {.latitudeDeg = 47.39805, .longitudeDeg = 8.54585, .altitudeM = 0.0},
                                      {.latitudeDeg = 47.39805, .longitudeDeg = 8.54615, .altitudeM = 0.0},
                                      {.latitudeDeg = 47.39835, .longitudeDeg = 8.54615, .altitudeM = 0.0},
                                      {.latitudeDeg = 47.39835, .longitudeDeg = 8.54585, .altitudeM = 0.0},
                                  }};
    const std::string expectedName = "Harbor inspection";
    const std::string expectedVehicleId = "usv-01";
    const std::string expectedPlannerId = "marine.coverage.bcd";
    constexpr double ExpectedSwathWidthM = 20.0;
    constexpr double ExpectedSafetyMarginM = 0.0;
    constexpr double ExpectedSweepAngleDeg = 90.0;
    constexpr double ExpectedExecutionMarginM = 0.25;

    QString expectedTaskId;
    PlanningResult expectedPlanningResult;
    QVariantList expectedPathRoleRuns;
    QList<MissionItem*> expectedMissionItems;
    QPointer<MarinePlanContext> firstContext;
    QJsonObject savedPlanJson;

    {
        PlanMasterController controller(MAV_AUTOPILOT_ARDUPILOTMEGA, MAV_TYPE_GROUND_ROVER);
        controller.setFlyView(false);
        controller.start();

        auto* creator = coverageCreator(controller);
        QVERIFY(creator != nullptr);
        creator->createPlan(QGeoCoordinate(47.3982, 8.5460));

        auto* item = coverageItem(controller);
        QVERIFY(item != nullptr);
        auto* context = controller.findChild<MarinePlanContext*>(QString(), Qt::FindDirectChildrenOnly);
        QVERIFY(context != nullptr);
        firstContext = context;

        expectedTaskId = item->taskId();
        const MarineTask* task = context->task(expectedTaskId.toStdString());
        QVERIFY(task != nullptr);
        MarineTask configuredTask = *task;
        configuredTask.name = expectedName;
        configuredTask.vehicleId = expectedVehicleId;
        configuredTask.region.coverageBoundary = expectedBoundary;
        configuredTask.region.navigationBoundary = expectedBoundary;
        configuredTask.coverage.swathWidthM = ExpectedSwathWidthM;
        configuredTask.safety.hardSafetyMarginM = ExpectedSafetyMarginM;
        configuredTask.safety.preferredSafetyMarginM = ExpectedSafetyMarginM;
        configuredTask.coverage.sweepAngleMode = SweepAngleMode::Manual;
        configuredTask.coverage.sweepAngleDeg = ExpectedSweepAngleDeg;
        configuredTask.sensors.cameraEnabled = true;
        configuredTask.sensors.cameraRecord = false;
        configuredTask.sensors.sonarEnabled = false;
        configuredTask.sensors.sonarRecord = true;
        configuredTask.planner.plannerId = expectedPlannerId;
        configuredTask.planner.executionSafety.executionMarginM = ExpectedExecutionMarginM;
        context->addTask(configuredTask);

        QVERIFY(item->addNoGoRegion());
        QGCMapPolygon* noGoPolygon = item->noGoPolygons()->value<QGCMapPolygon*>(0);
        QVERIFY(noGoPolygon != nullptr);
        for (const GeoPoint& point : expectedNoGo.vertices) {
            noGoPolygon->appendVertex(QGeoCoordinate(point.latitudeDeg, point.longitudeDeg));
        }
        QTRY_VERIFY(item->noGoRegionsReady());
        QTRY_COMPARE(context->task(expectedTaskId.toStdString())->region.noGoRegions.size(), std::size_t{1});

        QVERIFY2(item->plan(), item->planningResult().message.c_str());
        QVERIFY(item->planningArtifact().has_value());
        expectedPlanningResult = item->planningArtifact()->result;
        QCOMPARE(expectedPlanningResult.status, PlanningStatus::Success);
        QVERIFY(!expectedPlanningResult.path.empty());
        QCOMPARE(expectedPlanningResult.legRoles.size(), expectedPlanningResult.path.size() - 1);
        QVERIFY(expectedPlanningResult.cellCount >= 1);
        expectedPathRoleRuns = item->generatedPathRoleRuns();
        QVERIFY(!expectedPathRoleRuns.isEmpty());
        item->appendMissionItems(expectedMissionItems, this);
        QCOMPARE(expectedMissionItems.size(), static_cast<qsizetype>(expectedPlanningResult.path.size()));

        QVERIFY(controller.saveToFile(planPath));
        QVERIFY(QFile::exists(planPath));

        QFile savedPlan(planPath);
        QVERIFY(savedPlan.open(QIODevice::ReadOnly | QIODevice::Text));
        savedPlanJson = QJsonDocument::fromJson(savedPlan.readAll()).object();
        QVERIFY(savedPlanJson.contains(QStringLiteral("marine")));
        QCOMPARE(
            savedPlanJson.value(QStringLiteral("marine")).toObject().value(QStringLiteral("tasks")).toArray().size(),
            1);
        const QJsonObject savedTask = savedPlanJson.value(QStringLiteral("marine"))
                                          .toObject()
                                          .value(QStringLiteral("tasks"))
                                          .toArray()
                                          .first()
                                          .toObject();
        QCOMPARE(savedTask.value(QStringLiteral("version")).toInt(), 3);
        QCOMPARE(savedTask.value(QStringLiteral("planner"))
                     .toObject()
                     .value(QStringLiteral("executionSafety"))
                     .toObject()
                     .value(QStringLiteral("executionMarginM"))
                     .toDouble(),
                 ExpectedExecutionMarginM);
    }

    QVERIFY(firstContext.isNull());

    PlanMasterController restoredController(MAV_AUTOPILOT_ARDUPILOTMEGA, MAV_TYPE_GROUND_ROVER);
    restoredController.setFlyView(false);
    restoredController.start();
    restoredController.loadFromFile(planPath);

    auto* restoredContext = restoredController.findChild<MarinePlanContext*>(QString(), Qt::FindDirectChildrenOnly);
    QVERIFY(restoredContext != nullptr);
    auto* restoredItem = coverageItem(restoredController);
    QVERIFY(restoredItem != nullptr);
    QCOMPARE(restoredItem->taskId(), expectedTaskId);
    QCOMPARE(restoredItem->planningState(), CoverageInspectionComplexItem::Planned);

    const MarineTask* restoredTask = restoredContext->task(expectedTaskId.toStdString());
    QVERIFY(restoredTask != nullptr);
    QCOMPARE(restoredTask->name, expectedName);
    QCOMPARE(restoredTask->vehicleId, expectedVehicleId);
    QCOMPARE(restoredTask->type, MarineTaskType::CoverageInspection);
    QCOMPARE(restoredTask->planner.plannerId, expectedPlannerId);
    QCOMPARE(restoredTask->planner.executionSafety.executionMarginM, ExpectedExecutionMarginM);
    QCOMPARE(restoredTask->coverage.swathWidthM, ExpectedSwathWidthM);
    QCOMPARE(restoredTask->safety.hardSafetyMarginM, ExpectedSafetyMarginM);
    QCOMPARE(restoredTask->safety.preferredSafetyMarginM, ExpectedSafetyMarginM);
    QCOMPARE(restoredTask->coverage.sweepAngleMode, SweepAngleMode::Manual);
    QCOMPARE(restoredTask->coverage.sweepAngleDeg, ExpectedSweepAngleDeg);
    QVERIFY(restoredTask->sensors.cameraEnabled);
    QVERIFY(!restoredTask->sensors.cameraRecord);
    QVERIFY(!restoredTask->sensors.sonarEnabled);
    QVERIFY(restoredTask->sensors.sonarRecord);
    QCOMPARE(restoredTask->region.coverageBoundary.vertices.size(), expectedBoundary.vertices.size());
    QCOMPARE(restoredTask->region.navigationBoundary.vertices.size(), expectedBoundary.vertices.size());
    for (std::size_t index = 0; index < expectedBoundary.vertices.size(); ++index) {
        comparePoint(restoredTask->region.coverageBoundary.vertices.at(index), expectedBoundary.vertices.at(index));
        comparePoint(restoredTask->region.navigationBoundary.vertices.at(index), expectedBoundary.vertices.at(index));
    }
    QCOMPARE(restoredTask->region.noGoRegions.size(), std::size_t{1});
    QCOMPARE(restoredTask->region.noGoRegions.front().vertices.size(), expectedNoGo.vertices.size());
    for (std::size_t index = 0; index < expectedNoGo.vertices.size(); ++index) {
        comparePoint(restoredTask->region.noGoRegions.front().vertices.at(index), expectedNoGo.vertices.at(index));
    }
    QCOMPARE(restoredItem->noGoPolygons()->count(), 1);
    QGCMapPolygon* restoredNoGoPolygon = restoredItem->noGoPolygons()->value<QGCMapPolygon*>(0);
    QVERIFY(restoredNoGoPolygon != nullptr);
    QCOMPARE(restoredNoGoPolygon->count(), static_cast<int>(expectedNoGo.vertices.size()));
    for (int index = 0; index < restoredNoGoPolygon->count(); ++index) {
        const QGeoCoordinate coordinate = restoredNoGoPolygon->vertexCoordinate(index);
        comparePoint({coordinate.latitude(), coordinate.longitude(), 0.0},
                     expectedNoGo.vertices.at(static_cast<std::size_t>(index)));
    }

    QVERIFY(!restoredItem->planningResult().path.empty());
    QVERIFY(restoredItem->planningArtifact().has_value());
    QVERIFY(!restoredItem->planningArtifact()->stale);
    const PlanningResult& restoredResult = restoredItem->planningArtifact()->result;
    QCOMPARE(restoredResult.status, expectedPlanningResult.status);
    QCOMPARE(restoredResult.legRoles, expectedPlanningResult.legRoles);
    QCOMPARE(restoredItem->generatedPathRoleRuns(), expectedPathRoleRuns);
    QCOMPARE(restoredResult.coverageLengthM, expectedPlanningResult.coverageLengthM);
    QCOMPARE(restoredResult.transitLengthM, expectedPlanningResult.transitLengthM);
    QCOMPARE(restoredResult.selectedSweepAngleDeg, expectedPlanningResult.selectedSweepAngleDeg);
    QCOMPARE(restoredResult.cellCount, expectedPlanningResult.cellCount);
    QCOMPARE(restoredResult.turnCount, expectedPlanningResult.turnCount);
    QCOMPARE(restoredResult.message, expectedPlanningResult.message);
    QCOMPARE(restoredResult.path.size(), expectedPlanningResult.path.size());
    QVERIFY(qAbs(restoredResult.pathLengthM - expectedPlanningResult.pathLengthM) < DistanceToleranceM);
    for (std::size_t index = 0; index < expectedPlanningResult.path.size(); ++index) {
        comparePoint(restoredResult.path.at(index), expectedPlanningResult.path.at(index));
    }
    QVERIFY(!restoredItem->dirty());

    QList<MissionItem*> restoredMissionItems;
    restoredItem->appendMissionItems(restoredMissionItems, this);
    compareMissionItems(restoredMissionItems, expectedMissionItems);

    for (const int oldVersion : {1, 2}) {
        QJsonObject oldTaskPlan = savedPlanJson;
        auto marine = oldTaskPlan.value("marine").toObject();
        auto tasks = marine.value("tasks").toArray();
        auto task = tasks.first().toObject();
        task.insert("version", oldVersion);
        tasks[0] = task;
        marine.insert("tasks", tasks);
        oldTaskPlan.insert("marine", marine);
        QString error;
        QVERIFY(!QGCCorePlugin::instance()->preLoadFromJson(&restoredController, oldTaskPlan, error));
        QVERIFY(error.contains("Unsupported development schema"));
        QJsonArray artifacts;
        restoredItem->save(artifacts);
        QCOMPARE(artifacts.size(), 1);
        auto artifact = artifacts.first().toObject();
        artifact.insert("version", oldVersion);
        QVERIFY(!restoredItem->load(artifact, 0, error));
        QVERIFY(error.contains("Unsupported development schema"));
        QVERIFY(!restoredItem->planningResult().path.empty());
    }
}

UT_REGISTER_TEST(MarinePlanIntegrationTest, TestLabel::Integration, TestLabel::MissionManager)
