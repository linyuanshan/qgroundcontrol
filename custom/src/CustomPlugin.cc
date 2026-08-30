#include "CustomPlugin.h"

#include <QtCore/QApplicationStatic>

#include "CoverageInspectionComplexItem.h"
#include "MarinePlanContext.h"
#include "PlanMasterController.h"

Q_APPLICATION_STATIC(CustomPlugin, _customPluginInstance);

CustomPlugin::CustomPlugin(QObject* parent) : QGCCorePlugin(parent) {}

QGCCorePlugin* CustomPlugin::instance()
{
    return _customPluginInstance();
}

ComplexMissionItem* CustomPlugin::createComplexMissionItem(const QString& complexItemType,
                                                           PlanMasterController* masterController, bool flyView,
                                                           const QString& kmlOrShpFile)
{
    if ((complexItemType == CoverageInspectionComplexItem::canonicalName) && (masterController != nullptr)) {
        auto* marineContext =
            masterController->findChild<Marine::MarinePlanContext*>(QString(), Qt::FindDirectChildrenOnly);
        if (marineContext != nullptr) {
            return new CoverageInspectionComplexItem(masterController, flyView, marineContext);
        }
    }

    return QGCCorePlugin::createComplexMissionItem(complexItemType, masterController, flyView, kmlOrShpFile);
}
