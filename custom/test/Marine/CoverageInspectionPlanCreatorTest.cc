#include "CoverageInspectionPlanCreatorTest.h"

#include <cmath>

#include "CoverageInspectionComplexItem.h"
#include "CoverageInspectionPlanCreator.h"
#include "MarinePlanContext.h"
#include "MissionController.h"
#include "MissionItem.h"
#include "MissionSettingsItem.h"
#include "QGCMAVLink.h"
#include "QmlObjectListModel.h"

using namespace Marine;

void CoverageInspectionPlanCreatorTest::init()
{
    setOfflineFirmwareType(MAV_AUTOPILOT_ARDUPILOTMEGA);
    setOfflineVehicleType(MAV_TYPE_GROUND_ROVER);
    OfflineMissionTest::init();
    _marineContext = new MarinePlanContext(planController());
    _creator = new CoverageInspectionPlanCreator(planController(), _marineContext);
}

void CoverageInspectionPlanCreatorTest::cleanup()
{
    _creator = nullptr;
    _marineContext = nullptr;
    OfflineMissionTest::cleanup();
}

void CoverageInspectionPlanCreatorTest::_testMetadata()
{
    QCOMPARE(_creator->property("name").toString(), QStringLiteral("Coverage Inspection"));
    QVERIFY(_creator->supportsVehicleClass(QGCMAVLink::VehicleClassRoverBoat));
    QVERIFY(!_creator->supportsVehicleClass(QGCMAVLink::VehicleClassMultiRotor));
    QVERIFY(!_creator->supportsVehicleClass(QGCMAVLink::VehicleClassFixedWing));
}

void CoverageInspectionPlanCreatorTest::_testCreatePlan()
{
    const QGeoCoordinate mapCenter(38.1, 121.1);
    _creator->createPlan(mapCenter);

    QmlObjectListModel* visualItems = missionController()->visualItems();
    QCOMPARE(visualItems->count(), 2);
    QVERIFY(visualItems->value<MissionSettingsItem*>(0) != nullptr);
    auto* coverageItem = visualItems->value<CoverageInspectionComplexItem*>(1);
    QVERIFY(coverageItem != nullptr);
    QCOMPARE(coverageItem->sequenceNumber(), 1);
    QCOMPARE(missionController()->currentPlanViewSeqNum(), 1);
    QCOMPARE(coverageItem->planningState(), CoverageInspectionComplexItem::Unplanned);

    const MarineTask* task = _marineContext->task(coverageItem->taskId().toStdString());
    QVERIFY(task != nullptr);
    QCOMPARE(task->type, MarineTaskType::CoverageInspection);
    QCOMPARE(task->name, std::string("Coverage Inspection"));
    QCOMPARE(task->planner.plannerId, std::string("marine.coverage.mock"));
    QCOMPARE(task->region.outerBoundary.vertices.size(), std::size_t(4));
    QVERIFY(task->region.outerBoundary.vertices.front().latitudeDeg != mapCenter.latitude());
    QVERIFY(task->region.outerBoundary.vertices.front().longitudeDeg != mapCenter.longitude());
}

void CoverageInspectionPlanCreatorTest::_testCreatePlanReplacesExistingPlan()
{
    MarineTask oldTask;
    const std::string oldTaskId = oldTask.id;
    _marineContext->addTask(oldTask);
    missionController()->insertSimpleMissionItem(QGeoCoordinate(38.0, 121.0), -1);
    QCOMPARE(missionController()->visualItems()->count(), 2);

    _creator->createPlan(QGeoCoordinate(38.1, 121.1));

    QCOMPARE(missionController()->visualItems()->count(), 2);
    QVERIFY(_marineContext->task(oldTaskId) == nullptr);
    auto* coverageItem = missionController()->visualItems()->value<CoverageInspectionComplexItem*>(1);
    QVERIFY(coverageItem != nullptr);
    const QString firstTaskId = coverageItem->taskId();

    _creator->createPlan(QGeoCoordinate(38.2, 121.2));

    QCOMPARE(missionController()->visualItems()->count(), 2);
    QVERIFY(_marineContext->task(firstTaskId.toStdString()) == nullptr);
    coverageItem = missionController()->visualItems()->value<CoverageInspectionComplexItem*>(1);
    QVERIFY(coverageItem != nullptr);
    QVERIFY(coverageItem->taskId() != firstTaskId);
    QVERIFY(_marineContext->task(coverageItem->taskId().toStdString()) != nullptr);
}

void CoverageInspectionPlanCreatorTest::_testCreatePlanWithTwoDimensionalCenter()
{
    _creator->createPlan(QGeoCoordinate(38.1, 121.1));

    auto* coverageItem = missionController()->visualItems()->value<CoverageInspectionComplexItem*>(1);
    QVERIFY(coverageItem != nullptr);
    QVERIFY(coverageItem->plan());

    QList<MissionItem*> missionItems;
    coverageItem->appendMissionItems(missionItems, this);
    QVERIFY(!missionItems.isEmpty());
    for (const MissionItem* missionItem : missionItems) {
        QVERIFY(std::isfinite(missionItem->param5()));
        QVERIFY(std::isfinite(missionItem->param6()));
        QVERIFY(std::isfinite(missionItem->param7()));
        QVERIFY(missionItem->param5() >= -90.0 && missionItem->param5() <= 90.0);
        QVERIFY(missionItem->param6() >= -180.0 && missionItem->param6() <= 180.0);
    }
}

UT_REGISTER_TEST(CoverageInspectionPlanCreatorTest, TestLabel::Unit, TestLabel::MissionManager)
