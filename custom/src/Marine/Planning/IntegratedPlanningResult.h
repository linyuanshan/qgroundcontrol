#pragma once

#include "CoverageSafety.h"

namespace Marine {

/// Publish an already selected, independently hard-certified candidate. Does not rank, evaluate or repair.
void publishCanonicalOutcome(CoveragePlanningSolution& result, const SafetyCandidateAssessment& safety,
                             std::size_t selectedIndex, const CoverageRepairResult& repair,
                             const PlanningPathMetrics& initialMetrics, bool initialPolicyPass);
/// Rebuild deterministic issues/suggestions after Auto supplies the final source provenance.
void populatePlanningAdvice(CoveragePlanningSolution& result);
/// Publish a raw-space candidate only when at least one full leg violates hard execution clearance.
/// Returns false for a hard-safe raw candidate: callers must return it to normal certification/selection.
bool publishDiagnosticOutcome(CoveragePlanningSolution& result, const SafetyTrackRegionsResult& safety,
                              const PolygonRegionSet2D& raw, const std::vector<Point2D>& path,
                              const std::vector<PathLegRole>& roles);
void publishUnresolvedOutcome(CoveragePlanningSolution& result, const PolygonRegionSet2D& target,
                              const PolygonRegionSet2D& raw, bool unsupported);

}  // namespace Marine
