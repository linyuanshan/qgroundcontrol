#pragma once

#include <optional>
#include <span>

#include "CoveragePlanningProblem.h"

namespace Marine {

struct SafetyTrackRegions
{
    PolygonRegionSet2D nominalHardTrackRegion;
    PolygonRegionSet2D hardExecutionTrackRegion;
    PolygonRegionSet2D preferredExecutionTrackRegion;
};

struct SafetyTrackRegionsResult
{
    CoveragePlanningError error = CoveragePlanningError::GeometryFailure;
    SafetyTrackRegions regions;
};

enum class SafetySolutionTier
{
    D0,
    D1,
};

enum class SafetyLegClass
{
    PreferredSafe,
    HardSafeWarning,
};

struct SafetyCandidateAssessment
{
    CoveragePlanningError error = CoveragePlanningError::InvalidGeneratedPath;
    std::optional<SafetySolutionTier> tier;
    std::vector<SafetyLegClass> legs;
};

struct SafetyCandidateSelection
{
    SafetyCandidateAssessment assessment;
    std::vector<Point2D> path;
    bool usedHardFallback = false;
};

[[nodiscard]] CoveragePlanningError validateSafetyMargins(const SafetyConfig& safety,
                                                          const ExecutionSafetyProfile& executionSafety);
/// Empty sets are successful geometry results; failures never expose partially built regions.
[[nodiscard]] SafetyTrackRegionsResult buildSafetyTrackRegions(const Region2D& region, const SafetyConfig& safety,
                                                               const ExecutionSafetyProfile& executionSafety);
/// Geometric D0/D1 certification only. Does not imply coverage quality, readiness or upload permission.
[[nodiscard]] SafetyCandidateAssessment evaluateSafetyCandidate(const SafetyTrackRegionsResult& regions,
                                                                std::span<const Point2D> path);
/// V05-03 safety-only fallback between supplied paths. Not the §24 final candidate comparator:
/// coverage certification and quality must be ranked before preferred safety.
[[nodiscard]] SafetyCandidateSelection selectPreferredOrHardCandidate(const SafetyTrackRegionsResult& regions,
                                                                      std::span<const Point2D> preferredCandidate,
                                                                      std::span<const Point2D> hardCandidate);

}  // namespace Marine
