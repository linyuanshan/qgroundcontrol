#pragma once

#include <span>
#include <string>
#include <string_view>

#include "CoverageQualityPolicy.h"
#include "Geometry/GeometryTypes.h"
#include "Geometry/PolygonRegion.h"
#include "MarineTask.h"
#include "PathLegRole.h"

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

struct CoverageQualityResidualGeometry
{
    PolygonRegionSet2D criticalCoverageCore;
    PolygonRegionSet2D uncoveredRegion;
    PolygonRegionSet2D criticalUncoveredRegion;
    PolygonRegionSet2D boundaryShortfallRegion;
    PolygonRegionSet2D strictFallbackTargetComponents;
};

struct CoverageQualityEvaluation
{
    CoverageQualityStatus status = CoverageQualityStatus::AssessmentError;
    CoverageQualityError error = CoverageQualityError::None;
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
    CoverageQualityResidualGeometry residual;
    std::string message;
};

[[nodiscard]] CoverageQualityEvaluation evaluateCoverageQuality(
    const PolygonRegionSet2D& coverageTarget, std::span<const Point2D> path, std::span<const PathLegRole> legRoles,
    double swathWidthM, CoverageRequirement requirement,
    std::string_view policySemanticVersion = CoverageQualityPolicySemanticVersion);

[[nodiscard]] CoverageQualityComparison compareCoverageQuality(const CoverageQualityEvaluation& left,
                                                               const CoverageQualityEvaluation& right);

}  // namespace Marine
