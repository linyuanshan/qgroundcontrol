#pragma once

#include <string>

#include "CoveragePlanningProblem.h"

namespace Marine {

class CoverageProblemValidator final
{
public:
    [[nodiscard]] static CoveragePlanningError validateAndNormalize(CoveragePlanningProblem& problem);
    /// Historical planners support coincident C/N only; this is a capability gate, not v0.5 topology validation.
    [[nodiscard]] static CoveragePlanningError validateLegacyCoincidentBoundaries(const Region2D& region);
    [[nodiscard]] static PlanningStatus statusForError(CoveragePlanningError error);
    [[nodiscard]] static std::string messageForError(CoveragePlanningError error);
};

}  // namespace Marine
