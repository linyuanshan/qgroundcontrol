#include "CoveragePlanningPresentation.h"

#include <QtCore/QCoreApplication>
#include <QtPositioning/QGeoCoordinate>
#include <QtPositioning/QGeoPolygon>
#include <QtPositioning/QGeoShape>

namespace Marine::QGC {
namespace {

#define DISPLAY_CASE(Type, Name) \
    case Type::Name:             \
        return QStringLiteral(#Name)

QString code(MissionReadiness value)
{
    switch (value) {
        DISPLAY_CASE(MissionReadiness, None);
        DISPLAY_CASE(MissionReadiness, Ready);
        DISPLAY_CASE(MissionReadiness, ReadyWithWarning);
        DISPLAY_CASE(MissionReadiness, ReviewRequired);
        DISPLAY_CASE(MissionReadiness, DiagnosticOnly);
    }
    return {};
}

QString code(SafetyLegClass value)
{
    switch (value) {
        DISPLAY_CASE(SafetyLegClass, PreferredSafe);
        DISPLAY_CASE(SafetyLegClass, HardSafeWarning);
        DISPLAY_CASE(SafetyLegClass, ExecutionUnsafe);
        DISPLAY_CASE(SafetyLegClass, HardUnsafe);
    }
    return {};
}

QString code(CoverageQualityStatus value)
{
    switch (value) {
        DISPLAY_CASE(CoverageQualityStatus, Complete);
        DISPLAY_CASE(CoverageQualityStatus, Acceptable);
        DISPLAY_CASE(CoverageQualityStatus, Insufficient);
        DISPLAY_CASE(CoverageQualityStatus, AssessmentError);
    }
    return {};
}

QString code(CoverageQualityError value)
{
    switch (value) {
        DISPLAY_CASE(CoverageQualityError, None);
        DISPLAY_CASE(CoverageQualityError, InvalidTarget);
        DISPLAY_CASE(CoverageQualityError, InvalidPath);
        DISPLAY_CASE(CoverageQualityError, InvalidSwathWidth);
        DISPLAY_CASE(CoverageQualityError, InvalidRequirement);
        DISPLAY_CASE(CoverageQualityError, InvalidStrategy);
        DISPLAY_CASE(CoverageQualityError, UnsupportedPolicySemantics);
        DISPLAY_CASE(CoverageQualityError, GeometryFailure);
        DISPLAY_CASE(CoverageQualityError, NumericalFailure);
    }
    return {};
}

QString code(PlanningIssueCode value)
{
    switch (value) {
        DISPLAY_CASE(PlanningIssueCode, PlannerEscalated);
        DISPLAY_CASE(PlanningIssueCode, PreferredSafetyViolated);
        DISPLAY_CASE(PlanningIssueCode, HardSafetyUnavailable);
        DISPLAY_CASE(PlanningIssueCode, ExecutionReserveUnavailable);
        DISPLAY_CASE(PlanningIssueCode, UnsafeDiagnosticCandidate);
        DISPLAY_CASE(PlanningIssueCode, CoverageBoundaryShortfall);
        DISPLAY_CASE(PlanningIssueCode, CriticalCoverageGap);
        DISPLAY_CASE(PlanningIssueCode, CoverageBelowRequirement);
        DISPLAY_CASE(PlanningIssueCode, CoverageRepairApplied);
        DISPLAY_CASE(PlanningIssueCode, CoverageRepairInsufficient);
        DISPLAY_CASE(PlanningIssueCode, NavigationRegionDisconnected);
        DISPLAY_CASE(PlanningIssueCode, NavigationRegionUnsupported);
        DISPLAY_CASE(PlanningIssueCode, UnresolvedConnection);
        DISPLAY_CASE(PlanningIssueCode, IngressNotAssessed);
        DISPLAY_CASE(PlanningIssueCode, CoverageAssessmentFailed);
    }
    return {};
}

QString code(PlanningSuggestionCode value)
{
    switch (value) {
        DISPLAY_CASE(PlanningSuggestionCode, ExpandNavigationArea);
        DISPLAY_CASE(PlanningSuggestionCode, ReviewPreferredClearance);
        DISPLAY_CASE(PlanningSuggestionCode, ReviewSwathWidth);
        DISPLAY_CASE(PlanningSuggestionCode, IncreaseNavigationRoom);
        DISPLAY_CASE(PlanningSuggestionCode, ReviewHardNavigationFeasibility);
        DISPLAY_CASE(PlanningSuggestionCode, InspectRepairCost);
    }
    return {};
}

QString code(PlanningReferenceKind value)
{
    switch (value) {
        DISPLAY_CASE(PlanningReferenceKind, CanonicalPathLegRange);
        DISPLAY_CASE(PlanningReferenceKind, DiagnosticCandidateLegRange);
        DISPLAY_CASE(PlanningReferenceKind, DiagnosticOverlay);
        DISPLAY_CASE(PlanningReferenceKind, CoverageResidual);
    }
    return {};
}

QString code(PlanningResidualKind value)
{
    switch (value) {
        DISPLAY_CASE(PlanningResidualKind, Uncovered);
        DISPLAY_CASE(PlanningResidualKind, CriticalUncovered);
        DISPLAY_CASE(PlanningResidualKind, BoundaryShortfall);
    }
    return {};
}

QString code(PlanningIssueSeverity value)
{
    switch (value) {
        DISPLAY_CASE(PlanningIssueSeverity, Info);
        DISPLAY_CASE(PlanningIssueSeverity, Warning);
        DISPLAY_CASE(PlanningIssueSeverity, Blocking);
    }
    return {};
}

QString code(CoverageRepairReason value)
{
    switch (value) {
        DISPLAY_CASE(CoverageRepairReason, InitialPolicyPass);
        DISPLAY_CASE(CoverageRepairReason, AssessmentUnavailable);
        DISPLAY_CASE(CoverageRepairReason, NoCanonicalCandidate);
        DISPLAY_CASE(CoverageRepairReason, NoUsefulRepair);
        DISPLAY_CASE(CoverageRepairReason, AppliedPolicyPass);
        DISPLAY_CASE(CoverageRepairReason, AppliedStillInsufficient);
    }
    return {};
}

QString code(DiagnosticOverlayKind value)
{
    switch (value) {
        DISPLAY_CASE(DiagnosticOverlayKind, UnreachableTarget);
        DISPLAY_CASE(DiagnosticOverlayKind, DisconnectedNavigation);
        DISPLAY_CASE(DiagnosticOverlayKind, UnsupportedGeometry);
        DISPLAY_CASE(DiagnosticOverlayKind, UnresolvedConnection);
    }
    return {};
}

QString code(PlannerResolutionReason value)
{
    switch (value) {
        DISPLAY_CASE(PlannerResolutionReason, None);
        DISPLAY_CASE(PlannerResolutionReason, TargetHasMultipleComponents);
        DISPLAY_CASE(PlannerResolutionReason, TargetHasHoles);
        DISPLAY_CASE(PlannerResolutionReason, TargetNonMonotoneForSelectedSweep);
    }
    return {};
}

QString code(PlanningStatus value)
{
    switch (value) {
        DISPLAY_CASE(PlanningStatus, Success);
        DISPLAY_CASE(PlanningStatus, InvalidInput);
        DISPLAY_CASE(PlanningStatus, Failed);
    }
    return {};
}

QString code(SafetySolutionTier value)
{
    switch (value) {
        DISPLAY_CASE(SafetySolutionTier, D0);
        DISPLAY_CASE(SafetySolutionTier, D1);
        DISPLAY_CASE(SafetySolutionTier, D2);
        DISPLAY_CASE(SafetySolutionTier, D3);
    }
    return {};
}

#undef DISPLAY_CASE

QString issueMessage(PlanningIssueCode value)
{
    switch (value) {
        case PlanningIssueCode::PlannerEscalated:
            return QCoreApplication::translate("MarinePlanning",
                                               "Target topology required a more complex coverage strategy.");
        case PlanningIssueCode::PreferredSafetyViolated:
            return QCoreApplication::translate(
                "MarinePlanning", "Hard clearance is certified; preferred clearance is not met on every leg.");
        case PlanningIssueCode::HardSafetyUnavailable:
            return QCoreApplication::translate(
                "MarinePlanning", "Hard navigation clearance is unavailable. Do not relax hard safety automatically.");
        case PlanningIssueCode::ExecutionReserveUnavailable:
            return QCoreApplication::translate("MarinePlanning", "The required execution reserve is unavailable.");
        case PlanningIssueCode::UnsafeDiagnosticCandidate:
            return QCoreApplication::translate("MarinePlanning", "This diagnostic route is not executable.");
        case PlanningIssueCode::CoverageBoundaryShortfall:
            return QCoreApplication::translate("MarinePlanning", "A reliably assessed boundary shortfall remains.");
        case PlanningIssueCode::CriticalCoverageGap:
            return QCoreApplication::translate("MarinePlanning", "A critical coverage gap remains.");
        case PlanningIssueCode::CoverageBelowRequirement:
            return QCoreApplication::translate("MarinePlanning",
                                               "The selected coverage requirement is not met. Upload is blocked.");
        case PlanningIssueCode::CoverageRepairApplied:
            return QCoreApplication::translate("MarinePlanning",
                                               "Safe target-relative repair improved the selected result.");
        case PlanningIssueCode::CoverageRepairInsufficient:
            return QCoreApplication::translate(
                "MarinePlanning", "Repair did not meet the coverage requirement. The hard-safe route is retained.");
        case PlanningIssueCode::NavigationRegionDisconnected:
            return QCoreApplication::translate(
                "MarinePlanning", "Navigation regions are disconnected; no continuous executable route is available.");
        case PlanningIssueCode::NavigationRegionUnsupported:
            return QCoreApplication::translate("MarinePlanning",
                                               "Navigation geometry is outside the supported capability.");
        case PlanningIssueCode::UnresolvedConnection:
            return QCoreApplication::translate("MarinePlanning",
                                               "A required connection is unresolved. The overlay is explanatory only.");
        case PlanningIssueCode::IngressNotAssessed:
            return QCoreApplication::translate(
                "MarinePlanning",
                "The route from the current vehicle position to the first Mission waypoint has not been assessed.");
        case PlanningIssueCode::CoverageAssessmentFailed:
            return QCoreApplication::translate(
                "MarinePlanning", "Coverage assessment failed; coverage cannot be certified. Upload is blocked.");
    }
    return {};
}

QString suggestionMessage(PlanningSuggestionCode value)
{
    switch (value) {
        case PlanningSuggestionCode::ExpandNavigationArea:
            return QCoreApplication::translate("MarinePlanning",
                                               "Consider whether the navigation area can safely include more room.");
        case PlanningSuggestionCode::ReviewPreferredClearance:
            return QCoreApplication::translate("MarinePlanning", "Review the preferred clearance setting.");
        case PlanningSuggestionCode::ReviewSwathWidth:
            return QCoreApplication::translate("MarinePlanning", "Review the explicitly configured coverage width.");
        case PlanningSuggestionCode::IncreaseNavigationRoom:
            return QCoreApplication::translate("MarinePlanning", "Review navigation room near the critical gap.");
        case PlanningSuggestionCode::ReviewHardNavigationFeasibility:
            return QCoreApplication::translate(
                "MarinePlanning", "Review hard navigation feasibility without automatically relaxing H or E.");
        case PlanningSuggestionCode::InspectRepairCost:
            return QCoreApplication::translate("MarinePlanning", "Inspect the repair's added route length and turns.");
    }
    return {};
}

QGeoCoordinate coordinate(const GeoPoint& point)
{
    return {point.latitudeDeg, point.longitudeDeg, point.altitudeM};
}

QVariantList path(const std::vector<GeoPoint>& points)
{
    QVariantList values;
    for (const auto& point : points) {
        values.append(QVariant::fromValue(coordinate(point)));
    }
    return values;
}

QVariantList regions(const GeoPolygonRegionSet& source)
{
    QVariantList result;
    for (std::size_t index = 0; index < source.size(); ++index) {
        QGeoPolygon polygon;
        QList<QGeoCoordinate> outer;
        for (const auto& point : source[index].outerBoundary.vertices) {
            outer.append(coordinate(point));
        }
        polygon.setPerimeter(outer);
        for (const auto& hole : source[index].holes) {
            QList<QGeoCoordinate> vertices;
            for (const auto& point : hole.vertices) {
                vertices.append(coordinate(point));
            }
            polygon.addHole(vertices);
        }
        result.append(
            QVariantMap{{"componentIndex", QVariant::fromValue(index)},
                        {"labelCoordinate", outer.isEmpty() ? QVariant() : QVariant::fromValue(outer.front())},
                        {"geoShape", QVariant::fromValue(QGeoShape(polygon))}});
    }
    return result;
}

QVariantMap available(bool valid, const QVariant& value)
{
    return {{"available", valid}, {"value", valid ? value : QVariant()}};
}

QVariantMap quality(const GeoCoverageQualityEvaluation& source)
{
    const auto& a = source.availability;
    return {{"status", code(source.status)},
            {"error", code(source.error)},
            {"requirement", source.requirement == CoverageRequirement::Strict ? "Strict" : "Standard"},
            {"strategy", QString::fromStdString(source.strategy.strategyId)},
            {"strategyVersion", QString::fromStdString(source.strategy.semanticVersion)},
            {"policyVersion", QString::fromStdString(source.policySemanticVersion)},
            {"passesRequirement", source.passesRequirement},
            {"strictFallbackTriggered", source.strictFallbackTriggered},
            {"wholeTargetStrictFallback", source.wholeTargetStrictFallback},
            {"targetAreaM2", available(a.targetArea, source.targetAreaM2)},
            {"coveredAreaM2", available(a.coveredArea, source.coveredAreaM2)},
            {"uncoveredAreaM2", available(a.uncoveredArea, source.uncoveredAreaM2)},
            {"coverageRatio", available(a.ratio, source.coverageRatio)},
            {"criticalUncoveredAreaM2", available(a.criticalUncoveredArea, source.criticalUncoveredAreaM2)},
            {"numericalToleranceM2", available(a.numericalTolerance, source.numericalToleranceM2)},
            {"criticalCoverageCore", available(a.criticalCore, regions(source.residual.criticalCoverageCore))},
            {"uncovered", available(a.uncovered, regions(source.residual.uncoveredRegion))},
            {"criticalUncovered", available(a.criticalUncovered, regions(source.residual.criticalUncoveredRegion))},
            {"boundaryShortfall", available(a.boundaryShortfall, regions(source.residual.boundaryShortfallRegion))},
            {"fallbackComponents",
             available(a.fallbackComponents, regions(source.residual.strictFallbackTargetComponents))}};
}

QVariantMap reference(const std::optional<PlanningReference>& source)
{
    if (!source) {
        return {};
    }
    return {{"kind", code(source->kind)},
            {"index", QVariant::fromValue(source->index)},
            {"firstLeg", QVariant::fromValue(source->firstLeg)},
            {"legCount", QVariant::fromValue(source->legCount)},
            {"residual", code(source->residual)}};
}

QVariantList runs(const std::vector<GeoPoint>& points, const std::vector<PathLegRole>& roles,
                  const std::vector<SafetyLegClass>& assessments)
{
    QVariantList result;
    if (points.size() < 2 || roles.size() != points.size() - 1 || assessments.size() != roles.size()) {
        return result;
    }
    for (std::size_t first = 0; first < roles.size();) {
        std::size_t end = first + 1;
        while (end < roles.size() && roles[end] == roles[first] && assessments[end] == assessments[first]) {
            ++end;
        }
        QVariantList vertices;
        for (std::size_t index = first; index <= end; ++index) {
            vertices.append(QVariant::fromValue(coordinate(points[index])));
        }
        result.append(QVariantMap{{"firstLeg", QVariant::fromValue(first)},
                                  {"legCount", QVariant::fromValue(end - first)},
                                  {"role", roles[first] == PathLegRole::Coverage ? "Coverage" : "Transit"},
                                  {"safetyClass", code(assessments[first])},
                                  {"path", vertices}});
        first = end;
    }
    return result;
}
}  // namespace

QVariantMap planningPresentation(const PlanningResult& source, bool current, bool stale)
{
    QVariantMap result{{"hasCurrentResult", current},
                       {"stale", stale},
                       {"status", current ? code(source.status) : "Unplanned"},
                       {"readiness", current ? code(source.outcome.readiness) : "None"},
                       {"ingressMessage", issueMessage(PlanningIssueCode::IngressNotAssessed)}};
    if (!current) {
        return result;
    }
    const auto& o = source.outcome;
    result.insert("tier", o.tier ? QVariant(code(*o.tier)) : QVariant());
    result.insert("canonicalRuns", runs(source.path, source.legRoles, o.canonicalLegAssessments));
    if (o.coverageQuality) {
        result.insert("quality", quality(*o.coverageQuality));
    }
    QVariantList issues;
    for (const auto& item : o.issues) {
        issues.append(QVariantMap{{"code", code(item.code)},
                                  {"severity", code(item.severity)},
                                  {"message", issueMessage(item.code)},
                                  {"sourceMessage", QString::fromStdString(item.message)},
                                  {"reference", reference(item.reference)}});
    }
    result.insert("issues", issues);
    QVariantList suggestions;
    for (const auto& item : o.suggestions) {
        suggestions.append(QVariantMap{{"code", code(item.code)},
                                       {"issue", code(item.issue)},
                                       {"message", suggestionMessage(item.code)},
                                       {"sourceMessage", QString::fromStdString(item.message)},
                                       {"reference", reference(item.reference)}});
    }
    result.insert("suggestions", suggestions);
    if (o.diagnosticCandidate) {
        const auto& d = *o.diagnosticCandidate;
        result.insert("diagnosticCandidate", QVariantMap{{"identity", QString::fromStdString(d.identity)},
                                                         {"runs", runs(d.path, d.legRoles, d.legAssessments)},
                                                         {"path", path(d.path)},
                                                         {"coverageLengthM", d.coverageLengthM},
                                                         {"transitLengthM", d.transitLengthM},
                                                         {"pathLengthM", d.pathLengthM},
                                                         {"turnCount", d.turnCount}});
    }
    QVariantList overlays;
    for (const auto& overlay : o.diagnosticOverlays) {
        overlays.append(QVariantMap{{"kind", code(overlay.kind)},
                                    {"explanation", QString::fromStdString(overlay.explanation)},
                                    {"regions", regions(overlay.geometry)}});
    }
    result.insert("diagnosticOverlays", overlays);
    QVariantList components;
    for (const auto& c : o.repair.components) {
        components.append(QVariantMap{{"componentId", c.componentId},
                                      {"entryIndex", QVariant::fromValue(c.entryIndex)},
                                      {"reverse", c.reverse},
                                      {"transitionCostM", c.transitionCostM},
                                      {"path", path(c.componentPath)},
                                      {"before", quality(c.before)},
                                      {"after", quality(c.after)},
                                      {"pathLengthBeforeM", c.pathLengthBeforeM},
                                      {"pathLengthAfterM", c.pathLengthAfterM},
                                      {"turnCountBefore", c.turnCountBefore},
                                      {"turnCountAfter", c.turnCountAfter}});
    }
    result.insert("repair", QVariantMap{{"attempted", o.repair.attempted},
                                        {"applied", o.repair.applied},
                                        {"reason", code(o.repair.reason)},
                                        {"components", components}});
    if (source.plannerSource) {
        const auto& p = *source.plannerSource;
        result.insert(
            "plannerSource",
            QVariantMap{
                {"requestedPlannerId", QString::fromStdString(p.requestedPlannerId)},
                {"resolvedStrategyId", QString::fromStdString(p.resolvedStrategy.strategyId)},
                {"strategyVersion", QString::fromStdString(p.resolvedStrategy.semanticVersion)},
                {"resolutionStatus",
                 p.resolutionStatus == PlannerResolutionStatus::Resolved ? "Resolved" : "ResolvedStrategyUnavailable"},
                {"resolutionReason", code(p.resolutionReason)},
                {"escalated", p.escalated},
                {"requestedSweepMode", p.requestedSweepMode == SweepAngleMode::Auto ? "Auto" : "Manual"},
                {"selectedSweepAngleDeg", p.selectedSweepAngleDeg},
                {"sweepVersion", QString::fromStdString(p.sweepSemanticVersion)}});
    }
    const bool canonical = !source.path.empty();
    result.insert("metrics",
                  QVariantMap{{"pathLengthM", available(canonical, source.pathLengthM)},
                              {"coverageLengthM", available(canonical, source.coverageLengthM)},
                              {"transitLengthM", available(canonical, source.transitLengthM)},
                              {"turnCount", available(canonical, source.turnCount)},
                              {"cellCount", available(canonical && source.cellCount > 0, source.cellCount)}});
    return result;
}

}  // namespace Marine::QGC
