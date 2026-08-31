#include "CoverageInspectionPlanCreator.h"

#include "CoverageInspectionComplexItem.h"
#include "MarinePlanContext.h"
#include "MarineTask.h"
#include "MissionController.h"
#include "PlanMasterController.h"
#include "QGCMAVLink.h"

CoverageInspectionPlanCreator::CoverageInspectionPlanCreator(PlanMasterController* planMasterController,
                                                             Marine::MarinePlanContext* marineContext)
    : PlanCreator(planMasterController, CoverageInspectionComplexItem::tr(CoverageInspectionComplexItem::canonicalName),
                  QString(), {QGCMAVLink::VehicleClassRoverBoat}),
      _marineContext(marineContext)
{}

void CoverageInspectionPlanCreator::createPlan(const QGeoCoordinate& mapCenterCoord)
{
    if (!_marineContext) {
        return;
    }

    _planMasterController->removeAll();
    _marineContext->clearTasks();

    Marine::MarineTask task;
    task.name = CoverageInspectionComplexItem::canonicalName;
    task.planner.plannerId = "marine.coverage.mock";
    // Keep the creator deterministic and immediately plannable; the editor can replace this test region.
    const double latitude = mapCenterCoord.latitude();
    const double longitude = mapCenterCoord.longitude();
    constexpr double delta = 0.0005;
    constexpr double altitudeM = 0.0;
    task.region.outerBoundary.vertices = {
        {.latitudeDeg = latitude - delta, .longitudeDeg = longitude - delta, .altitudeM = altitudeM},
        {.latitudeDeg = latitude - delta, .longitudeDeg = longitude + delta, .altitudeM = altitudeM},
        {.latitudeDeg = latitude + delta, .longitudeDeg = longitude + delta, .altitudeM = altitudeM},
        {.latitudeDeg = latitude + delta, .longitudeDeg = longitude - delta, .altitudeM = altitudeM},
    };
    _marineContext->addTask(task);

    VisualMissionItem* visualItem = _missionController->insertComplexMissionItem(
        CoverageInspectionComplexItem::canonicalName, mapCenterCoord, -1, true);
    auto* coverageItem = qobject_cast<CoverageInspectionComplexItem*>(visualItem);
    if (coverageItem == nullptr) {
        _marineContext->removeTask(task.id);
        return;
    }

    coverageItem->setTaskId(QString::fromStdString(task.id));
}
