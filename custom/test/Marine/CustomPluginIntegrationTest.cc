#include "CustomPluginIntegrationTest.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>

#include "CoverageInspectionComplexItem.h"
#include "CoverageInspectionPlanCreator.h"
#include "MarinePlanContext.h"
#include "MarineTaskJsonCodec.h"
#include "MissionController.h"
#include "PlanMasterController.h"
#include "QGCCorePlugin.h"
#include "QmlObjectListModel.h"

using namespace Marine;

namespace {

MarineTask validTask(const std::string& id)
{
    MarineTask task;
    task.id = id;
    task.name = "Harbor inspection";
    task.planner.plannerId = "marine.coverage.mock";
    task.coverage.swathWidthM = 5.0;
    task.region.outerBoundary.vertices = {
        {.latitudeDeg = 47.3977, .longitudeDeg = 8.5455, .altitudeM = 0.0},
        {.latitudeDeg = 47.3977, .longitudeDeg = 8.5465, .altitudeM = 0.0},
        {.latitudeDeg = 47.3987, .longitudeDeg = 8.5465, .altitudeM = 0.0},
        {.latitudeDeg = 47.3987, .longitudeDeg = 8.5455, .altitudeM = 0.0},
    };
    return task;
}

QJsonObject saveTask(const MarineTask& task)
{
    QJsonObject taskJson;
    QString errorString;
    (void) MarineTaskJsonCodec::save(task, taskJson, errorString);
    return taskJson;
}

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
    QVERIFY(contexts.first()->plannerRegistry().planner("marine.coverage.lawnmower") != nullptr);

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

void CustomPluginIntegrationTest::_testMarinePlanSaveFiltersOrphans()
{
    auto* context = planController()->findChild<MarinePlanContext*>(QString(), Qt::FindDirectChildrenOnly);
    QVERIFY(context != nullptr);
    const MarineTask referencedTask = validTask("referenced-task");
    const MarineTask orphanTask = validTask("orphan-task");
    context->addTask(referencedTask);
    context->addTask(orphanTask);

    VisualMissionItem* visualItem = missionController()->insertComplexMissionItem(
        CoverageInspectionComplexItem::canonicalName, QGeoCoordinate(47.398, 8.546), -1, true);
    auto* coverageItem = qobject_cast<CoverageInspectionComplexItem*>(visualItem);
    QVERIFY(coverageItem != nullptr);
    coverageItem->setTaskId(QString::fromStdString(referencedTask.id));
    QVERIFY(coverageItem->plan());

    const QJsonObject planJson = planController()->saveToJson().object();
    const QJsonObject marineJson = planJson.value(QStringLiteral("marine")).toObject();
    QCOMPARE(marineJson.value(QStringLiteral("version")).toInt(), 1);
    const QJsonArray tasksJson = marineJson.value(QStringLiteral("tasks")).toArray();
    QCOMPARE(tasksJson.size(), 1);
    QCOMPARE(tasksJson.at(0).toObject().value(QStringLiteral("id")).toString(),
             QString::fromStdString(referencedTask.id));
}

void CustomPluginIntegrationTest::_testOrdinaryPlanOmitsMarineSection()
{
    const QJsonObject planJson = planController()->saveToJson().object();
    QVERIFY(!planJson.contains(QStringLiteral("marine")));
}

void CustomPluginIntegrationTest::_testMarinePlanPreload()
{
    auto* context = planController()->findChild<MarinePlanContext*>(QString(), Qt::FindDirectChildrenOnly);
    QVERIFY(context != nullptr);
    context->addTask(validTask("old-task"));
    const MarineTask loadedTask = validTask("loaded-task");
    QJsonObject planJson{
        {QStringLiteral("marine"),
         QJsonObject{{QStringLiteral("version"), 1}, {QStringLiteral("tasks"), QJsonArray{saveTask(loadedTask)}}}}};

    QString errorString;
    QVERIFY2(QGCCorePlugin::instance()->preLoadFromJson(planController(), planJson, errorString),
             qPrintable(errorString));
    QVERIFY(context->task("old-task") == nullptr);
    const MarineTask* restoredTask = context->task(loadedTask.id);
    QVERIFY(restoredTask != nullptr);
    QCOMPARE(restoredTask->name, loadedTask.name);
    QVERIFY(context->plannerRegistry().planner("marine.coverage.mock") != nullptr);
    QVERIFY(context->plannerRegistry().planner("marine.coverage.lawnmower") != nullptr);
}

void CustomPluginIntegrationTest::_testMarinePlanPreloadValidation()
{
    auto* context = planController()->findChild<MarinePlanContext*>(QString(), Qt::FindDirectChildrenOnly);
    QVERIFY(context != nullptr);
    const MarineTask existingTask = validTask("existing-task");
    context->addTask(existingTask);
    QGCCorePlugin* plugin = QGCCorePlugin::instance();
    QString errorString;

    QJsonObject unsupportedVersion{{QStringLiteral("marine"), QJsonObject{{QStringLiteral("version"), 2},
                                                                          {QStringLiteral("tasks"), QJsonArray()}}}};
    QVERIFY(!plugin->preLoadFromJson(planController(), unsupportedVersion, errorString));
    QVERIFY(errorString.contains(QStringLiteral("version"), Qt::CaseInsensitive));
    QVERIFY(context->task(existingTask.id) != nullptr);

    const MarineTask duplicateTask = validTask("duplicate-task");
    QJsonObject duplicateIds{
        {QStringLiteral("marine"),
         QJsonObject{{QStringLiteral("version"), 1},
                     {QStringLiteral("tasks"), QJsonArray{saveTask(duplicateTask), saveTask(duplicateTask)}}}}};
    errorString.clear();
    QVERIFY(!plugin->preLoadFromJson(planController(), duplicateIds, errorString));
    QVERIFY(errorString.contains(QStringLiteral("duplicate"), Qt::CaseInsensitive));
    QVERIFY(context->task(existingTask.id) != nullptr);

    QJsonObject marineWithoutTasks{{QStringLiteral("marine"), QJsonObject{{QStringLiteral("version"), 1},
                                                                          {QStringLiteral("tasks"), QJsonArray()}}}};
    QVERIFY(plugin->preLoadFromJson(planController(), marineWithoutTasks, errorString));
    QJsonObject brokenReference{
        {QStringLiteral("version"), 1},
        {QStringLiteral("type"), QStringLiteral("ComplexItem")},
        {QStringLiteral("complexItemType"), CoverageInspectionComplexItem::jsonComplexItemTypeValue},
        {QStringLiteral("taskId"), QStringLiteral("missing-task")},
        {QStringLiteral("planningStatus"), QStringLiteral("success")},
        {QStringLiteral("generatedPath"), QJsonArray()},
        {QStringLiteral("pathLengthM"), 0.0},
        {QStringLiteral("planningMessage"), QStringLiteral("")},
    };
    errorString.clear();
    auto* item = new CoverageInspectionComplexItem(planController(), false, context);
    QVERIFY(!item->load(brokenReference, 0, errorString));
    QVERIFY(errorString.contains(QStringLiteral("missing-task")));
}

UT_REGISTER_TEST(CustomPluginIntegrationTest, TestLabel::Unit, TestLabel::MissionManager)
