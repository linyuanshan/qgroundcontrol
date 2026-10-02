#include "CoverageInspectionPlanCreatorTest.h"

#include <cmath>
#include <memory>

#include "BoustrophedonCoveragePlanner.h"
#include "CoverageInspectionComplexItem.h"
#include "CoverageInspectionPlanCreator.h"
#include "CoverageStrategySemantics.h"
#include "LawnmowerCoveragePlanner.h"
#include "MarinePlanContext.h"
#include "MissionController.h"
#include "MissionItem.h"
#include "MissionSettingsItem.h"
#include "MockCoveragePlanner.h"
#include "PlanMasterController.h"
#include "QGCMAVLink.h"
#include "QmlObjectListModel.h"

using namespace Marine;

void CoverageInspectionPlanCreatorTest::init()
{
    setOfflineFirmwareType(MAV_AUTOPILOT_ARDUPILOTMEGA);
    setOfflineVehicleType(MAV_TYPE_GROUND_ROVER);
    OfflineMissionTest::init();
    _marineContext = planController()->findChild<MarinePlanContext*>(QString(), Qt::FindDirectChildrenOnly);
    if (_marineContext == nullptr) {
        _marineContext = new MarinePlanContext(planController());
    }
    if (!_marineContext->plannerRegistry().planner("marine.coverage.mock")) {
        QVERIFY(_marineContext->plannerRegistry().registerPlanner(std::make_shared<MockCoveragePlanner>()));
    }
    if (!_marineContext->plannerRegistry().planner("marine.coverage.lawnmower")) {
        QVERIFY(_marineContext->plannerRegistry().registerPlanner(std::make_shared<LawnmowerCoveragePlanner>()));
    }
    if (!_marineContext->plannerRegistry().planner("marine.coverage.bcd")) {
        QVERIFY(_marineContext->plannerRegistry().registerPlanner(std::make_shared<BoustrophedonCoveragePlanner>()));
    }
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
    QCOMPARE(task->planner.plannerId, std::string("marine.coverage.auto"));
    QVERIFY(std::isnan(task->safety.hardSafetyMarginM));
    QVERIFY(std::isnan(task->safety.preferredSafetyMarginM));
    QVERIFY(std::isnan(task->planner.executionSafety.executionMarginM));
    QVERIFY(!task->schemaValid());
    MarineTask configured = *task;
    configured.coverage.swathWidthM = 5.0;
    configured.safety.hardSafetyMarginM = 0.0;
    configured.safety.preferredSafetyMarginM = 0.0;
    configured.planner.executionSafety.executionMarginM = 0.0;
    QVERIFY(configured.schemaValid());
    QCOMPARE(task->region.coverageBoundary.vertices.size(), std::size_t(4));
    QCOMPARE(task->region.navigationBoundary.vertices.size(), std::size_t(4));
    QCOMPARE(task->region.navigationBoundary.vertices.front().latitudeDeg,
             task->region.coverageBoundary.vertices.front().latitudeDeg);
    QVERIFY(task->region.coverageBoundary.vertices.front().latitudeDeg != mapCenter.latitude());
    QVERIFY(task->region.coverageBoundary.vertices.front().longitudeDeg != mapCenter.longitude());
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
    coverageItem->setSwathWidthM(5.0);
    QCOMPARE(coverageItem->plannerId(), QStringLiteral("marine.coverage.auto"));
    QVERIFY(!coverageItem->plan());
    QVERIFY(coverageItem->planningResult().path.empty());

    // Explicit v0.5 BCD exercises creation and integrated certification.
    MarineTask task = *_marineContext->task(coverageItem->taskId().toStdString());
    task.planner.plannerId = "marine.coverage.bcd";
    task.safety.hardSafetyMarginM = 0.0;
    task.safety.preferredSafetyMarginM = 0.0;
    task.planner.executionSafety.executionMarginM = 0.0;
    QVERIFY(_marineContext->updateTask(task));
    QVERIFY2(coverageItem->plan(), coverageItem->planningResult().message.c_str());
    QVERIFY(coverageItem->planningArtifact().has_value());
    const auto& result = coverageItem->planningArtifact()->result;
    QCOMPARE(result.status, PlanningStatus::Success);
    QVERIFY(result.path.size() > 3);
    QCOMPARE(result.legRoles.size(), result.path.size() - 1);
    QVERIFY(result.coverageLengthM > 0.0);
    QVERIFY(result.transitLengthM >= 0.0);
    QCOMPARE(result.pathLengthM, result.coverageLengthM + result.transitLengthM);
    QVERIFY(result.cellCount >= 1);
    QVERIFY(result.turnCount > 0);
    QVERIFY(result.plannerSource.has_value());
    const PlannerSourceInfo& source = *result.plannerSource;
    QCOMPARE(source.requestedPlannerId, std::string(CoverageStrategySemantics::BoustrophedonId));
    QCOMPARE(source.resolvedStrategy.strategyId, std::string(CoverageStrategySemantics::BoustrophedonId));
    QCOMPARE(source.resolvedStrategy.semanticVersion, std::string(CoverageStrategySemantics::BoustrophedonVersion));
    QCOMPARE(source.resolutionStatus, PlannerResolutionStatus::Resolved);
    QCOMPARE(source.resolutionReason, PlannerResolutionReason::None);
    QVERIFY(!source.escalated);
    for (const auto& point : result.path) {
        QVERIFY(std::isfinite(point.latitudeDeg));
        QVERIFY(std::isfinite(point.longitudeDeg));
        QVERIFY(std::isfinite(point.altitudeM));
        QVERIFY(point.latitudeDeg >= -90.0 && point.latitudeDeg <= 90.0);
        QVERIFY(point.longitudeDeg >= -180.0 && point.longitudeDeg <= 180.0);
    }
    QVERIFY(!coverageItem->planningResult().path.empty());
    QList<MissionItem*> missionItems;
    coverageItem->appendMissionItems(missionItems, this);
    QCOMPARE(missionItems.size(), static_cast<qsizetype>(result.path.size()));
}

UT_REGISTER_TEST(CoverageInspectionPlanCreatorTest, TestLabel::Unit, TestLabel::MissionManager)
