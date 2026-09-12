#pragma once

#include <string>

#include "CoveragePlanningProblem.h"

namespace Marine {

class CoverageProblemValidator final
{
public:
    [[nodiscard]] static CoveragePlanningError validateAndNormalize(CoveragePlanningProblem& problem);
    [[nodiscard]] static PlanningStatus statusForError(CoveragePlanningError error);
    [[nodiscard]] static std::string messageForError(CoveragePlanningError error);
};

}  // namespace Marine
