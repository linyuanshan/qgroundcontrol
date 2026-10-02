#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "CoverageQualityEvaluator.h"
#include "PathLegRole.h"

namespace Marine {

enum class MissionReadiness
{
    None,
    Ready,
    ReadyWithWarning,
    ReviewRequired,
    DiagnosticOnly
};
enum class SafetySolutionTier
{
    D0,
    D1,
    D2,
    D3
};
enum class SafetyLegClass
{
    PreferredSafe,
    HardSafeWarning,
    ExecutionUnsafe,
    HardUnsafe
};
enum class PlanningIssueCode
{
    PlannerEscalated,
    PreferredSafetyViolated,
    HardSafetyUnavailable,
    ExecutionReserveUnavailable,
    UnsafeDiagnosticCandidate,
    CoverageBoundaryShortfall,
    CriticalCoverageGap,
    CoverageBelowRequirement,
    CoverageRepairApplied,
    CoverageRepairInsufficient,
    NavigationRegionDisconnected,
    NavigationRegionUnsupported,
    UnresolvedConnection,
    IngressNotAssessed,
    CoverageAssessmentFailed
};
enum class PlanningIssueSeverity
{
    Info,
    Warning,
    Blocking
};
enum class PlanningReferenceKind
{
    CanonicalPathLegRange,
    DiagnosticCandidateLegRange,
    DiagnosticOverlay,
    CoverageResidual
};
enum class PlanningResidualKind
{
    Uncovered,
    CriticalUncovered,
    BoundaryShortfall
};
enum class PlanningSuggestionCode
{
    ExpandNavigationArea,
    ReviewPreferredClearance,
    ReviewSwathWidth,
    IncreaseNavigationRoom,
    ReviewHardNavigationFeasibility,
    InspectRepairCost
};
enum class DiagnosticOverlayKind
{
    UnreachableTarget,
    DisconnectedNavigation,
    UnsupportedGeometry,
    UnresolvedConnection
};
enum class CoverageRepairReason
{
    InitialPolicyPass,
    AssessmentUnavailable,
    NoCanonicalCandidate,
    NoUsefulRepair,
    AppliedPolicyPass,
    AppliedStillInsufficient
};

struct PlanningReference
{
    PlanningReferenceKind kind = PlanningReferenceKind::CoverageResidual;
    std::size_t index = 0;
    std::size_t firstLeg = 0;
    std::size_t legCount = 0;
    PlanningResidualKind residual = PlanningResidualKind::Uncovered;
    bool operator==(const PlanningReference&) const = default;
};

struct PlanningIssue
{
    PlanningIssueCode code = PlanningIssueCode::IngressNotAssessed;
    PlanningIssueSeverity severity = PlanningIssueSeverity::Info;
    std::string message;
    std::optional<PlanningReference> reference;
    bool operator==(const PlanningIssue&) const = default;
};

struct PlanningSuggestion
{
    PlanningSuggestionCode code = PlanningSuggestionCode::ExpandNavigationArea;
    PlanningIssueCode issue = PlanningIssueCode::PreferredSafetyViolated;
    std::string message;
    std::optional<PlanningReference> reference;
    bool operator==(const PlanningSuggestion&) const = default;
};

template <typename Point>
struct DiagnosticCandidate
{
    std::string identity;
    std::vector<Point> path;
    std::vector<PathLegRole> legRoles;
    std::vector<SafetyLegClass> legAssessments;
    double coverageLengthM = 0.0;
    double transitLengthM = 0.0;
    double pathLengthM = 0.0;
    int turnCount = 0;
};

template <typename RegionSet>
struct DiagnosticOverlay
{
    DiagnosticOverlayKind kind = DiagnosticOverlayKind::UnresolvedConnection;
    RegionSet geometry;
    std::string explanation;
};

template <typename Point, typename RegionSet>
struct AppliedRepairComponent
{
    std::uint32_t componentId = 0;
    std::vector<Point> componentPath;
    std::size_t entryIndex = 0;
    bool reverse = false;
    double transitionCostM = 0.0;
    CoverageEvaluation<RegionSet> before;
    CoverageEvaluation<RegionSet> after;
    double pathLengthBeforeM = 0.0;
    double pathLengthAfterM = 0.0;
    int turnCountBefore = 0;
    int turnCountAfter = 0;
};

template <typename Point, typename RegionSet>
struct SelectedRepairProvenance
{
    bool attempted = false;
    bool applied = false;
    CoverageRepairReason reason = CoverageRepairReason::NoCanonicalCandidate;
    std::vector<AppliedRepairComponent<Point, RegionSet>> components;
};

// One metadata model for local planning and geographic publication. Conversion changes coordinates only.
template <typename Point, typename RegionSet>
struct PlanningOutcome
{
    MissionReadiness readiness = MissionReadiness::None;
    std::optional<SafetySolutionTier> tier;
    std::vector<SafetyLegClass> canonicalLegAssessments;
    std::optional<CoverageEvaluation<RegionSet>> coverageQuality;
    std::optional<std::size_t> selectedCandidateIndex;
    SelectedRepairProvenance<Point, RegionSet> repair;
    std::vector<PlanningIssue> issues;
    std::vector<PlanningSuggestion> suggestions;
    std::optional<DiagnosticCandidate<Point>> diagnosticCandidate;
    std::vector<DiagnosticOverlay<RegionSet>> diagnosticOverlays;
};

}  // namespace Marine
