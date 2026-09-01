#pragma once

#include <QtCore/QPointer>

#include "PlanCreator.h"

namespace Marine {
class MarinePlanContext;
}

class CoverageInspectionPlanCreator final : public PlanCreator
{
    Q_OBJECT

public:
    CoverageInspectionPlanCreator(PlanMasterController* planMasterController, Marine::MarinePlanContext* marineContext);

    Q_INVOKABLE void createPlan(const QGeoCoordinate& mapCenterCoord) final;

private:
    QPointer<Marine::MarinePlanContext> _marineContext;
};
