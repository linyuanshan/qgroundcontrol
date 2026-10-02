#pragma once

#include "PlanningInputIdentity.h"
#include "PlanningResult.h"

namespace Marine {

enum class PlanningResultContract
{
    InfrastructureOnly,
    IntegratedV05
};

struct PlanningArtifact
{
    PlanningResult result;
    PlanningInputIdentity identity;
    bool stale = false;
    PlanningResultContract resultContract = PlanningResultContract::InfrastructureOnly;
};

// Existing transitional callers remain source-compatible, but the backend inspects resultContract.
using InfrastructurePlanningArtifact = PlanningArtifact;

}  // namespace Marine
