#pragma once

#include "PlanningInputIdentity.h"
#include "PlanningResult.h"

namespace Marine {

// Persistence data only. V05-08 must provide execution certification separately.
struct InfrastructurePlanningArtifact
{
    PlanningResult result;
    PlanningInputIdentity identity;
    bool stale = false;
};

}  // namespace Marine
