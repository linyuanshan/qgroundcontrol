#include "CustomPluginIntegrationTest.h"

#include "CoverageInspectionComplexItem.h"
#include "CoverageInspectionPlanCreator.h"
#include "MarinePlanContext.h"
#include "MissionController.h"
#include "PlanMasterController.h"
#include "QGCCorePlugin.h"
#include "QmlObjectListModel.h"

namespace {

int coverageMenuEntryCount(const QVariantList& entries)
{
    int count = 0;
    for (const QVariant& entry : entries) {
        if (entry.toMap().value(QStringLiteral("canonicalName")).toString() ==
            CoverageInspectionComplexItem::canonicalName) {
            ++count;
        }
    }
    return count;
}

}  // namespace

void CustomPluginIntegrationTest::init()
{
    setOfflineFirmwareType(MAV_AUTOPILOT_ARDUPILOTMEGA);
    setOfflineVehicleType(MAV_TYPE_GROUND_ROVER);
    OfflineMissionTest::init();
}

void CustomPluginIntegrationTest::_testPlanContextAndCreatorRegistration()
{
    const auto contexts =
        planController()->findChildren<Marine::MarinePlanContext*>(QString(), Qt::FindDirectChildrenOnly);
    QCOMPARE(contexts.size(), 1);
    QVERIFY(contexts.first()->plannerRegistry().planner("marine.coverage.mock") != nullptr);

    QmlObjectListModel* creators = planController()->planCreators();
    QVERIFY(creators != nullptr);
    int coverageCreatorCount = 0;
    for (int index = 0; index < creators->count(); ++index) {
        if (creators->value<CoverageInspectionPlanCreator*>(index) != nullptr) {
            ++coverageCreatorCount;
        }
    }
    QCOMPARE(coverageCreatorCount, 1);
}

void CustomPluginIntegrationTest::_testComplexItemMenuRegistration()
{
    const QVariantList entries = missionController()->complexMissionItems();
    QCOMPARE(coverageMenuEntryCount(entries), 1);

    for (const QVariant& entry : entries) {
        const QVariantMap item = entry.toMap();
        if (item.value(QStringLiteral("canonicalName")).toString() == CoverageInspectionComplexItem::canonicalName) {
            QCOMPARE(item.value(QStringLiteral("translatedName")).toString(), QStringLiteral("Coverage Inspection"));
        }
    }
}

void CustomPluginIntegrationTest::_testFactoryRegistrationAndContextReuse()
{
    PlanMasterController controller(MAV_AUTOPILOT_ARDUPILOTMEGA, MAV_TYPE_GROUND_ROVER);
    controller.setFlyView(false);
    QGCCorePlugin* plugin = QGCCorePlugin::instance();

    ComplexMissionItem* canonicalItem =
        plugin->createComplexMissionItem(CoverageInspectionComplexItem::canonicalName, &controller, false);
    ComplexMissionItem* jsonItem =
        plugin->createComplexMissionItem(CoverageInspectionComplexItem::jsonComplexItemTypeValue, &controller, false);

    QVERIFY(qobject_cast<CoverageInspectionComplexItem*>(canonicalItem) != nullptr);
    QVERIFY(qobject_cast<CoverageInspectionComplexItem*>(jsonItem) != nullptr);
    const auto contexts = controller.findChildren<Marine::MarinePlanContext*>(QString(), Qt::FindDirectChildrenOnly);
    QCOMPARE(contexts.size(), 1);

    delete canonicalItem;
    delete jsonItem;
}

void CustomPluginIntegrationTest::_testMarineEntriesFilteredForNonMarineVehicle()
{
    PlanMasterController controller(MAV_AUTOPILOT_PX4, MAV_TYPE_QUADROTOR);
    controller.setFlyView(false);
    controller.start();

    QmlObjectListModel* creators = controller.planCreators();
    QVERIFY(creators != nullptr);
    for (int index = 0; index < creators->count(); ++index) {
        QVERIFY(creators->value<CoverageInspectionPlanCreator*>(index) == nullptr);
    }
    QCOMPARE(coverageMenuEntryCount(controller.missionController()->complexMissionItems()), 0);
}

void CustomPluginIntegrationTest::_testNullVehicleMenuHandled()
{
    QCOMPARE(QGCCorePlugin::instance()->complexMissionItemNames(nullptr).size(), 0);
}

UT_REGISTER_TEST(CustomPluginIntegrationTest, TestLabel::Unit, TestLabel::MissionManager)
