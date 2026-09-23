#include "BoustrophedonCoveragePlanner.h"

#include <cmath>
#include <utility>

#include "BoundaryCoverageSupport.h"
#include "BoustrophedonDecomposition.h"
#include "CellCoverage.h"
#include "ComplexCoverageAssembly.h"
#include "CoverageFreeSpace.h"
#include "CoverageProblemValidator.h"
#include "Geometry/MarineGeometry.h"
#include "GlobalSweepSelector.h"
#include "GreedyCellOrdering.h"
#include "NominalCoverageValidator.h"

namespace {

Marine::CoveragePlanningSolution failure(Marine::PlanningStatus status, Marine::CoveragePlanningError error,
                                         std::string message)
{
    Marine::CoveragePlanningSolution solution;
    solution.status = status;
    solution.error = error;
    solution.message = message.empty() ? Marine::CoverageProblemValidator::messageForError(error) : std::move(message);
    return solution;
}

Marine::CoveragePlanningSolution failure(Marine::CoveragePlanningError error, std::string message = {})
{
    return failure(Marine::CoverageProblemValidator::statusForError(error), error, std::move(message));
}

}  // namespace

namespace Marine {

std::string BoustrophedonCoveragePlanner::id() const
{
    return "marine.coverage.bcd";
}

std::string BoustrophedonCoveragePlanner::displayName() const
{
    return "Boustrophedon Coverage Planner";
}

CoveragePlanningSolution BoustrophedonCoveragePlanner::plan(const CoveragePlanningProblem& problem) const
{
    CoveragePlanningProblem normalizedProblem = problem;
    const CoveragePlanningError validationError = CoverageProblemValidator::validateAndNormalize(normalizedProblem);
    if (validationError != CoveragePlanningError::None) {
        return failure(validationError);
    }

    const CoverageFreeSpaceResult freeSpace = buildCoverageFreeSpace(normalizedProblem);
    if (freeSpace.status != PlanningStatus::Success) {
        return failure(freeSpace.status, freeSpace.error, freeSpace.message);
    }

    double selectedSweepAngleDeg = normalizedProblem.requestedSweepAngleDeg;
    if (normalizedProblem.sweepAngleMode == SweepAngleMode::Auto) {
        const GlobalSweepSelectionResult selection =
            selectGlobalSweepAngle(normalizedProblem.region.outerBoundary,
                                   freeSpace.freeSpace.executionTrackFeasibleRegion, normalizedProblem.swathWidthM);
        if (selection.status != PlanningStatus::Success) {
            return failure(selection.status, selection.error, selection.message);
        }
        selectedSweepAngleDeg = selection.selectedSweepAngleDeg;
    }

    const CoverageDecompositionResult decomposition =
        decomposeBoustrophedon(freeSpace.freeSpace.executionTrackFeasibleRegion, selectedSweepAngleDeg);
    if (decomposition.status != PlanningStatus::Success) {
        return failure(decomposition.status, decomposition.error, decomposition.message);
    }

    const CellCoverageGenerationResult cellCoverage =
        generateCellCoverage(decomposition.cells, normalizedProblem.swathWidthM, selectedSweepAngleDeg);
    if (cellCoverage.status != PlanningStatus::Success) {
        return failure(cellCoverage.status, cellCoverage.error, cellCoverage.message);
    }

    const BoundaryCoverageSupportResult boundarySupport =
        generateBoundaryCoverageSupport(freeSpace.freeSpace.executionTrackFeasibleRegion);
    if (boundarySupport.status != PlanningStatus::Success) {
        return failure(boundarySupport.status, boundarySupport.error, boundarySupport.message);
    }

    const CellOrderingResult ordering = orderCellTraversals(freeSpace.freeSpace.executionTrackFeasibleRegion,
                                                            cellCoverage.cells, cellCoverage.traversalStates);
    if (ordering.status != PlanningStatus::Success) {
        return failure(ordering.status, ordering.error, ordering.message);
    }

    ComplexCoverageAssemblyResult assembly =
        assembleComplexCoverage(freeSpace.freeSpace.executionTrackFeasibleRegion, boundarySupport.components,
                                cellCoverage.cells, ordering.visits);
    if (assembly.status != PlanningStatus::Success) {
        return failure(assembly.status, assembly.error, assembly.message);
    }
    for (std::size_t index = 1; index < assembly.path.size(); ++index) {
        if (!Geometry::segmentInsidePolygonRegion(freeSpace.freeSpace.executionTrackFeasibleRegion,
                                                  assembly.path[index - 1], assembly.path[index])) {
            return failure(CoveragePlanningError::InvalidGeneratedPath,
                           "Coverage path leaves the execution-safe region");
        }
    }

    const PolygonRegionSet2D coverageTarget{freeSpace.freeSpace.coverageTarget};
    const CoverageCompletenessResult completeness =
        validateNominalCoverage(coverageTarget, assembly.path, assembly.legRoles, normalizedProblem.swathWidthM);
    if (completeness.status != PlanningStatus::Success) {
        return failure(completeness.status, completeness.error, completeness.message);
    }

    const bool validMetrics = std::isfinite(assembly.coverageLengthM) && std::isfinite(assembly.transitLengthM) &&
                              std::isfinite(assembly.pathLengthM) && (assembly.coverageLengthM >= 0.0) &&
                              (assembly.transitLengthM >= 0.0) &&
                              (std::abs(assembly.coverageLengthM + assembly.transitLengthM - assembly.pathLengthM) <=
                               Geometry::LengthEpsilonM);
    if ((selectedSweepAngleDeg < 0.0) || (selectedSweepAngleDeg >= 180.0) || !validMetrics ||
        (assembly.path.size() < 2) || (assembly.legRoles.size() != assembly.path.size() - 1) ||
        std::cmp_not_equal(assembly.cellCount, decomposition.cells.size())) {
        return failure(CoveragePlanningError::InvalidGeneratedPath,
                       "Boustrophedon coverage pipeline produced inconsistent output");
    }

    CoveragePlanningSolution solution;
    solution.status = PlanningStatus::Success;
    solution.path = std::move(assembly.path);
    solution.legRoles = std::move(assembly.legRoles);
    solution.coverageLengthM = assembly.coverageLengthM;
    solution.transitLengthM = assembly.transitLengthM;
    solution.pathLengthM = assembly.pathLengthM;
    solution.selectedSweepAngleDeg = selectedSweepAngleDeg;
    solution.cellCount = assembly.cellCount;
    solution.turnCount = assembly.turnCount;
    solution.error = CoveragePlanningError::None;
    solution.message = "Boustrophedon coverage path generated and nominally complete";
    return solution;
}

}  // namespace Marine
