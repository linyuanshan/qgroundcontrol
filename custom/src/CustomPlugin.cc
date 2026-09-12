#include "CustomPlugin.h"

#include <QtCore/QApplicationStatic>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QSet>

#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "CoverageInspectionComplexItem.h"
#include "CoverageInspectionPlanCreator.h"
#include "JsonParsing.h"
#include "LawnmowerCoveragePlanner.h"
#include "MarinePlanContext.h"
#include "MarineTaskJsonCodec.h"
#include "MissionController.h"
#include "MockCoveragePlanner.h"
#include "PlanMasterController.h"
#include "QmlObjectListModel.h"
#include "Vehicle.h"

namespace {

constexpr int MarinePlanVersion = 1;
constexpr const char* MarineKey = "marine";
constexpr const char* VersionKey = "version";
constexpr const char* TasksKey = "tasks";

Marine::MarinePlanContext* marinePlanContextFor(PlanMasterController* controller)
{
    if (controller == nullptr) {
        return nullptr;
    }

    auto* context = controller->findChild<Marine::MarinePlanContext*>(QString(), Qt::FindDirectChildrenOnly);
    if (context == nullptr) {
        context = new Marine::MarinePlanContext(controller);
    }
    if (context->plannerRegistry().planner("marine.coverage.mock") == nullptr) {
        (void) context->plannerRegistry().registerPlanner(std::make_shared<Marine::MockCoveragePlanner>());
    }
    if (context->plannerRegistry().planner("marine.coverage.lawnmower") == nullptr) {
        (void) context->plannerRegistry().registerPlanner(std::make_shared<Marine::LawnmowerCoveragePlanner>());
    }
    return context;
}

}  // namespace

Q_APPLICATION_STATIC(CustomPlugin, _customPluginInstance);

CustomPlugin::CustomPlugin(QObject* parent) : QGCCorePlugin(parent) {}

QGCCorePlugin* CustomPlugin::instance()
{
    return _customPluginInstance();
}

QVariantList CustomPlugin::complexMissionItemNames(Vehicle* vehicle)
{
    if (vehicle == nullptr) {
        return {};
    }

    QVariantList items = QGCCorePlugin::complexMissionItemNames(vehicle);
    if (vehicle->rover()) {
        QVariantMap entry;
        entry.insert(QStringLiteral("canonicalName"), QString(CoverageInspectionComplexItem::canonicalName));
        entry.insert(QStringLiteral("translatedName"),
                     CoverageInspectionComplexItem::tr(CoverageInspectionComplexItem::canonicalName));
        items.append(entry);
    }
    return items;
}

ComplexMissionItem* CustomPlugin::createComplexMissionItem(const QString& complexItemType,
                                                           PlanMasterController* masterController, bool flyView,
                                                           const QString& kmlOrShpFile)
{
    if ((complexItemType == CoverageInspectionComplexItem::canonicalName) ||
        (complexItemType == CoverageInspectionComplexItem::jsonComplexItemTypeValue)) {
        Marine::MarinePlanContext* marineContext = marinePlanContextFor(masterController);
        if (marineContext != nullptr) {
            return new CoverageInspectionComplexItem(masterController, flyView, marineContext);
        }
        return nullptr;
    }

    return QGCCorePlugin::createComplexMissionItem(complexItemType, masterController, flyView, kmlOrShpFile);
}

QList<PlanCreator*> CustomPlugin::planCreators(PlanMasterController* planMasterController)
{
    QList<PlanCreator*> creators = QGCCorePlugin::planCreators(planMasterController);
    Marine::MarinePlanContext* marineContext = marinePlanContextFor(planMasterController);
    if (marineContext != nullptr) {
        creators.append(new CoverageInspectionPlanCreator(planMasterController, marineContext));
    }
    return creators;
}

void CustomPlugin::postSaveToJson(PlanMasterController* planMasterController, QJsonObject& json)
{
    QJsonArray tasksJson;
    Marine::MarinePlanContext* context = marinePlanContextFor(planMasterController);
    MissionController* missionController =
        (planMasterController != nullptr) ? planMasterController->missionController() : nullptr;
    QmlObjectListModel* visualItems = (missionController != nullptr) ? missionController->visualItems() : nullptr;
    QSet<QString> savedTaskIds;

    if ((context != nullptr) && (visualItems != nullptr)) {
        for (int index = 0; index < visualItems->count(); ++index) {
            const auto* coverageItem = visualItems->value<CoverageInspectionComplexItem*>(index);
            if (coverageItem == nullptr) {
                continue;
            }

            const QString taskId = coverageItem->taskId();
            if (taskId.isEmpty() || savedTaskIds.contains(taskId)) {
                continue;
            }
            const Marine::MarineTask* task = context->task(taskId.toStdString());
            if (task == nullptr) {
                continue;
            }

            QJsonObject taskJson;
            QString errorString;
            if (Marine::MarineTaskJsonCodec::save(*task, taskJson, errorString)) {
                tasksJson.append(taskJson);
                savedTaskIds.insert(taskId);
            }
        }
    }

    if (!tasksJson.isEmpty()) {
        json.insert(QLatin1String(MarineKey),
                    QJsonObject{{QLatin1String(VersionKey), MarinePlanVersion}, {QLatin1String(TasksKey), tasksJson}});
    } else {
        json.remove(QLatin1String(MarineKey));
    }
}

bool CustomPlugin::preLoadFromJson(PlanMasterController* planMasterController, QJsonObject& json, QString& errorString)
{
    errorString.clear();
    Marine::MarinePlanContext* context = marinePlanContextFor(planMasterController);
    if (context == nullptr) {
        errorString = tr("Marine plan context is unavailable");
        return false;
    }

    std::vector<Marine::MarineTask> loadedTasks;
    std::unordered_set<std::string> loadedTaskIds;
    const QJsonValue marineValue = json.value(QLatin1String(MarineKey));
    if (!marineValue.isUndefined()) {
        if (!marineValue.isObject()) {
            errorString = tr("Marine plan section must be an object");
            return false;
        }

        const QJsonObject marineJson = marineValue.toObject();
        const QList<JsonParsing::KeyValidateInfo> keyInfo = {
            {.key = VersionKey, .type = QJsonValue::Double, .required = true},
            {.key = TasksKey, .type = QJsonValue::Array, .required = true},
        };
        if (!JsonParsing::validateKeys(marineJson, keyInfo, errorString)) {
            return false;
        }
        if (marineJson.value(QLatin1String(VersionKey)).toInt() != MarinePlanVersion) {
            errorString = tr("Unsupported marine plan version");
            return false;
        }

        const QJsonArray tasksJson = marineJson.value(QLatin1String(TasksKey)).toArray();
        loadedTasks.reserve(static_cast<std::size_t>(tasksJson.size()));
        for (const auto value : tasksJson) {
            if (!value.isObject()) {
                errorString = tr("Marine task must be an object");
                return false;
            }

            Marine::MarineTask task;
            if (!Marine::MarineTaskJsonCodec::load(value.toObject(), task, errorString)) {
                return false;
            }
            if (!loadedTaskIds.insert(task.id).second) {
                errorString = tr("Duplicate marine task id '%1'").arg(QString::fromStdString(task.id));
                return false;
            }
            loadedTasks.push_back(std::move(task));
        }
    }

    context->clearTasks();
    for (const Marine::MarineTask& task : loadedTasks) {
        context->addTask(task);
    }
    return true;
}
