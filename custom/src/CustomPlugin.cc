#include "CustomPlugin.h"

#include <QtCore/QApplicationStatic>

#include <memory>

#include "CoverageInspectionComplexItem.h"
#include "CoverageInspectionPlanCreator.h"
#include "MarinePlanContext.h"
#include "MockCoveragePlanner.h"
#include "PlanMasterController.h"
#include "Vehicle.h"

namespace {

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
