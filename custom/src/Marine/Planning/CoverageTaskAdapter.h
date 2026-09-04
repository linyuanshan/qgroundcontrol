#pragma once

#include <optional>

#include "CoveragePlanningProblem.h"
#include "Geometry/GeoReference.h"
#include "MarineTask.h"
#include "PlanningResult.h"

namespace Marine {

class CoverageTaskAdapter final
{
public:
    [[nodiscard]] static bool buildProblem(const MarineTask& task, CoveragePlanningProblem& problem,
                                           std::optional<GeoReference>& geoReference, CoveragePlanningError& error);

    [[nodiscard]] static PlanningResult toPlanningResult(const CoveragePlanningSolution& solution,
                                                         const GeoReference& geoReference);
};

}  // namespace Marine
