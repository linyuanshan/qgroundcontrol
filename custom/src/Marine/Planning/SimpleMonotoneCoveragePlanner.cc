#include "SimpleMonotoneCoveragePlanner.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "CoverageGeometry.h"
#include "CoverageProblemValidator.h"
#include "CoverageQualityEvaluator.h"
#include "CoverageRepair.h"
#include "CoverageSafety.h"
#include "CoverageStrategySemantics.h"
#include "Geometry/MarineGeometry.h"
#include "Geometry/PolygonRegion.h"
#include "GlobalSweepSelector.h"
#include "IntegratedPlanningResult.h"
#include "PlanningPathMetrics.h"
#include "SimpleMonotoneCapability.h"
#include "StaticSafeRouter.h"

namespace Marine {
namespace {

struct LaneSegment
{
    double minimumXM = 0.0;
    double maximumXM = 0.0;
    double yM = 0.0;
};

struct Candidate
{
    std::vector<Point2D> path;
    std::vector<PathLegRole> legRoles;
    PlanningPathMetrics metrics;
    CoverageQualityEvaluation quality;
    bool preferredSafe = false;
    bool preferredTier = false;
    int orientationOrder = 0;
};

CoveragePlanningSolution failure(CoveragePlanningError error, std::string message = {},
                                 std::optional<PlannerSourceInfo> source = std::nullopt)
{
    CoveragePlanningSolution result;
    result.status = CoverageProblemValidator::statusForError(error);
    result.error = error;
    result.message = message.empty() ? CoverageProblemValidator::messageForError(error) : std::move(message);
    result.plannerSource = std::move(source);
    if (result.plannerSource) {
        result.selectedSweepAngleDeg = result.plannerSource->selectedSweepAngleDeg;
    }
    return result;
}

bool samePoint(const Point2D& first, const Point2D& second)
{
    return first.xM == second.xM && first.yM == second.yM;
}

PolygonRegionSet2D toSweepFrame(const PolygonRegionSet2D& regions, double mathAngleDeg)
{
    PolygonRegionSet2D result;
    result.reserve(regions.size());
    for (const PolygonRegion2D& region : regions) {
        PolygonRegion2D transformed{.outerBoundary = Geometry::toSweepFrame(region.outerBoundary, mathAngleDeg)};
        transformed.holes.reserve(region.holes.size());
        for (const Polygon2D& hole : region.holes) {
            transformed.holes.push_back(Geometry::toSweepFrame(hole, mathAngleDeg));
        }
        result.push_back(std::move(transformed));
    }
    return result;
}

std::optional<std::vector<double>> buildLaneSchedule(const PolygonRegionSet2D& target, double swathWidthM)
{
    if (target.size() != 1 || !target.front().holes.empty() || target.front().outerBoundary.vertices.size() < 3) {
        return std::nullopt;
    }
    double minimumYM = std::numeric_limits<double>::infinity();
    double maximumYM = -std::numeric_limits<double>::infinity();
    for (const Point2D& point : target.front().outerBoundary.vertices) {
        minimumYM = std::min(minimumYM, point.yM);
        maximumYM = std::max(maximumYM, point.yM);
    }
    if (!std::isfinite(minimumYM) || !std::isfinite(maximumYM) || maximumYM <= minimumYM ||
        !std::isfinite(swathWidthM) || swathWidthM <= 0.0) {
        return std::nullopt;
    }

    const double halfSwathM = swathWidthM / 2.0;
    const double firstYM = minimumYM + halfSwathM;
    const double lastYM = maximumYM - halfSwathM;
    if (lastYM <= firstYM + Geometry::LengthEpsilonM) {
        return std::vector<double>{(minimumYM + maximumYM) / 2.0};
    }

    const double intervalCountValue = std::ceil((lastYM - firstYM) / swathWidthM);
    if (!std::isfinite(intervalCountValue) || intervalCountValue < 1.0 ||
        intervalCountValue >= static_cast<double>(std::numeric_limits<int>::max())) {
        return std::nullopt;
    }
    const auto intervalCount = static_cast<std::size_t>(intervalCountValue);
    const double spacingM = (lastYM - firstYM) / intervalCountValue;
    std::vector<double> lanes;
    lanes.reserve(intervalCount + 1);
    for (std::size_t index = 0; index <= intervalCount; ++index) {
        lanes.push_back(firstYM + static_cast<double>(index) * spacingM);
    }
    return lanes;
}

std::optional<std::vector<LaneSegment>> buildLaneSegments(const PolygonRegionSet2D& targetSweep,
                                                          const PolygonRegionSet2D& activeTrackSweep,
                                                          const std::vector<double>& lanePositionsYM,
                                                          CoveragePlanningError& error)
{
    std::vector<LaneSegment> lanes;
    lanes.reserve(lanePositionsYM.size());
    for (const double laneY : lanePositionsYM) {
        const Geometry::ScanlineResult target = Geometry::intersectScanlineForValidatedGeometry(targetSweep, laneY);
        if (target.status != Geometry::ScanlineStatus::Success || target.intervals.size() != 1) {
            error = target.status == Geometry::ScanlineStatus::InvalidInput ||
                            target.status == Geometry::ScanlineStatus::GeometryFailure
                        ? CoveragePlanningError::GeometryFailure
                        : CoveragePlanningError::InvalidGeneratedPath;
            return std::nullopt;
        }

        const Geometry::ScanlineResult feasible =
            Geometry::intersectScanlineForValidatedGeometry(activeTrackSweep, laneY);
        if (feasible.status == Geometry::ScanlineStatus::InvalidInput ||
            feasible.status == Geometry::ScanlineStatus::GeometryFailure) {
            error = CoveragePlanningError::GeometryFailure;
            return std::nullopt;
        }
        if (feasible.status == Geometry::ScanlineStatus::NoIntersection) {
            error = CoveragePlanningError::CoverageImpossibleWithExecutionMargin;
            return std::nullopt;
        }

        const Geometry::ScanlineInterval& targetInterval = target.intervals.front();
        bool foundIntersection = false;
        LaneSegment best;
        double bestLengthM = -1.0;
        for (const Geometry::ScanlineInterval& feasibleInterval : feasible.intervals) {
            const double minimumXM = std::max(targetInterval.minimumXM, feasibleInterval.minimumXM);
            const double maximumXM = std::min(targetInterval.maximumXM, feasibleInterval.maximumXM);
            const double overlapM = maximumXM - minimumXM;
            if (overlapM <= 0.0) {
                continue;
            }
            const bool longer = overlapM > bestLengthM;
            const bool stableTie =
                overlapM == bestLengthM &&
                (minimumXM < best.minimumXM || (minimumXM == best.minimumXM && maximumXM < best.maximumXM));
            if (!foundIntersection || longer || stableTie) {
                best = {.minimumXM = minimumXM, .maximumXM = maximumXM, .yM = laneY};
                bestLengthM = overlapM;
                foundIntersection = true;
            }
        }
        if (!foundIntersection) {
            error = CoveragePlanningError::CoverageImpossibleWithExecutionMargin;
            return std::nullopt;
        }
        lanes.push_back(best);
    }
    error = CoveragePlanningError::None;
    return lanes;
}

bool appendLeg(std::vector<Point2D>& path, std::vector<PathLegRole>& roles, const Point2D& point, PathLegRole role)
{
    if (!point.isFinite()) {
        return false;
    }
    if (path.empty()) {
        path.push_back(point);
        return true;
    }
    if (samePoint(path.back(), point)) {
        return true;
    }
    path.push_back(point);
    roles.push_back(role);
    return true;
}

std::optional<Candidate> buildCandidate(const std::vector<LaneSegment>& lanes,
                                        const PolygonRegionSet2D& activeTrackRegion, double mathAngleDeg,
                                        bool startForward, int orientationOrder, CoveragePlanningError& error)
{
    Candidate candidate;
    candidate.orientationOrder = orientationOrder;
    for (std::size_t laneIndex = 0; laneIndex < lanes.size(); ++laneIndex) {
        const LaneSegment& lane = lanes[laneIndex];
        const bool minimumToMaximum = ((laneIndex % 2) == 0) == startForward;
        const Point2D sweepEntry{.xM = minimumToMaximum ? lane.minimumXM : lane.maximumXM, .yM = lane.yM};
        const Point2D sweepExit{.xM = minimumToMaximum ? lane.maximumXM : lane.minimumXM, .yM = lane.yM};
        const Point2D entry = Geometry::fromSweepFrame(sweepEntry, mathAngleDeg);
        const Point2D exit = Geometry::fromSweepFrame(sweepExit, mathAngleDeg);

        if (laneIndex == 0) {
            candidate.path.push_back(entry);
        } else {
            const StaticRoute route = routeStatic(activeTrackRegion, candidate.path.back(), entry);
            if (route.status != PlanningStatus::Success || route.error != CoveragePlanningError::None ||
                route.path.size() < 2) {
                error = route.error == CoveragePlanningError::None ? CoveragePlanningError::SafeTransitNotFound
                                                                   : route.error;
                return std::nullopt;
            }
            for (std::size_t routeIndex = 1; routeIndex < route.path.size(); ++routeIndex) {
                if (!appendLeg(candidate.path, candidate.legRoles, route.path[routeIndex], PathLegRole::Transit)) {
                    error = CoveragePlanningError::InvalidGeneratedPath;
                    return std::nullopt;
                }
            }
            if (!samePoint(candidate.path.back(), entry)) {
                error = CoveragePlanningError::InvalidGeneratedPath;
                return std::nullopt;
            }
        }
        if (!appendLeg(candidate.path, candidate.legRoles, exit, PathLegRole::Coverage)) {
            error = CoveragePlanningError::InvalidGeneratedPath;
            return std::nullopt;
        }
    }

    const auto metrics = calculatePlanningPathMetrics(candidate.path, candidate.legRoles);
    if (!metrics) {
        error = CoveragePlanningError::InvalidGeneratedPath;
        return std::nullopt;
    }
    candidate.metrics = *metrics;
    error = CoveragePlanningError::None;
    return candidate;
}

int qualityCategory(const CoverageQualityEvaluation& quality)
{
    if (quality.status == CoverageQualityStatus::Insufficient) {
        return 1;
    }
    if ((quality.status == CoverageQualityStatus::Complete || quality.status == CoverageQualityStatus::Acceptable) &&
        quality.passesRequirement) {
        return 2;
    }
    return 0;
}

bool candidateIsBetter(const Candidate& candidate, const Candidate& current)
{
    const int candidateCategory = qualityCategory(candidate.quality);
    const int currentCategory = qualityCategory(current.quality);
    if (candidateCategory != currentCategory) {
        return candidateCategory > currentCategory;
    }
    if (candidateCategory > 0) {
        const CoverageQualityComparison comparison = compareCoverageQuality(candidate.quality, current.quality);
        if (comparison == CoverageQualityComparison::Better) {
            return true;
        }
        if (comparison == CoverageQualityComparison::Worse || comparison == CoverageQualityComparison::NotComparable) {
            return false;
        }
    }
    if (candidate.preferredSafe != current.preferredSafe) {
        return candidate.preferredSafe;
    }
    if (candidate.metrics.pathLengthM != current.metrics.pathLengthM) {
        return candidate.metrics.pathLengthM < current.metrics.pathLengthM;
    }
    if (candidate.metrics.turnCount != current.metrics.turnCount) {
        return candidate.metrics.turnCount < current.metrics.turnCount;
    }
    return candidate.orientationOrder < current.orientationOrder;
}

PlannerSourceInfo directSource(const SimpleMonotoneCoveragePlanner& planner, SweepAngleMode requestedMode,
                               double selectedAngleDeg)
{
    return {.requestedPlannerId = planner.id(),
            .resolvedStrategy = {.strategyId = planner.id(), .semanticVersion = planner.semanticVersion()},
            .resolutionStatus = PlannerResolutionStatus::Resolved,
            .resolutionReason = PlannerResolutionReason::None,
            .escalated = false,
            .requestedSweepMode = requestedMode,
            .selectedSweepAngleDeg = selectedAngleDeg,
            .sweepSemanticVersion =
                requestedMode == SweepAngleMode::Auto ? CoverageStrategySemantics::GlobalSweepVersion : ""};
}

}  // namespace

std::string SimpleMonotoneCoveragePlanner::id() const
{
    return CoverageStrategySemantics::SimpleMonotoneId;
}

std::string SimpleMonotoneCoveragePlanner::semanticVersion() const
{
    return CoverageStrategySemantics::SimpleMonotoneVersion;
}

std::string SimpleMonotoneCoveragePlanner::displayName() const
{
    return "Simple Monotone Coverage Planner";
}

CoveragePlanningSolution SimpleMonotoneCoveragePlanner::plan(const CoveragePlanningProblem& problem) const
{
    CoveragePlanningProblem normalized = problem;
    const CoveragePlanningError validationError = CoverageProblemValidator::validateAndNormalize(normalized);
    if (validationError != CoveragePlanningError::None) {
        return failure(validationError);
    }
    const CoverageGeometryResult coverageGeometry = buildCoverageGeometry(normalized.region);
    if (coverageGeometry.error != CoveragePlanningError::None) {
        auto result = failure(coverageGeometry.error);
        publishUnresolvedOutcome(result, {}, {}, false);
        return result;
    }
    const SafetyTrackRegionsResult safetyRegions =
        buildSafetyTrackRegions(normalized.region, normalized.safety, normalized.executionSafety);
    if (safetyRegions.error != CoveragePlanningError::None) {
        auto result = failure(safetyRegions.error);
        publishUnresolvedOutcome(result, coverageGeometry.geometry.coverageTarget,
                                 coverageGeometry.geometry.rawNavigationFreeSpace, false);
        return result;
    }

    double selectedAngleDeg = normalized.requestedSweepAngleDeg;
    if (normalized.sweepAngleMode == SweepAngleMode::Auto) {
        const GlobalSweepSelectionResult selection = selectGlobalSweepAngle(
            normalized.region.coverageBoundary,
            (safetyRegions.regions.hardExecutionTrackRegion.empty() ? coverageGeometry.geometry.rawNavigationFreeSpace
                                                                    : safetyRegions.regions.hardExecutionTrackRegion),
            normalized.swathWidthM);
        if (selection.status != PlanningStatus::Success) {
            auto result = failure(selection.error, selection.message);
            publishUnresolvedOutcome(result, coverageGeometry.geometry.coverageTarget,
                                     coverageGeometry.geometry.rawNavigationFreeSpace, false);
            return result;
        }
        selectedAngleDeg = selection.selectedSweepAngleDeg;
    }
    const PlannerSourceInfo source = directSource(*this, problem.sweepAngleMode, selectedAngleDeg);
    const SimpleMonotoneCapabilityResult capability =
        assessSimpleMonotoneCapability(coverageGeometry.geometry.coverageTarget, selectedAngleDeg);
    if (!capability.applicable) {
        auto result = failure(CoveragePlanningError::UnsupportedStrategyCapability, capability.message, source);
        publishUnresolvedOutcome(result, coverageGeometry.geometry.coverageTarget,
                                 coverageGeometry.geometry.rawNavigationFreeSpace, true);
        return result;
    }

    const double mathAngleDeg = Geometry::navigationAngleToMathAngle(selectedAngleDeg);
    const PolygonRegionSet2D targetSweep = toSweepFrame(coverageGeometry.geometry.coverageTarget, mathAngleDeg);
    const auto lanePositions = buildLaneSchedule(targetSweep, normalized.swathWidthM);
    if (!lanePositions) {
        auto result =
            failure(CoveragePlanningError::GeometryFailure, "SimpleMonotone lane schedule is invalid", source);
        publishUnresolvedOutcome(result, coverageGeometry.geometry.coverageTarget,
                                 coverageGeometry.geometry.rawNavigationFreeSpace, false);
        return result;
    }

    const PlannerStrategyIdentity strategy{.strategyId = id(), .semanticVersion = semanticVersion()};
    std::vector<Candidate> candidates;
    CoveragePlanningError lastHardError = CoveragePlanningError::CoverageImpossibleWithExecutionMargin;
    const auto generateTier = [&](const PolygonRegionSet2D& activeTrack, bool preferredTier) {
        if (activeTrack.empty()) {
            if (!preferredTier) {
                lastHardError = CoveragePlanningError::NoNavigableArea;
            }
            return;
        }
        const PolygonRegionSet2D activeSweep = toSweepFrame(activeTrack, mathAngleDeg);
        CoveragePlanningError generationError = CoveragePlanningError::None;
        const auto lanes = buildLaneSegments(targetSweep, activeSweep, *lanePositions, generationError);
        if (!lanes) {
            if (!preferredTier) {
                lastHardError = generationError;
            }
            return;
        }
        for (int orientationOrder = 0; orientationOrder < 2; ++orientationOrder) {
            CoveragePlanningError candidateError = CoveragePlanningError::None;
            auto candidate = buildCandidate(*lanes, activeTrack, mathAngleDeg, orientationOrder == 0, orientationOrder,
                                            candidateError);
            if (!candidate) {
                if (!preferredTier) {
                    lastHardError = candidateError;
                }
                continue;
            }
            const SafetyCandidateAssessment safety = evaluateSafetyCandidate(safetyRegions, candidate->path);
            if (safety.error != CoveragePlanningError::None) {
                if (!preferredTier) {
                    lastHardError = safety.error;
                }
                continue;
            }
            candidate->preferredSafe = safety.tier == SafetySolutionTier::D0;
            candidate->preferredTier = preferredTier;
            candidate->quality =
                evaluateCoverageQuality(coverageGeometry.geometry.coverageTarget, candidate->path, candidate->legRoles,
                                        normalized.swathWidthM, normalized.coverageRequirement, strategy);
            candidates.push_back(std::move(*candidate));
        }
    };

    generateTier(safetyRegions.regions.preferredExecutionTrackRegion, true);
    generateTier(safetyRegions.regions.hardExecutionTrackRegion, false);
    if (candidates.empty()) {
        auto diagnostic = failure(lastHardError, {}, source);
        diagnostic.selectedSweepAngleDeg = selectedAngleDeg;
        const auto& raw = coverageGeometry.geometry.rawNavigationFreeSpace;
        const auto rawLanes =
            buildLaneSegments(targetSweep, toSweepFrame(raw, mathAngleDeg), *lanePositions, lastHardError);
        std::optional<Candidate> rawDiagnostic;
        if (rawLanes) {
            for (int orientation = 0; orientation < 2; ++orientation) {
                auto rawCandidate =
                    buildCandidate(*rawLanes, raw, mathAngleDeg, orientation == 0, orientation, lastHardError);
                if (!rawCandidate) {
                    continue;
                }
                const auto certification = evaluateSafetyCandidate(safetyRegions, rawCandidate->path);
                if (certification.error == CoveragePlanningError::None) {
                    // A newly discovered hard-safe route enters the existing repair and final ranking below.
                    rawCandidate->preferredSafe = certification.tier == SafetySolutionTier::D0;
                    rawCandidate->quality = evaluateCoverageQuality(
                        coverageGeometry.geometry.coverageTarget, rawCandidate->path, rawCandidate->legRoles,
                        normalized.swathWidthM, normalized.coverageRequirement, strategy);
                    candidates.push_back(std::move(*rawCandidate));
                } else if (!rawDiagnostic || rawCandidate->metrics.pathLengthM < rawDiagnostic->metrics.pathLengthM) {
                    rawDiagnostic = std::move(rawCandidate);
                }
            }
        }
        if (candidates.empty()) {
            if (rawDiagnostic && publishDiagnosticOutcome(diagnostic, safetyRegions, raw, rawDiagnostic->path,
                                                          rawDiagnostic->legRoles)) {
                return diagnostic;
            }
            publishUnresolvedOutcome(diagnostic, coverageGeometry.geometry.coverageTarget, raw, false);
            return diagnostic;
        }
    }

    const bool initialPass = std::ranges::any_of(
        candidates, [](const Candidate& candidate) { return coveragePolicyPass(candidate.quality); });
    std::vector<CoverageRepairResult> repairCandidates;
    std::vector<PlanningPathMetrics> initialMetrics;
    for (Candidate& candidate : candidates) {
        initialMetrics.push_back(candidate.metrics);
        CoverageRepairCandidate initial{candidate.path, candidate.legRoles, candidate.metrics, candidate.quality,
                                        candidate.preferredSafe};
        CoverageRepairResult repair{.candidate = initial};
        if (!initialPass) {
            const auto& active = candidate.preferredTier ? safetyRegions.regions.preferredExecutionTrackRegion
                                                         : safetyRegions.regions.hardExecutionTrackRegion;
            repair = repairCoverageCandidate(coverageGeometry.geometry.coverageTarget, active, safetyRegions,
                                             normalized.swathWidthM, normalized.coverageRequirement, strategy, initial);
            candidate.path = repair.candidate.path;
            candidate.legRoles = repair.candidate.legRoles;
            candidate.metrics = repair.candidate.metrics;
            candidate.quality = repair.candidate.quality;
            candidate.preferredSafe = repair.candidate.preferredSafe;
        }
        repairCandidates.push_back(std::move(repair));
    }

    const auto best = std::ranges::min_element(
        candidates, [](const Candidate& left, const Candidate& right) { return candidateIsBetter(left, right); });
    CoveragePlanningSolution result;
    result.repairCandidates = std::move(repairCandidates);
    result.plannerSource = source;
    result.outcome.coverageQuality = best->quality;
    result.selectedSweepAngleDeg = selectedAngleDeg;
    result.cellCount = 1;
    result.turnCount = best->metrics.turnCount;
    result.path = best->path;
    result.legRoles = best->legRoles;
    result.coverageLengthM = best->metrics.coverageLengthM;
    result.transitLengthM = best->metrics.transitLengthM;
    result.pathLengthM = best->metrics.pathLengthM;
    result.message = best->quality.message;
    const auto selectedIndex = static_cast<std::size_t>(best - candidates.begin());
    publishCanonicalOutcome(result, evaluateSafetyCandidate(safetyRegions, best->path), selectedIndex,
                            result.repairCandidates[selectedIndex], initialMetrics[selectedIndex], initialPass);
    return result;
}

}  // namespace Marine
