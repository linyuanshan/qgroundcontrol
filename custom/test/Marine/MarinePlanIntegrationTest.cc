#include "MarinePlanIntegrationTest.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QPointer>
#include <QtCore/QTemporaryDir>

#include "CoverageInspectionComplexItem.h"
#include "CoverageInspectionPlanCreator.h"
#include "MarinePlanContext.h"
#include "MissionController.h"
#include "MissionItem.h"
#include "PlanMasterController.h"
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
    const std::string expectedName = "Harbor inspection";
    const std::string expectedVehicleId = "usv-01";
    const std::string expectedPlannerId = "marine.coverage.mock";
    constexpr double ExpectedSwathWidthM = 8.5;
    constexpr double ExpectedSafetyMarginM = 2.25;
    constexpr double ExpectedSweepAngleDeg = 37.5;

    QString expectedTaskId;
    PlanningResult expectedPlanningResult;
    QList<MissionItem*> expectedMissionItems;
    QPointer<MarinePlanContext> firstContext;

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
        configuredTask.region.outerBoundary = expectedBoundary;
        configuredTask.coverage.swathWidthM = ExpectedSwathWidthM;
        configuredTask.coverage.safetyMarginM = ExpectedSafetyMarginM;
        configuredTask.coverage.sweepAngleMode = SweepAngleMode::Manual;
        configuredTask.coverage.sweepAngleDeg = ExpectedSweepAngleDeg;
        configuredTask.sensors.cameraEnabled = true;
        configuredTask.sensors.cameraRecord = false;
        configuredTask.sensors.sonarEnabled = false;
        configuredTask.sensors.sonarRecord = true;
        configuredTask.planner.plannerId = expectedPlannerId;
        context->addTask(configuredTask);

        QVERIFY(item->plan());
        expectedPlanningResult = item->planningResult();
        QCOMPARE(expectedPlanningResult.status, PlanningStatus::Success);
        QCOMPARE(expectedPlanningResult.path.size(), 3);
        item->appendMissionItems(expectedMissionItems, this);
        QCOMPARE(expectedMissionItems.size(), 3);

        QVERIFY(controller.saveToFile(planPath));
        QVERIFY(QFile::exists(planPath));

        QFile savedPlan(planPath);
        QVERIFY(savedPlan.open(QIODevice::ReadOnly | QIODevice::Text));
        const QJsonObject savedJson = QJsonDocument::fromJson(savedPlan.readAll()).object();
        QVERIFY(savedJson.contains(QStringLiteral("marine")));
        QCOMPARE(savedJson.value(QStringLiteral("marine")).toObject().value(QStringLiteral("tasks")).toArray().size(),
                 1);
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
    QCOMPARE(restoredTask->coverage.swathWidthM, ExpectedSwathWidthM);
    QCOMPARE(restoredTask->coverage.safetyMarginM, ExpectedSafetyMarginM);
    QCOMPARE(restoredTask->coverage.sweepAngleMode, SweepAngleMode::Manual);
    QCOMPARE(restoredTask->coverage.sweepAngleDeg, ExpectedSweepAngleDeg);
    QVERIFY(restoredTask->sensors.cameraEnabled);
    QVERIFY(!restoredTask->sensors.cameraRecord);
    QVERIFY(!restoredTask->sensors.sonarEnabled);
    QVERIFY(restoredTask->sensors.sonarRecord);
    QCOMPARE(restoredTask->region.outerBoundary.vertices.size(), expectedBoundary.vertices.size());
    for (std::size_t index = 0; index < expectedBoundary.vertices.size(); ++index) {
        comparePoint(restoredTask->region.outerBoundary.vertices.at(index), expectedBoundary.vertices.at(index));
    }
    QVERIFY(restoredTask->region.noGoRegions.empty());

    const PlanningResult& restoredResult = restoredItem->planningResult();
    QCOMPARE(restoredResult.status, expectedPlanningResult.status);
    QCOMPARE(restoredResult.selectedSweepAngleDeg, expectedPlanningResult.selectedSweepAngleDeg);
    QCOMPARE(restoredResult.turnCount, expectedPlanningResult.turnCount);
    QCOMPARE(restoredResult.path.size(), expectedPlanningResult.path.size());
    QVERIFY(qAbs(restoredResult.pathLengthM - expectedPlanningResult.pathLengthM) < DistanceToleranceM);
    for (std::size_t index = 0; index < expectedPlanningResult.path.size(); ++index) {
        comparePoint(restoredResult.path.at(index), expectedPlanningResult.path.at(index));
    }

    QList<MissionItem*> restoredMissionItems;
    restoredItem->appendMissionItems(restoredMissionItems, this);
    compareMissionItems(restoredMissionItems, expectedMissionItems);
}

UT_REGISTER_TEST(MarinePlanIntegrationTest, TestLabel::Integration, TestLabel::MissionManager)
