#pragma once

#include <span>
#include <string>
#include <string_view>

#include "CoverageQualityPolicy.h"
#include "Geometry/GeometryTypes.h"
#include "Geometry/PolygonRegion.h"
#include "MarineTask.h"
#include "PathLegRole.h"
#include "PlannerSource.h"

namespace Marine {

enum class CoverageQualityStatus
{
    Complete,
    Acceptable,
    Insufficient,
    AssessmentError,
};

enum class CoverageQualityError
{
    None,
    InvalidTarget,
    InvalidPath,
    InvalidSwathWidth,
    InvalidRequirement,
    InvalidStrategy,
    UnsupportedPolicySemantics,
    GeometryFailure,
    NumericalFailure,
};

enum class CoverageQualityComparison
{
    Better,
    Equivalent,
    Worse,
    NotComparable,
};

template <typename RegionSet>
struct CoverageResidualGeometry
{
    RegionSet criticalCoverageCore;
    RegionSet uncoveredRegion;
    RegionSet criticalUncoveredRegion;
    RegionSet boundaryShortfallRegion;
    RegionSet strictFallbackTargetComponents;
};

struct CoverageQualityAvailability
{
    bool targetArea = false;
    bool coveredArea = false;
    bool uncoveredArea = false;
    bool ratio = false;
    bool criticalUncoveredArea = false;
    bool numericalTolerance = false;
    bool criticalCore = false;
    bool uncovered = false;
    bool criticalUncovered = false;
    bool boundaryShortfall = false;
    bool fallbackComponents = false;

    bool operator==(const CoverageQualityAvailability&) const = default;
};

template <typename RegionSet>
struct CoverageEvaluation
{
    CoverageQualityStatus status = CoverageQualityStatus::AssessmentError;
    CoverageQualityError error = CoverageQualityError::None;
    PlannerStrategyIdentity strategy;
    CoverageRequirement requirement = CoverageRequirement::Standard;
    std::string policySemanticVersion;
    double targetAreaM2 = 0.0;
    double coveredAreaM2 = 0.0;
    double uncoveredAreaM2 = 0.0;
    double coverageRatio = 0.0;
    double criticalUncoveredAreaM2 = 0.0;
    double numericalToleranceM2 = 0.0;
    bool passesRequirement = false;
    bool strictFallbackTriggered = false;
    bool wholeTargetStrictFallback = false;
    CoverageQualityAvailability availability;
    CoverageResidualGeometry<RegionSet> residual;
    std::string message;
};

using CoverageQualityResidualGeometry = CoverageResidualGeometry<PolygonRegionSet2D>;
using CoverageQualityEvaluation = CoverageEvaluation<PolygonRegionSet2D>;
using GeoCoverageQualityEvaluation = CoverageEvaluation<GeoPolygonRegionSet>;

[[nodiscard]] CoverageQualityEvaluation evaluateCoverageQuality(
    const PolygonRegionSet2D& coverageTarget, std::span<const Point2D> path, std::span<const PathLegRole> legRoles,
    double swathWidthM, CoverageRequirement requirement, const PlannerStrategyIdentity& strategy,
    std::string_view policySemanticVersion = CoverageQualityPolicySemanticVersion);

[[nodiscard]] CoverageQualityComparison compareCoverageQuality(const CoverageQualityEvaluation& left,
                                                               const CoverageQualityEvaluation& right);

}  // namespace Marine
