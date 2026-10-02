#pragma once

#include <optional>

#include "BoundaryCoverageSupport.h"
#include "CoverageRepairData.h"
#include "CoverageSafety.h"

namespace Marine {

[[nodiscard]] bool coveragePolicyPass(const CoverageQualityEvaluation& quality);

/// §23 trial ordering; quality first, then actual route cost and stable ring/entry/direction keys.
[[nodiscard]] bool coverageRepairTrialBetter(const CoverageRepairStep& left, const CoverageRepairStep& right);

/// Internal trial primitive for complete components from generateCoverageRepairSupport().
/// Returns only a strictly improving, fully certified assembly; never mutates the retained candidate.
[[nodiscard]] std::optional<CoverageRepairStep> evaluateCoverageRepairTrial(
    const PolygonRegionSet2D& target, const PolygonRegionSet2D& active, const SafetyTrackRegionsResult& safety,
    double swath, CoverageRequirement requirement, const PlannerStrategyIdentity& strategy,
    const CoverageRepairCandidate& current, const BoundaryCoverageComponent& component, std::size_t entry,
    bool reverse);

/// Append complete target-relative components, one strictly improving, fully certified trial at a time.
/// Caller must suppress this stage for the whole initial collection if ANY initial candidate passes.
[[nodiscard]] CoverageRepairResult repairCoverageCandidate(const PolygonRegionSet2D& target,
                                                           const PolygonRegionSet2D& activeExecutionRegion,
                                                           const SafetyTrackRegionsResult& safetyRegions,
                                                           double swathWidthM, CoverageRequirement requirement,
                                                           const PlannerStrategyIdentity& strategy,
                                                           const CoverageRepairCandidate& initial);

}  // namespace Marine
