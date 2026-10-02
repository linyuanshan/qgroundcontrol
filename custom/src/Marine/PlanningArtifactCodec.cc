#include "PlanningArtifactCodec.h"

#include <QtCore/QJsonArray>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <span>
#include <string_view>
#include <utility>

#include "Geometry/GeoReference.h"
#include "PlanningPathMetrics.h"

namespace Marine {
namespace {

template <typename Enum>
std::span<const char* const> enumNames();

template <>
std::span<const char* const> enumNames<MissionReadiness>()
{
    static const char* const names[] = {"None", "Ready", "ReadyWithWarning", "ReviewRequired", "DiagnosticOnly"};
    return names;
}

template <>
std::span<const char* const> enumNames<SafetySolutionTier>()
{
    static const char* const names[] = {"D0", "D1", "D2", "D3"};
    return names;
}

template <>
std::span<const char* const> enumNames<SafetyLegClass>()
{
    static const char* const names[] = {"PreferredSafe", "HardSafeWarning", "ExecutionUnsafe", "HardUnsafe"};
    return names;
}

template <>
std::span<const char* const> enumNames<PlanningIssueCode>()
{
    static const char* const names[] = {"PlannerEscalated",
                                        "PreferredSafetyViolated",
                                        "HardSafetyUnavailable",
                                        "ExecutionReserveUnavailable",
                                        "UnsafeDiagnosticCandidate",
                                        "CoverageBoundaryShortfall",
                                        "CriticalCoverageGap",
                                        "CoverageBelowRequirement",
                                        "CoverageRepairApplied",
                                        "CoverageRepairInsufficient",
                                        "NavigationRegionDisconnected",
                                        "NavigationRegionUnsupported",
                                        "UnresolvedConnection",
                                        "IngressNotAssessed",
                                        "CoverageAssessmentFailed"};
    return names;
}

template <>
std::span<const char* const> enumNames<PlanningIssueSeverity>()
{
    static const char* const names[] = {"Info", "Warning", "Blocking"};
    return names;
}

template <>
std::span<const char* const> enumNames<PlanningReferenceKind>()
{
    static const char* const names[] = {"CanonicalPathLegRange", "DiagnosticCandidateLegRange", "DiagnosticOverlay",
                                        "CoverageResidual"};
    return names;
}

template <>
std::span<const char* const> enumNames<PlanningResidualKind>()
{
    static const char* const names[] = {"Uncovered", "CriticalUncovered", "BoundaryShortfall"};
    return names;
}

template <>
std::span<const char* const> enumNames<PlanningSuggestionCode>()
{
    static const char* const names[] = {
        "ExpandNavigationArea",   "ReviewPreferredClearance",        "ReviewSwathWidth",
        "IncreaseNavigationRoom", "ReviewHardNavigationFeasibility", "InspectRepairCost"};
    return names;
}

template <>
std::span<const char* const> enumNames<DiagnosticOverlayKind>()
{
    static const char* const names[] = {"UnreachableTarget", "DisconnectedNavigation", "UnsupportedGeometry",
                                        "UnresolvedConnection"};
    return names;
}

template <>
std::span<const char* const> enumNames<CoverageRepairReason>()
{
    static const char* const names[] = {"InitialPolicyPass", "AssessmentUnavailable", "NoCanonicalCandidate",
                                        "NoUsefulRepair",    "AppliedPolicyPass",     "AppliedStillInsufficient"};
    return names;
}

template <>
std::span<const char* const> enumNames<CoverageQualityStatus>()
{
    static const char* const names[] = {"Complete", "Acceptable", "Insufficient", "AssessmentError"};
    return names;
}

template <>
std::span<const char* const> enumNames<CoverageQualityError>()
{
    static const char* const names[] = {"None",
                                        "InvalidTarget",
                                        "InvalidPath",
                                        "InvalidSwathWidth",
                                        "InvalidRequirement",
                                        "InvalidStrategy",
                                        "UnsupportedPolicySemantics",
                                        "GeometryFailure",
                                        "NumericalFailure"};
    return names;
}

template <>
std::span<const char* const> enumNames<CoveragePlanningError>()
{
    static const char* const names[] = {"None",
                                        "InvalidOuterBoundary",
                                        "InvalidNavigationBoundary",
                                        "CoverageOutsideNavigationBoundary",
                                        "EmptyCoverageTarget",
                                        "InvalidCoverageTarget",
                                        "UnsupportedSeparateBoundaries",
                                        "InvalidSwathWidth",
                                        "InvalidSafetyMargin",
                                        "InvalidPreferredSafetyMargin",
                                        "InvalidExecutionMargin",
                                        "InvalidSweepAngle",
                                        "InvalidCoverageRequirement",
                                        "CoverageImpossibleWithSafetyMargin",
                                        "CoverageImpossibleWithExecutionMargin",
                                        "UnsupportedExecutionSafetyProfile",
                                        "ExecutionRegionNotConservative",
                                        "UnsupportedNoGoRegion",
                                        "SafetyInsetEmpty",
                                        "SafetyInsetDisconnected",
                                        "NonMonotoneSweep",
                                        "UnsafeConnector",
                                        "InvalidGeneratedPath",
                                        "GeometryFailure",
                                        "InvalidNoGoRegion",
                                        "NoGoOutsideBoundary",
                                        "NoGoBoundaryConflict",
                                        "NoGoOverlapOrTouch",
                                        "NoNavigableArea",
                                        "DisconnectedFeasibleRegion",
                                        "DecompositionFailed",
                                        "InvalidCoverageCell",
                                        "CellCoverageFailed",
                                        "SafeTransitNotFound",
                                        "CoverageIncomplete",
                                        "UnsupportedStrategyCapability",
                                        "ResolvedStrategyUnavailable"};
    return names;
}

template <>
std::span<const char* const> enumNames<PlannerResolutionStatus>()
{
    static const char* const names[] = {"Resolved", "ResolvedStrategyUnavailable"};
    return names;
}

template <>
std::span<const char* const> enumNames<PlannerResolutionReason>()
{
    static const char* const names[] = {"None", "TargetHasMultipleComponents", "TargetHasHoles",
                                        "TargetNonMonotoneForSelectedSweep"};
    return names;
}

template <>
std::span<const char* const> enumNames<MarineTaskType>()
{
    static const char* const names[] = {"CoverageInspection"};
    return names;
}

template <>
std::span<const char* const> enumNames<CoverageRequirement>()
{
    static const char* const names[] = {"Standard", "Strict"};
    return names;
}

template <>
std::span<const char* const> enumNames<SweepAngleMode>()
{
    static const char* const names[] = {"Manual", "Auto"};
    return names;
}

template <>
std::span<const char* const> enumNames<PlanningStatus>()
{
    static const char* const names[] = {"Success", "InvalidInput", "Failed"};
    return names;
}

template <>
std::span<const char* const> enumNames<PathLegRole>()
{
    static const char* const names[] = {"Coverage", "Transit"};
    return names;
}

template <typename Enum>
QString enumText(Enum value)
{
    const auto names = enumNames<Enum>();
    const auto index = static_cast<std::size_t>(value);
    return index < names.size() ? QString::fromLatin1(names[index]) : QString();
}

template <typename Enum>
bool readEnum(const QJsonValue& json, Enum& value)
{
    if (!json.isString()) {
        return false;
    }
    const auto names = enumNames<Enum>();
    for (std::size_t index = 0; index < names.size(); ++index) {
        if (json.toString() == QLatin1StringView(names[index])) {
            value = static_cast<Enum>(index);
            return true;
        }
    }
    return false;
}

bool number(const QJsonValue& json, double& value)
{
    if (!json.isDouble() || !std::isfinite(json.toDouble())) {
        return false;
    }
    value = json.toDouble();
    return true;
}

bool count(const QJsonValue& json, std::size_t& value)
{
    double numeric = 0;
    // All persisted indices are bounded by container sizes; never permit a lossy JSON integer conversion.
    if (!number(json, numeric) || numeric < 0 || numeric > std::numeric_limits<int>::max() ||
        std::floor(numeric) != numeric) {
        return false;
    }
    value = static_cast<std::size_t>(numeric);
    return true;
}

bool integer(const QJsonValue& json, int& value)
{
    std::size_t numeric = 0;
    if (!count(json, numeric)) {
        return false;
    }
    value = static_cast<int>(numeric);
    return true;
}

bool boolean(const QJsonValue& json, bool& value)
{
    if (!json.isBool()) {
        return false;
    }
    value = json.toBool();
    return true;
}

bool string(const QJsonValue& json, std::string& value)
{
    if (!json.isString()) {
        return false;
    }
    value = json.toString().toStdString();
    return true;
}

bool validPoint(const GeoPoint& point)
{
    return std::isfinite(point.latitudeDeg) && std::isfinite(point.longitudeDeg) && std::isfinite(point.altitudeM) &&
           point.latitudeDeg >= -90 && point.latitudeDeg <= 90 && point.longitudeDeg >= -180 &&
           point.longitudeDeg <= 180;
}

QJsonArray pathJson(const std::vector<GeoPoint>& path)
{
    QJsonArray array;
    for (const auto& point : path) {
        array.append(QJsonArray{point.latitudeDeg, point.longitudeDeg, point.altitudeM});
    }
    return array;
}

bool readPath(const QJsonValue& json, std::vector<GeoPoint>& path)
{
    if (!json.isArray()) {
        return false;
    }
    for (const auto& value : json.toArray()) {
        const auto tuple = value.toArray();
        GeoPoint point;
        if (!value.isArray() || tuple.size() != 3 || !number(tuple[0], point.latitudeDeg) ||
            !number(tuple[1], point.longitudeDeg) || !number(tuple[2], point.altitudeM) || !validPoint(point)) {
            return false;
        }
        path.push_back(point);
    }
    return true;
}

QJsonArray regionsJson(const GeoPolygonRegionSet& regions)
{
    QJsonArray array;
    for (const auto& region : regions) {
        QJsonArray holes;
        for (const auto& hole : region.holes) {
            holes.append(pathJson(hole.vertices));
        }
        array.append(QJsonObject{{"outer", pathJson(region.outerBoundary.vertices)}, {"holes", holes}});
    }
    return array;
}

bool readRegions(const QJsonValue& json, GeoPolygonRegionSet& regions)
{
    if (!json.isArray()) {
        return false;
    }
    for (const auto& value : json.toArray()) {
        if (!value.isObject()) {
            return false;
        }
        const auto object = value.toObject();
        GeoPolygonRegion region;
        if (!readPath(object.value("outer"), region.outerBoundary.vertices) ||
            region.outerBoundary.vertices.size() < 3 || !object.value("holes").isArray()) {
            return false;
        }
        for (const auto& holeJson : object.value("holes").toArray()) {
            GeoPolygon hole;
            if (!readPath(holeJson, hole.vertices) || hole.vertices.size() < 3) {
                return false;
            }
            region.holes.push_back(std::move(hole));
        }
        regions.push_back(std::move(region));
    }
    return true;
}

template <typename Enum>
QJsonArray enumArray(const std::vector<Enum>& values)
{
    QJsonArray array;
    for (const auto value : values) {
        array.append(enumText(value));
    }
    return array;
}

template <typename Enum>
bool readEnums(const QJsonValue& json, std::vector<Enum>& values)
{
    if (!json.isArray()) {
        return false;
    }
    for (const auto& jsonValue : json.toArray()) {
        Enum value{};
        if (!readEnum(jsonValue, value)) {
            return false;
        }
        values.push_back(value);
    }
    return true;
}

QJsonObject factJson(bool available, const QJsonValue& value)
{
    return {{"available", available}, {"value", available ? value : QJsonValue(QJsonValue::Null)}};
}

bool readFact(const QJsonValue& json, bool& available, QJsonValue& value)
{
    if (!json.isObject() || !boolean(json.toObject().value("available"), available)) {
        return false;
    }
    value = json.toObject().value("value");
    return !value.isUndefined() && (available ? !value.isNull() : value.isNull());
}

QJsonObject qualityJson(const GeoCoverageQualityEvaluation& quality)
{
    QJsonObject json{{"status", enumText(quality.status)},
                     {"error", enumText(quality.error)},
                     {"strategyId", QString::fromStdString(quality.strategy.strategyId)},
                     {"strategyVersion", QString::fromStdString(quality.strategy.semanticVersion)},
                     {"requirement", enumText(quality.requirement)},
                     {"policySemanticVersion", QString::fromStdString(quality.policySemanticVersion)},
                     {"passesRequirement", quality.passesRequirement},
                     {"strictFallbackTriggered", quality.strictFallbackTriggered},
                     {"wholeTargetStrictFallback", quality.wholeTargetStrictFallback},
                     {"message", QString::fromStdString(quality.message)}};
    json.insert("targetAreaM2", factJson(quality.availability.targetArea, quality.targetAreaM2));
    json.insert("coveredAreaM2", factJson(quality.availability.coveredArea, quality.coveredAreaM2));
    json.insert("uncoveredAreaM2", factJson(quality.availability.uncoveredArea, quality.uncoveredAreaM2));
    json.insert("coverageRatio", factJson(quality.availability.ratio, quality.coverageRatio));
    json.insert("criticalUncoveredAreaM2",
                factJson(quality.availability.criticalUncoveredArea, quality.criticalUncoveredAreaM2));
    json.insert("numericalToleranceM2",
                factJson(quality.availability.numericalTolerance, quality.numericalToleranceM2));
    json.insert("criticalCoverageCore",
                factJson(quality.availability.criticalCore, regionsJson(quality.residual.criticalCoverageCore)));
    json.insert("uncoveredRegion",
                factJson(quality.availability.uncovered, regionsJson(quality.residual.uncoveredRegion)));
    json.insert("criticalUncoveredRegion", factJson(quality.availability.criticalUncovered,
                                                    regionsJson(quality.residual.criticalUncoveredRegion)));
    json.insert("boundaryShortfallRegion", factJson(quality.availability.boundaryShortfall,
                                                    regionsJson(quality.residual.boundaryShortfallRegion)));
    json.insert("strictFallbackTargetComponents",
                factJson(quality.availability.fallbackComponents,
                         regionsJson(quality.residual.strictFallbackTargetComponents)));
    return json;
}

bool readQuality(const QJsonValue& value, GeoCoverageQualityEvaluation& quality)
{
    if (!value.isObject()) {
        return false;
    }
    const auto json = value.toObject();
    if (!readEnum(json.value("status"), quality.status) || !readEnum(json.value("error"), quality.error) ||
        !readEnum(json.value("requirement"), quality.requirement) ||
        !string(json.value("strategyId"), quality.strategy.strategyId) ||
        !string(json.value("strategyVersion"), quality.strategy.semanticVersion) ||
        !string(json.value("policySemanticVersion"), quality.policySemanticVersion) ||
        !boolean(json.value("passesRequirement"), quality.passesRequirement) ||
        !boolean(json.value("strictFallbackTriggered"), quality.strictFallbackTriggered) ||
        !boolean(json.value("wholeTargetStrictFallback"), quality.wholeTargetStrictFallback) ||
        !string(json.value("message"), quality.message)) {
        return false;
    }
    QJsonValue fact;
    if (!readFact(json.value("targetAreaM2"), quality.availability.targetArea, fact) ||
        (quality.availability.targetArea && !number(fact, quality.targetAreaM2))) {
        return false;
    }
    if (!readFact(json.value("coveredAreaM2"), quality.availability.coveredArea, fact) ||
        (quality.availability.coveredArea && !number(fact, quality.coveredAreaM2))) {
        return false;
    }
    if (!readFact(json.value("uncoveredAreaM2"), quality.availability.uncoveredArea, fact) ||
        (quality.availability.uncoveredArea && !number(fact, quality.uncoveredAreaM2))) {
        return false;
    }
    if (!readFact(json.value("coverageRatio"), quality.availability.ratio, fact) ||
        (quality.availability.ratio && !number(fact, quality.coverageRatio))) {
        return false;
    }
    if (!readFact(json.value("criticalUncoveredAreaM2"), quality.availability.criticalUncoveredArea, fact) ||
        (quality.availability.criticalUncoveredArea && !number(fact, quality.criticalUncoveredAreaM2))) {
        return false;
    }
    if (!readFact(json.value("numericalToleranceM2"), quality.availability.numericalTolerance, fact) ||
        (quality.availability.numericalTolerance && !number(fact, quality.numericalToleranceM2))) {
        return false;
    }
    if (!readFact(json.value("criticalCoverageCore"), quality.availability.criticalCore, fact) ||
        (quality.availability.criticalCore && !readRegions(fact, quality.residual.criticalCoverageCore))) {
        return false;
    }
    if (!readFact(json.value("uncoveredRegion"), quality.availability.uncovered, fact) ||
        (quality.availability.uncovered && !readRegions(fact, quality.residual.uncoveredRegion))) {
        return false;
    }
    if (!readFact(json.value("criticalUncoveredRegion"), quality.availability.criticalUncovered, fact) ||
        (quality.availability.criticalUncovered && !readRegions(fact, quality.residual.criticalUncoveredRegion))) {
        return false;
    }
    if (!readFact(json.value("boundaryShortfallRegion"), quality.availability.boundaryShortfall, fact) ||
        (quality.availability.boundaryShortfall && !readRegions(fact, quality.residual.boundaryShortfallRegion))) {
        return false;
    }
    if (!readFact(json.value("strictFallbackTargetComponents"), quality.availability.fallbackComponents, fact) ||
        (quality.availability.fallbackComponents &&
         !readRegions(fact, quality.residual.strictFallbackTargetComponents))) {
        return false;
    }
    return true;
}

QJsonValue sourceJson(const std::optional<PlannerSourceInfo>& source)
{
    if (!source) {
        return QJsonValue(QJsonValue::Null);
    }
    return QJsonObject{{"requestedPlannerId", QString::fromStdString(source->requestedPlannerId)},
                       {"resolvedStrategyId", QString::fromStdString(source->resolvedStrategy.strategyId)},
                       {"strategySemanticVersion", QString::fromStdString(source->resolvedStrategy.semanticVersion)},
                       {"resolutionStatus", enumText(source->resolutionStatus)},
                       {"resolutionReason", enumText(source->resolutionReason)},
                       {"escalated", source->escalated},
                       {"requestedSweepMode", enumText(source->requestedSweepMode)},
                       {"selectedSweepAngleDeg", source->selectedSweepAngleDeg},
                       {"sweepSemanticVersion", QString::fromStdString(source->sweepSemanticVersion)}};
}

bool readSource(const QJsonValue& value, std::optional<PlannerSourceInfo>& source)
{
    if (value.isNull()) {
        return true;
    }
    if (!value.isObject()) {
        return false;
    }
    const auto json = value.toObject();
    PlannerSourceInfo parsed;
    if (!string(json.value("requestedPlannerId"), parsed.requestedPlannerId) ||
        !string(json.value("resolvedStrategyId"), parsed.resolvedStrategy.strategyId) ||
        !string(json.value("strategySemanticVersion"), parsed.resolvedStrategy.semanticVersion) ||
        !readEnum(json.value("resolutionStatus"), parsed.resolutionStatus) ||
        !readEnum(json.value("resolutionReason"), parsed.resolutionReason) ||
        !boolean(json.value("escalated"), parsed.escalated) ||
        !readEnum(json.value("requestedSweepMode"), parsed.requestedSweepMode) ||
        !number(json.value("selectedSweepAngleDeg"), parsed.selectedSweepAngleDeg) ||
        !string(json.value("sweepSemanticVersion"), parsed.sweepSemanticVersion)) {
        return false;
    }
    source = std::move(parsed);
    return true;
}

QJsonValue referenceJson(const std::optional<PlanningReference>& reference)
{
    if (!reference) {
        return QJsonValue(QJsonValue::Null);
    }
    return QJsonObject{{"kind", enumText(reference->kind)},
                       {"index", static_cast<double>(reference->index)},
                       {"firstLeg", static_cast<double>(reference->firstLeg)},
                       {"legCount", static_cast<double>(reference->legCount)},
                       {"residual", enumText(reference->residual)}};
}

bool readReference(const QJsonValue& value, std::optional<PlanningReference>& reference)
{
    if (value.isNull()) {
        return true;
    }
    if (!value.isObject()) {
        return false;
    }
    const auto json = value.toObject();
    PlanningReference parsed;
    if (!readEnum(json.value("kind"), parsed.kind) || !count(json.value("index"), parsed.index) ||
        !count(json.value("firstLeg"), parsed.firstLeg) || !count(json.value("legCount"), parsed.legCount) ||
        !readEnum(json.value("residual"), parsed.residual)) {
        return false;
    }
    reference = parsed;
    return true;
}

QJsonObject repairJson(const SelectedRepairProvenance<GeoPoint, GeoPolygonRegionSet>& repair)
{
    QJsonArray components;
    for (const auto& component : repair.components) {
        components.append(QJsonObject{{"componentId", static_cast<double>(component.componentId)},
                                      {"componentPath", pathJson(component.componentPath)},
                                      {"entryIndex", static_cast<double>(component.entryIndex)},
                                      {"reverse", component.reverse},
                                      {"transitionCostM", component.transitionCostM},
                                      {"before", qualityJson(component.before)},
                                      {"after", qualityJson(component.after)},
                                      {"pathLengthBeforeM", component.pathLengthBeforeM},
                                      {"pathLengthAfterM", component.pathLengthAfterM},
                                      {"turnCountBefore", component.turnCountBefore},
                                      {"turnCountAfter", component.turnCountAfter}});
    }
    return {{"attempted", repair.attempted},
            {"applied", repair.applied},
            {"reason", enumText(repair.reason)},
            {"components", components}};
}

bool readRepair(const QJsonValue& value, SelectedRepairProvenance<GeoPoint, GeoPolygonRegionSet>& repair)
{
    if (!value.isObject()) {
        return false;
    }
    const auto json = value.toObject();
    if (!boolean(json.value("attempted"), repair.attempted) || !boolean(json.value("applied"), repair.applied) ||
        !readEnum(json.value("reason"), repair.reason) || !json.value("components").isArray()) {
        return false;
    }
    for (const auto& componentValue : json.value("components").toArray()) {
        if (!componentValue.isObject()) {
            return false;
        }
        const auto item = componentValue.toObject();
        AppliedRepairComponent<GeoPoint, GeoPolygonRegionSet> component;
        std::size_t id = 0;
        if (!count(item.value("componentId"), id) || !count(item.value("entryIndex"), component.entryIndex) ||
            !readPath(item.value("componentPath"), component.componentPath) ||
            !boolean(item.value("reverse"), component.reverse) ||
            !number(item.value("transitionCostM"), component.transitionCostM) ||
            !readQuality(item.value("before"), component.before) ||
            !readQuality(item.value("after"), component.after) ||
            !number(item.value("pathLengthBeforeM"), component.pathLengthBeforeM) ||
            !number(item.value("pathLengthAfterM"), component.pathLengthAfterM) ||
            !integer(item.value("turnCountBefore"), component.turnCountBefore) ||
            !integer(item.value("turnCountAfter"), component.turnCountAfter)) {
            return false;
        }
        component.componentId = static_cast<std::uint32_t>(id);
        repair.components.push_back(std::move(component));
    }
    return true;
}

QJsonObject outcomeJson(const PlanningOutcome<GeoPoint, GeoPolygonRegionSet>& outcome)
{
    QJsonArray issues;
    for (const auto& issue : outcome.issues) {
        issues.append(QJsonObject{{"code", enumText(issue.code)},
                                  {"severity", enumText(issue.severity)},
                                  {"message", QString::fromStdString(issue.message)},
                                  {"reference", referenceJson(issue.reference)}});
    }
    QJsonArray suggestions;
    for (const auto& suggestion : outcome.suggestions) {
        suggestions.append(QJsonObject{{"code", enumText(suggestion.code)},
                                       {"issue", enumText(suggestion.issue)},
                                       {"message", QString::fromStdString(suggestion.message)},
                                       {"reference", referenceJson(suggestion.reference)}});
    }
    QJsonValue diagnostic(QJsonValue::Null);
    if (outcome.diagnosticCandidate) {
        const auto& candidate = *outcome.diagnosticCandidate;
        diagnostic = QJsonObject{{"identity", QString::fromStdString(candidate.identity)},
                                 {"path", pathJson(candidate.path)},
                                 {"legRoles", enumArray(candidate.legRoles)},
                                 {"legAssessments", enumArray(candidate.legAssessments)},
                                 {"coverageLengthM", candidate.coverageLengthM},
                                 {"transitLengthM", candidate.transitLengthM},
                                 {"pathLengthM", candidate.pathLengthM},
                                 {"turnCount", candidate.turnCount}};
    }
    QJsonArray overlays;
    for (const auto& overlay : outcome.diagnosticOverlays) {
        overlays.append(QJsonObject{{"kind", enumText(overlay.kind)},
                                    {"geometry", regionsJson(overlay.geometry)},
                                    {"explanation", QString::fromStdString(overlay.explanation)}});
    }
    return {{"readiness", enumText(outcome.readiness)},
            {"tier", outcome.tier ? QJsonValue(enumText(*outcome.tier)) : QJsonValue(QJsonValue::Null)},
            {"canonicalLegAssessments", enumArray(outcome.canonicalLegAssessments)},
            {"coverageQuality", outcome.coverageQuality ? QJsonValue(qualityJson(*outcome.coverageQuality))
                                                        : QJsonValue(QJsonValue::Null)},
            {"selectedCandidateIndex", outcome.selectedCandidateIndex
                                           ? QJsonValue(static_cast<double>(*outcome.selectedCandidateIndex))
                                           : QJsonValue(QJsonValue::Null)},
            {"repair", repairJson(outcome.repair)},
            {"issues", issues},
            {"suggestions", suggestions},
            {"diagnosticCandidate", diagnostic},
            {"diagnosticOverlays", overlays}};
}

bool readOutcome(const QJsonValue& value, PlanningOutcome<GeoPoint, GeoPolygonRegionSet>& outcome)
{
    if (!value.isObject()) {
        return false;
    }
    const auto json = value.toObject();
    if (!readEnum(json.value("readiness"), outcome.readiness) ||
        !readEnums(json.value("canonicalLegAssessments"), outcome.canonicalLegAssessments) ||
        !readRepair(json.value("repair"), outcome.repair) || !json.value("issues").isArray() ||
        !json.value("suggestions").isArray() || !json.value("diagnosticOverlays").isArray()) {
        return false;
    }
    if (!json.value("tier").isNull()) {
        SafetySolutionTier tier{};
        if (!readEnum(json.value("tier"), tier)) {
            return false;
        }
        outcome.tier = tier;
    }
    if (!json.value("selectedCandidateIndex").isNull()) {
        std::size_t index = 0;
        if (!count(json.value("selectedCandidateIndex"), index)) {
            return false;
        }
        outcome.selectedCandidateIndex = index;
    }
    if (!json.value("coverageQuality").isNull()) {
        outcome.coverageQuality.emplace();
        if (!readQuality(json.value("coverageQuality"), *outcome.coverageQuality)) {
            return false;
        }
    }
    for (const auto& issueValue : json.value("issues").toArray()) {
        if (!issueValue.isObject()) {
            return false;
        }
        const auto item = issueValue.toObject();
        PlanningIssue issue;
        if (!readEnum(item.value("code"), issue.code) || !readEnum(item.value("severity"), issue.severity) ||
            !string(item.value("message"), issue.message) || !readReference(item.value("reference"), issue.reference)) {
            return false;
        }
        outcome.issues.push_back(std::move(issue));
    }
    for (const auto& suggestionValue : json.value("suggestions").toArray()) {
        if (!suggestionValue.isObject()) {
            return false;
        }
        const auto item = suggestionValue.toObject();
        PlanningSuggestion suggestion;
        if (!readEnum(item.value("code"), suggestion.code) || !readEnum(item.value("issue"), suggestion.issue) ||
            !string(item.value("message"), suggestion.message) ||
            !readReference(item.value("reference"), suggestion.reference)) {
            return false;
        }
        outcome.suggestions.push_back(std::move(suggestion));
    }
    if (!json.value("diagnosticCandidate").isNull()) {
        if (!json.value("diagnosticCandidate").isObject()) {
            return false;
        }
        const auto item = json.value("diagnosticCandidate").toObject();
        DiagnosticCandidate<GeoPoint> candidate;
        if (!string(item.value("identity"), candidate.identity) || !readPath(item.value("path"), candidate.path) ||
            !readEnums(item.value("legRoles"), candidate.legRoles) ||
            !readEnums(item.value("legAssessments"), candidate.legAssessments) ||
            !number(item.value("coverageLengthM"), candidate.coverageLengthM) ||
            !number(item.value("transitLengthM"), candidate.transitLengthM) ||
            !number(item.value("pathLengthM"), candidate.pathLengthM) ||
            !integer(item.value("turnCount"), candidate.turnCount)) {
            return false;
        }
        outcome.diagnosticCandidate = std::move(candidate);
    }
    for (const auto& overlayValue : json.value("diagnosticOverlays").toArray()) {
        if (!overlayValue.isObject()) {
            return false;
        }
        const auto item = overlayValue.toObject();
        DiagnosticOverlay<GeoPolygonRegionSet> overlay;
        if (!readEnum(item.value("kind"), overlay.kind) || !readRegions(item.value("geometry"), overlay.geometry) ||
            !string(item.value("explanation"), overlay.explanation)) {
            return false;
        }
        outcome.diagnosticOverlays.push_back(std::move(overlay));
    }
    return true;
}

bool validQuality(const GeoCoverageQualityEvaluation& quality)
{
    if (enumText(quality.status).isEmpty() || enumText(quality.error).isEmpty() ||
        enumText(quality.requirement).isEmpty() || quality.strategy.strategyId.empty() ||
        quality.strategy.semanticVersion.empty() || quality.policySemanticVersion.empty()) {
        return false;
    }
    const auto& a = quality.availability;
    const bool completeAvailability = a.targetArea && a.coveredArea && a.uncoveredArea && a.ratio &&
                                      a.criticalUncoveredArea && a.numericalTolerance && a.criticalCore &&
                                      a.uncovered && a.criticalUncovered && a.boundaryShortfall && a.fallbackComponents;
    GeoCoverageQualityEvaluation parsed;
    if (!readQuality(qualityJson(quality), parsed)) {
        return false;
    }
    if ((a.targetArea && quality.targetAreaM2 <= 0) || (a.numericalTolerance && quality.numericalToleranceM2 <= 0) ||
        (a.coveredArea && quality.coveredAreaM2 < 0) || (a.uncoveredArea && quality.uncoveredAreaM2 < 0) ||
        (a.criticalUncoveredArea && quality.criticalUncoveredAreaM2 < 0) ||
        (a.ratio && (quality.coverageRatio < 0 || quality.coverageRatio > 1))) {
        return false;
    }
    if (quality.status == CoverageQualityStatus::AssessmentError) {
        return !quality.passesRequirement && quality.error != CoverageQualityError::None;
    }
    if (!completeAvailability || quality.error != CoverageQualityError::None) {
        return false;
    }
    const double tolerance = quality.numericalToleranceM2;
    if (std::abs(quality.coveredAreaM2 + quality.uncoveredAreaM2 - quality.targetAreaM2) > tolerance ||
        quality.criticalUncoveredAreaM2 > quality.uncoveredAreaM2 + tolerance ||
        std::abs(quality.coverageRatio - quality.coveredAreaM2 / quality.targetAreaM2) > 1e-12) {
        return false;
    }
    switch (quality.status) {
        case CoverageQualityStatus::Complete:
            return quality.passesRequirement && quality.uncoveredAreaM2 <= tolerance;
        case CoverageQualityStatus::Acceptable:
            return quality.passesRequirement && quality.requirement == CoverageRequirement::Standard &&
                   quality.uncoveredAreaM2 > tolerance &&
                   (quality.policySemanticVersion != CoverageQualityPolicySemanticVersion ||
                    quality.coverageRatio >= StandardCoveragePolicy::minimumCoverageRatio) &&
                   quality.criticalUncoveredAreaM2 <= tolerance;
        case CoverageQualityStatus::Insufficient:
            return !quality.passesRequirement && quality.uncoveredAreaM2 > tolerance;
        case CoverageQualityStatus::AssessmentError:
            break;
    }
    return false;
}

CoverageQualityEvaluation metricsView(const GeoCoverageQualityEvaluation& quality)
{
    // Feed the frozen comparator its original numerical facts, without recomputing geographic geometry.
    CoverageQualityEvaluation view;
    view.status = quality.status;
    view.error = quality.error;
    view.policySemanticVersion = quality.policySemanticVersion;
    view.targetAreaM2 = quality.targetAreaM2;
    view.coveredAreaM2 = quality.coveredAreaM2;
    view.uncoveredAreaM2 = quality.uncoveredAreaM2;
    view.criticalUncoveredAreaM2 = quality.criticalUncoveredAreaM2;
    view.coverageRatio = quality.coverageRatio;
    view.numericalToleranceM2 = quality.numericalToleranceM2;
    return view;
}

bool sameEvaluationContext(const GeoCoverageQualityEvaluation& left, const GeoCoverageQualityEvaluation& right)
{
    return left.requirement == right.requirement && left.strategy == right.strategy &&
           left.policySemanticVersion == right.policySemanticVersion &&
           left.availability.targetArea == right.availability.targetArea && left.targetAreaM2 == right.targetAreaM2 &&
           left.availability.numericalTolerance == right.availability.numericalTolerance &&
           left.numericalToleranceM2 == right.numericalToleranceM2;
}

bool sameEvaluationFacts(const GeoCoverageQualityEvaluation& left, const GeoCoverageQualityEvaluation& right)
{
    // Runtime steps copy the preceding evaluation; compare persisted facts without reevaluation.
    auto leftFacts = qualityJson(left);
    auto rightFacts = qualityJson(right);
    leftFacts.remove("message");
    rightFacts.remove("message");
    return leftFacts == rightFacts;
}

bool validPath(const std::vector<GeoPoint>& path, const std::vector<PathLegRole>& roles, double coverage,
               double transit, double length, int turns, const MarineTask* task)
{
    if (path.size() < 2 || roles.size() != path.size() - 1 || turns < 0 || !std::ranges::all_of(path, validPoint) ||
        !std::isfinite(coverage) || coverage < 0 || !std::isfinite(transit) || transit < 0 || !std::isfinite(length) ||
        length <= 0 || std::abs(coverage + transit - length) > PathMetricsConsistencyToleranceM) {
        return false;
    }
    for (std::size_t i = 1; i < path.size(); ++i) {
        if ((path[i - 1].latitudeDeg == path[i].latitudeDeg && path[i - 1].longitudeDeg == path[i].longitudeDeg) ||
            enumText(roles[i - 1]).isEmpty()) {
            return false;
        }
    }
    // A mismatching task is never used to certify an old artifact's coordinate frame.
    if (!task) {
        return true;
    }
    const auto reference = GeoReference::create(task->region.coverageBoundary);
    if (!reference) {
        return false;
    }
    std::vector<Point2D> local;
    for (const auto& point : path) {
        const auto converted = reference->toLocal(point);
        if (!converted) {
            return false;
        }
        local.push_back(*converted);
    }
    const auto actual = calculatePlanningPathMetrics(local, roles);
    return actual && planningPathMetricsMatch(*actual, coverage, transit, length) && actual->turnCount == turns;
}

bool validReference(const std::optional<PlanningReference>& reference, const PlanningResult& result)
{
    if (!reference) {
        return true;
    }
    const auto& ref = *reference;
    const auto rangeValid = [&](std::size_t count) {
        return ref.index == 0 && ref.legCount > 0 && ref.firstLeg < count && ref.legCount <= count - ref.firstLeg;
    };
    switch (ref.kind) {
        case PlanningReferenceKind::CanonicalPathLegRange:
            return rangeValid(result.legRoles.size());
        case PlanningReferenceKind::DiagnosticCandidateLegRange:
            return result.outcome.diagnosticCandidate &&
                   rangeValid(result.outcome.diagnosticCandidate->legRoles.size());
        case PlanningReferenceKind::DiagnosticOverlay:
            return ref.index < result.outcome.diagnosticOverlays.size() && ref.firstLeg == 0 && ref.legCount == 0;
        case PlanningReferenceKind::CoverageResidual:
            if (!result.outcome.coverageQuality || ref.index != 0 || ref.firstLeg != 0 || ref.legCount != 0) {
                return false;
            }
            switch (ref.residual) {
                case PlanningResidualKind::Uncovered:
                    return result.outcome.coverageQuality->availability.uncovered;
                case PlanningResidualKind::CriticalUncovered:
                    return result.outcome.coverageQuality->availability.criticalUncovered;
                case PlanningResidualKind::BoundaryShortfall:
                    return result.outcome.coverageQuality->availability.boundaryShortfall;
            }
    }
    return false;
}

bool sourceMatchesIdentity(const PlanningResult& result, const PlanningInputIdentity& identity)
{
    if (!result.plannerSource) {
        return (result.status == PlanningStatus::InvalidInput ||
                (result.status == PlanningStatus::Failed && result.outcome.tier == SafetySolutionTier::D3)) &&
               identity.semantics.resolvedStrategy == PlanningSemantics{}.resolvedStrategy &&
               identity.semantics.strategyVersion == PlanningSemantics{}.strategyVersion;
    }
    const auto& source = *result.plannerSource;
    if (result.outcome.coverageQuality &&
        QString::fromStdString(result.outcome.coverageQuality->policySemanticVersion) !=
            identity.semantics.policyVersion) {
        return false;
    }
    return QString::fromStdString(source.resolvedStrategy.strategyId) == identity.semantics.resolvedStrategy &&
           QString::fromStdString(source.resolvedStrategy.semanticVersion) == identity.semantics.strategyVersion;
}

bool sourceMatchesTask(const PlannerSourceInfo& source, const MarineTask& task)
{
    if (source.requestedPlannerId != task.planner.plannerId ||
        source.requestedSweepMode != task.coverage.sweepAngleMode) {
        return false;
    }
    if (source.requestedSweepMode == SweepAngleMode::Manual) {
        double angle = std::fmod(task.coverage.sweepAngleDeg, 180.0);
        if (angle < 0.0) {
            angle += 180.0;
        }
        if (!std::isfinite(angle) || source.selectedSweepAngleDeg != angle || !source.sweepSemanticVersion.empty()) {
            return false;
        }
    } else if (source.sweepSemanticVersion != CoverageStrategySemantics::GlobalSweepVersion) {
        return false;
    }
    const auto& strategy = source.resolvedStrategy;
    if (task.planner.plannerId == CoverageStrategySemantics::SimpleMonotoneId) {
        return strategy.strategyId == CoverageStrategySemantics::SimpleMonotoneId &&
               strategy.semanticVersion == CoverageStrategySemantics::SimpleMonotoneVersion;
    }
    if (task.planner.plannerId == CoverageStrategySemantics::BoustrophedonId) {
        return strategy.strategyId == CoverageStrategySemantics::BoustrophedonId &&
               strategy.semanticVersion == CoverageStrategySemantics::BoustrophedonVersion;
    }
    return task.planner.plannerId != CoverageStrategySemantics::AutoPlannerId ||
           ((strategy.strategyId == CoverageStrategySemantics::SimpleMonotoneId &&
             strategy.semanticVersion == CoverageStrategySemantics::SimpleMonotoneVersion) ||
            (strategy.strategyId == CoverageStrategySemantics::BoustrophedonId &&
             strategy.semanticVersion == CoverageStrategySemantics::BoustrophedonVersion));
}
}  // namespace

bool PlanningArtifactCodec::validateResult(const PlanningResult& result, const MarineTask* matchingTask, QString& error)
{
    const auto invalid = [&]() {
        error = QStringLiteral("Invalid IntegratedV05 planning result certification or structure");
        return false;
    };
    const auto& out = result.outcome;
    if (enumText(result.status).isEmpty() || enumText(result.error).isEmpty() || enumText(out.readiness).isEmpty() ||
        (out.tier && enumText(*out.tier).isEmpty()) || !std::isfinite(result.selectedSweepAngleDeg) ||
        result.selectedSweepAngleDeg < 0 || result.selectedSweepAngleDeg >= 180 || result.cellCount < 0 ||
        result.turnCount < 0) {
        return invalid();
    }
    const bool canonical = result.status == PlanningStatus::Success;
    if (canonical) {
        if (result.error != CoveragePlanningError::None || !out.selectedCandidateIndex || !out.coverageQuality ||
            !validQuality(*out.coverageQuality) || result.cellCount < 1 ||
            (out.tier != SafetySolutionTier::D0 && out.tier != SafetySolutionTier::D1) ||
            out.canonicalLegAssessments.size() != result.legRoles.size() || out.diagnosticCandidate ||
            !out.diagnosticOverlays.empty() ||
            !validPath(result.path, result.legRoles, result.coverageLengthM, result.transitLengthM, result.pathLengthM,
                       result.turnCount, matchingTask)) {
            return invalid();
        }
        const bool preferred = std::ranges::all_of(out.canonicalLegAssessments,
                                                   [](auto leg) { return leg == SafetyLegClass::PreferredSafe; });
        if (preferred != (out.tier == SafetySolutionTier::D0) ||
            !std::ranges::all_of(out.canonicalLegAssessments, [](auto leg) {
                return leg == SafetyLegClass::PreferredSafe || leg == SafetyLegClass::HardSafeWarning;
            })) {
            return invalid();
        }
        const auto& quality = *out.coverageQuality;
        if (matchingTask && quality.requirement != matchingTask->coverage.coverageRequirement) {
            return invalid();
        }
        const bool pass = quality.status == CoverageQualityStatus::Complete ||
                          (quality.status == CoverageQualityStatus::Acceptable && quality.passesRequirement);
        const auto requiredReadiness = pass ? (preferred ? MissionReadiness::Ready : MissionReadiness::ReadyWithWarning)
                                            : MissionReadiness::ReviewRequired;
        if (out.readiness != requiredReadiness) {
            return invalid();
        }
    } else {
        if (!result.path.empty() || !result.legRoles.empty() || !out.canonicalLegAssessments.empty() ||
            result.coverageLengthM != 0 || result.transitLengthM != 0 || result.pathLengthM != 0 ||
            result.turnCount != 0 || result.cellCount != 0 || out.selectedCandidateIndex || out.coverageQuality ||
            out.repair.attempted || out.repair.applied || !out.repair.components.empty()) {
            return invalid();
        }
        if (result.status == PlanningStatus::InvalidInput) {
            if (out.readiness != MissionReadiness::None || out.tier || out.diagnosticCandidate ||
                !out.diagnosticOverlays.empty()) {
                return invalid();
            }
        } else if (out.readiness != MissionReadiness::DiagnosticOnly ||
                   (out.tier != SafetySolutionTier::D2 && out.tier != SafetySolutionTier::D3)) {
            return invalid();
        }
    }
    if (out.diagnosticCandidate) {
        const auto& diagnostic = *out.diagnosticCandidate;
        if (out.tier != SafetySolutionTier::D2 || diagnostic.identity.empty() ||
            diagnostic.legAssessments.size() != diagnostic.legRoles.size() ||
            !validPath(diagnostic.path, diagnostic.legRoles, diagnostic.coverageLengthM, diagnostic.transitLengthM,
                       diagnostic.pathLengthM, diagnostic.turnCount, matchingTask) ||
            !std::ranges::all_of(diagnostic.legAssessments, [](auto leg) { return !enumText(leg).isEmpty(); }) ||
            !std::ranges::any_of(diagnostic.legAssessments, [](auto leg) {
                return leg == SafetyLegClass::ExecutionUnsafe || leg == SafetyLegClass::HardUnsafe;
            })) {
            return invalid();
        }
    } else if (out.tier == SafetySolutionTier::D2) {
        return invalid();
    }
    if (out.tier == SafetySolutionTier::D3 && (out.diagnosticCandidate || out.diagnosticOverlays.empty())) {
        return invalid();
    }
    for (const auto& overlay : out.diagnosticOverlays) {
        GeoPolygonRegionSet parsed;
        if (enumText(overlay.kind).isEmpty() || overlay.explanation.empty() ||
            !readRegions(regionsJson(overlay.geometry), parsed)) {
            return invalid();
        }
    }
    if (result.plannerSource) {
        const auto& source = *result.plannerSource;
        if (source.requestedPlannerId.empty() || source.resolvedStrategy.strategyId.empty() ||
            source.resolvedStrategy.semanticVersion.empty() || enumText(source.resolutionStatus).isEmpty() ||
            enumText(source.resolutionReason).isEmpty() || enumText(source.requestedSweepMode).isEmpty() ||
            source.selectedSweepAngleDeg != result.selectedSweepAngleDeg ||
            (matchingTask && !sourceMatchesTask(source, *matchingTask)) ||
            (canonical &&
             (source.resolutionStatus != PlannerResolutionStatus::Resolved ||
              (out.coverageQuality->strategy != source.resolvedStrategy) ||
              (matchingTask && out.coverageQuality->policySemanticVersion != CoverageQualityPolicySemanticVersion)))) {
            return invalid();
        }
    } else if (result.status != PlanningStatus::InvalidInput &&
               !(result.status == PlanningStatus::Failed && out.tier == SafetySolutionTier::D3)) {
        return invalid();
    }
    const auto& repair = out.repair;
    if (enumText(repair.reason).isEmpty() || repair.applied != !repair.components.empty() ||
        (repair.applied && !repair.attempted) ||
        (!canonical && repair.reason != CoverageRepairReason::NoCanonicalCandidate) ||
        (repair.reason == CoverageRepairReason::InitialPolicyPass && (repair.attempted || repair.applied)) ||
        (repair.reason == CoverageRepairReason::AssessmentUnavailable && (repair.attempted || repair.applied)) ||
        (out.coverageQuality && out.coverageQuality->status == CoverageQualityStatus::AssessmentError &&
         repair.reason != CoverageRepairReason::AssessmentUnavailable)) {
        return invalid();
    }
    std::set<std::uint32_t> ids;
    for (std::size_t index = 0; index < repair.components.size(); ++index) {
        const auto& component = repair.components[index];
        if (!ids.insert(component.componentId).second || component.componentPath.size() < 4 ||
            component.entryIndex >= component.componentPath.size() - 1 ||
            !std::ranges::all_of(component.componentPath, validPoint) ||
            component.componentPath.front().latitudeDeg != component.componentPath.back().latitudeDeg ||
            component.componentPath.front().longitudeDeg != component.componentPath.back().longitudeDeg ||
            !std::isfinite(component.transitionCostM) || component.transitionCostM < 0 ||
            !validQuality(component.before) || !validQuality(component.after) ||
            !sameEvaluationContext(component.before, *out.coverageQuality) ||
            !sameEvaluationContext(component.after, *out.coverageQuality) ||
            component.before.status != CoverageQualityStatus::Insufficient ||
            compareCoverageQuality(metricsView(component.after), metricsView(component.before)) !=
                CoverageQualityComparison::Better ||
            !std::isfinite(component.pathLengthBeforeM) || !std::isfinite(component.pathLengthAfterM) ||
            component.pathLengthBeforeM <= 0 || component.pathLengthAfterM < component.pathLengthBeforeM ||
            component.turnCountBefore < 0 || component.turnCountAfter < 0 ||
            (index > 0 && (repair.components[index - 1].after.passesRequirement ||
                           !sameEvaluationFacts(repair.components[index - 1].after, component.before) ||
                           repair.components[index - 1].pathLengthAfterM != component.pathLengthBeforeM ||
                           repair.components[index - 1].turnCountAfter != component.turnCountBefore))) {
            return invalid();
        }
    }
    if (repair.applied) {
        const auto& last = repair.components.back();
        if (std::abs(last.pathLengthAfterM - result.pathLengthM) > PathMetricsConsistencyToleranceM ||
            last.turnCountAfter != result.turnCount || !sameEvaluationFacts(last.after, *out.coverageQuality) ||
            repair.reason != (last.after.passesRequirement ? CoverageRepairReason::AppliedPolicyPass
                                                           : CoverageRepairReason::AppliedStillInsufficient)) {
            return invalid();
        }
    }
    std::set<PlanningIssueCode> codes;
    for (const auto& issue : out.issues) {
        if (enumText(issue.code).isEmpty() || enumText(issue.severity).isEmpty() || issue.message.empty() ||
            !codes.insert(issue.code).second || !validReference(issue.reference, result)) {
            return invalid();
        }
    }
    if (!std::ranges::is_sorted(out.issues, {}, &PlanningIssue::code)) {
        return invalid();
    }
    const auto has = [&](PlanningIssueCode code) { return codes.contains(code); };
    if (canonical && !has(PlanningIssueCode::IngressNotAssessed)) {
        return invalid();
    }
    if ((out.tier == SafetySolutionTier::D1) != has(PlanningIssueCode::PreferredSafetyViolated) ||
        repair.applied != has(PlanningIssueCode::CoverageRepairApplied) ||
        (result.plannerSource && result.plannerSource->escalated) != has(PlanningIssueCode::PlannerEscalated)) {
        return invalid();
    }
    if (out.coverageQuality) {
        const auto& quality = *out.coverageQuality;
        if ((quality.status == CoverageQualityStatus::AssessmentError) !=
                has(PlanningIssueCode::CoverageAssessmentFailed) ||
            (quality.status == CoverageQualityStatus::Insufficient) !=
                has(PlanningIssueCode::CoverageBelowRequirement) ||
            (repair.attempted && quality.status == CoverageQualityStatus::Insufficient) !=
                has(PlanningIssueCode::CoverageRepairInsufficient)) {
            return invalid();
        }
    }
    if (!out.coverageQuality &&
        (has(PlanningIssueCode::CoverageBelowRequirement) || has(PlanningIssueCode::CoverageAssessmentFailed) ||
         has(PlanningIssueCode::CriticalCoverageGap) || has(PlanningIssueCode::CoverageBoundaryShortfall) ||
         has(PlanningIssueCode::CoverageRepairInsufficient))) {
        return invalid();
    }
    if ((out.tier == SafetySolutionTier::D2) != has(PlanningIssueCode::UnsafeDiagnosticCandidate)) {
        return invalid();
    }
    std::set<PlanningSuggestionCode> suggestionCodes;
    for (const auto& suggestion : out.suggestions) {
        if (enumText(suggestion.code).isEmpty() || !suggestionCodes.insert(suggestion.code).second ||
            !has(suggestion.issue) || suggestion.message.empty() || !validReference(suggestion.reference, result)) {
            return invalid();
        }
    }
    if (!std::ranges::is_sorted(out.suggestions, {}, &PlanningSuggestion::code)) {
        return invalid();
    }
    error.clear();
    return true;
}

bool PlanningArtifactCodec::save(const PlanningArtifact& artifact, const MarineTask& task, QJsonObject& json,
                                 QString& error)
{
    if (artifact.resultContract != PlanningResultContract::IntegratedV05 ||
        !sourceMatchesIdentity(artifact.result, artifact.identity) ||
        !validateResult(artifact.result, matchesCurrentInput(artifact, task) ? &task : nullptr, error)) {
        if (error.isEmpty()) {
            error = QStringLiteral("Unsupported or inconsistent integrated planning artifact");
        }
        return false;
    }
    const auto& result = artifact.result;
    json = {{"version", 3},
            {"resultContract", "IntegratedV05"},
            {"inputIdentity", artifact.identity.toJson()},
            {"planningStatus", enumText(result.status)},
            {"planningError", enumText(result.error)},
            {"generatedPath", pathJson(result.path)},
            {"legRoles", enumArray(result.legRoles)},
            {"coverageLengthM", result.coverageLengthM},
            {"transitLengthM", result.transitLengthM},
            {"pathLengthM", result.pathLengthM},
            {"selectedSweepAngleDeg", result.selectedSweepAngleDeg},
            {"cellCount", result.cellCount},
            {"turnCount", result.turnCount},
            {"planningMessage", QString::fromStdString(result.message)},
            {"plannerSource", sourceJson(result.plannerSource)},
            {"outcome", outcomeJson(result.outcome)}};
    return true;
}

bool PlanningArtifactCodec::load(const QJsonObject& json, const MarineTask& task, PlanningArtifact& artifact,
                                 QString& error)
{
    PlanningArtifact parsed;
    parsed.resultContract = PlanningResultContract::IntegratedV05;
    // Parse identity before any task-referenced metric validation.
    if (!json.value("version").isDouble() || json.value("version").toDouble() != 3 ||
        json.value("resultContract").toString() != QStringLiteral("IntegratedV05") ||
        !json.value("inputIdentity").isObject() ||
        !PlanningInputIdentity::fromJson(json.value("inputIdentity").toObject(), parsed.identity, error)) {
        error = QStringLiteral("Invalid IntegratedV05 artifact header or identity");
        return false;
    }
    parsed.stale = !parsed.identity.matchesSupported(task);
    auto& result = parsed.result;
    if (!readEnum(json.value("planningStatus"), result.status) ||
        !readEnum(json.value("planningError"), result.error) || !readPath(json.value("generatedPath"), result.path) ||
        !readEnums(json.value("legRoles"), result.legRoles) ||
        !number(json.value("coverageLengthM"), result.coverageLengthM) ||
        !number(json.value("transitLengthM"), result.transitLengthM) ||
        !number(json.value("pathLengthM"), result.pathLengthM) ||
        !number(json.value("selectedSweepAngleDeg"), result.selectedSweepAngleDeg) ||
        !integer(json.value("cellCount"), result.cellCount) || !integer(json.value("turnCount"), result.turnCount) ||
        !string(json.value("planningMessage"), result.message) ||
        !readSource(json.value("plannerSource"), result.plannerSource) ||
        !readOutcome(json.value("outcome"), result.outcome) || !sourceMatchesIdentity(result, parsed.identity) ||
        !validateResult(result, matchesCurrentInput(parsed, task) ? &task : nullptr, error)) {
        error = QStringLiteral("Malformed IntegratedV05 artifact result");
        return false;
    }
    parsed.stale = !matchesCurrentInput(parsed, task);
    artifact = std::move(parsed);
    error.clear();
    return true;
}

bool PlanningArtifactCodec::matchesCurrentInput(const PlanningArtifact& artifact, const MarineTask& task)
{
    // Unknown saved sweep semantics remain preservable stale data, never a current certificate.
    if (artifact.result.plannerSource && !artifact.result.plannerSource->sweepSemanticVersion.empty() &&
        artifact.result.plannerSource->sweepSemanticVersion != CoverageStrategySemantics::GlobalSweepVersion) {
        return false;
    }
    // Input rejection or early solver failure may precede strategy resolution. Restore only the exact
    // current unresolved identity; neither outcome can reach the executable gate.
    if (artifact.resultContract == PlanningResultContract::IntegratedV05 && !artifact.result.plannerSource &&
        (artifact.result.status == PlanningStatus::InvalidInput ||
         (artifact.result.status == PlanningStatus::Failed &&
          artifact.result.outcome.tier == SafetySolutionTier::D3))) {
        return artifact.identity.matches(task, PlanningSemantics{});
    }
    return artifact.identity.matchesSupported(task);
}

bool PlanningArtifactCodec::uploadAllowed(const PlanningArtifact& artifact, const MarineTask& currentTask,
                                          QString& error)
{
    if (artifact.resultContract != PlanningResultContract::IntegratedV05 || artifact.stale ||
        !matchesCurrentInput(artifact, currentTask) || !sourceMatchesIdentity(artifact.result, artifact.identity) ||
        artifact.result.status != PlanningStatus::Success ||
        (artifact.result.outcome.readiness != MissionReadiness::Ready &&
         artifact.result.outcome.readiness != MissionReadiness::ReadyWithWarning)) {
        error = QStringLiteral("Marine result is not current, integrated and ready for upload");
        return false;
    }
    return validateResult(artifact.result, &currentTask, error);
}

}  // namespace Marine
