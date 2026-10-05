#include "IntegratedPlanningResultTest.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <set>
#include <utility>

#include "ArduPilotMissionAdapter.h"
#include "AutoCoveragePlanner.h"
#include "BoustrophedonCoveragePlanner.h"
#include "CoverageGeometry.h"
#include "CoverageInspectionComplexItem.h"
#include "CoverageProblemValidator.h"
#include "CoverageQualityEvaluator.h"
#include "CoverageRepair.h"
#include "CoverageSafety.h"
#include "CoverageTaskAdapter.h"
#include "Geometry/MarineGeometry.h"
#include "IntegratedPlanningResult.h"
#include "MarinePlanContext.h"
#include "MarineTaskJsonCodec.h"
#include "MissionItem.h"
#include "PlanningArtifactCodec.h"
#include "SimpleMonotoneCoveragePlanner.h"
#include "V05ExecutionFixtures.h"

using namespace Marine;

namespace {
Polygon2D rectangle(double left, double bottom, double right, double top)
{
    return {.vertices = {{left, bottom}, {right, bottom}, {right, top}, {left, top}}};
}

CoveragePlanningProblem problem(double hard = 0, double preferred = 0, double execution = 0)
{
    CoveragePlanningProblem p;
    p.region.coverageBoundary = rectangle(0, 0, 20, 20);
    p.region.navigationBoundary = p.region.coverageBoundary;
    p.swathWidthM = 5;
    p.safety = {hard, preferred};
    p.executionSafety.executionMarginM = execution;
    p.coverageRequirement = CoverageRequirement::Strict;
    p.sweepAngleMode = SweepAngleMode::Manual;
    p.requestedSweepAngleDeg = 90;
    return p;
}

CoveragePlanningProblem unsupportedProblem()
{
    auto p = problem(0, 0, 100);
    p.region.navigationBoundary = rectangle(-5, -5, 25, 25);
    p.region.noGoRegions = {rectangle(8, 8, 12, 12)};
    return p;
}

MarineTask taskFor(const CoveragePlanningProblem& p, std::string planner = CoverageStrategySemantics::AutoPlannerId)
{
    MarineTask task;
    task.id = "integrated-fixture";
    task.safety = p.safety;
    task.coverage.swathWidthM = p.swathWidthM;
    task.coverage.coverageRequirement = p.coverageRequirement;
    task.coverage.sweepAngleMode = p.sweepAngleMode;
    task.coverage.sweepAngleDeg = p.requestedSweepAngleDeg;
    task.planner.executionSafety = p.executionSafety;
    task.planner.plannerId = std::move(planner);
    const auto reference = GeoReference::create(GeoPoint{38, 121, 0});
    const auto convert = [&](const Polygon2D& input) {
        GeoPolygon output;
        for (const auto& point : input.vertices) {
            output.vertices.push_back(*reference->toGeo(point));
        }
        return output;
    };
    task.region.coverageBoundary = convert(p.region.coverageBoundary);
    task.region.navigationBoundary = convert(p.region.navigationBoundary);
    for (const auto& hole : p.region.noGoRegions) {
        task.region.noGoRegions.push_back(convert(hole));
    }
    return task;
}

PlanningArtifact artifactFor(const CoveragePlanningProblem& p, const CoveragePlanningSolution& solution,
                             std::string unresolvedPlanner = CoverageStrategySemantics::AutoPlannerId)
{
    const auto task =
        taskFor(p, solution.plannerSource ? solution.plannerSource->requestedPlannerId : unresolvedPlanner);
    const auto reference = GeoReference::create(GeoPoint{38, 121, 0});
    PlanningSemantics semantics;
    if (solution.plannerSource) {
        semantics.resolvedStrategy = QString::fromStdString(solution.plannerSource->resolvedStrategy.strategyId);
        semantics.strategyVersion = QString::fromStdString(solution.plannerSource->resolvedStrategy.semanticVersion);
    }
    return {CoverageTaskAdapter::toPlanningResult(solution, *reference),
            *PlanningInputIdentity::fromTask(task, semantics), false, PlanningResultContract::IntegratedV05};
}

bool hasIssue(const CoveragePlanningSolution& result, PlanningIssueCode code)
{
    return std::ranges::find(result.outcome.issues, code, &PlanningIssue::code) != result.outcome.issues.end();
}

void verifyRejected(const PlanningArtifact& artifact, const MarineTask& task)
{
    QObject parent;
    QList<MissionItem*> items{new MissionItem(&parent)};
    const auto existing = items.front();
    int sequence = 7;
    QString error;
    QVERIFY(!ArduPilotMissionAdapter::appendWaypoints(artifact, task, items, &parent, sequence, error));
    QCOMPARE(items.size(), 1);
    QCOMPARE(items.front(), existing);
    QCOMPARE(sequence, 7);
    QVERIFY(!error.isEmpty());
}

void verifyAllowed(const PlanningArtifact& artifact, const MarineTask& task)
{
    QObject parent;
    QList<MissionItem*> items;
    int sequence = 7;
    QString error;
    QVERIFY2(ArduPilotMissionAdapter::appendWaypoints(artifact, task, items, &parent, sequence, error),
             qPrintable(error));
    QCOMPARE(items.size(), static_cast<qsizetype>(artifact.result.path.size()));
    QCOMPARE(sequence, 7 + static_cast<int>(artifact.result.path.size()));
    for (int i = 0; i < items.size(); ++i) {
        QCOMPARE(items[i]->command(), MAV_CMD_NAV_WAYPOINT);
        QCOMPARE(items[i]->sequenceNumber(), 7 + i);
        QCOMPARE(items[i]->param5(), artifact.result.path[i].latitudeDeg);
        QCOMPARE(items[i]->param6(), artifact.result.path[i].longitudeDeg);
        QCOMPARE(items[i]->param7(), 0.0);
    }
}

QJsonObject serialized(const PlanningArtifact& artifact, const MarineTask& task)
{
    QJsonObject json;
    QString error;
    if (!PlanningArtifactCodec::save(artifact, task, json, error)) {
        qWarning() << error;
    }
    return json;
}

void verifyRoundTrip(const CoveragePlanningProblem& p, const CoveragePlanningSolution& solution)
{
    const auto task = taskFor(p, solution.plannerSource->requestedPlannerId);
    const auto artifact = artifactFor(p, solution);
    const auto json = serialized(artifact, task);
    QVERIFY(!json.isEmpty());
    QCOMPARE(json.value("resultContract").toString(), QStringLiteral("IntegratedV05"));
    QCOMPARE(json.value("version").toInt(), 3);
    PlanningArtifact restored;
    QString error;
    QVERIFY2(PlanningArtifactCodec::load(json, task, restored, error), qPrintable(error));
    QVERIFY(!restored.stale);
    QCOMPARE(serialized(restored, task),
             json);  // Includes every metric, coordinate, availability and provenance field.
}

QJsonObject complexObject(QJsonObject json, const MarineTask& task)
{
    json.insert("type", "ComplexItem");
    json.insert("complexItemType", "coverageInspection");
    json.insert("taskId", QString::fromStdString(task.id));
    return json;
}

void verifyScenarioIdentity(const char* canonical, const char* expectedSha256)
{
    const QByteArray actual = QCryptographicHash::hash(QByteArray(canonical), QCryptographicHash::Sha256).toHex();
    QCOMPARE(actual, QByteArray(expectedSha256));
}

void verifySITLBinding(const QString& scenario, const QByteArray& canonical, const QByteArray& fixtureHash,
                       const CoveragePlanningProblem& expected)
{
    const QString root = qEnvironmentVariable("QGC_V05_10_R1_BINDING_DIR");
    if (root.isEmpty()) {
        return;
    }
    const QDir directory(QDir(root).filePath(scenario));
    const auto bytes = [&](const QString& name) {
        QFile file(directory.filePath(name));
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
    };
    const auto digest = [](const QByteArray& value) {
        return QCryptographicHash::hash(value, QCryptographicHash::Sha256).toHex();
    };
    const auto evidence = QJsonDocument::fromJson(bytes(QStringLiteral("scenario.json"))).object();
    QVERIFY(!evidence.isEmpty());
    QCOMPARE(evidence.value("canonical").toString().toUtf8(), canonical);
    QCOMPARE(evidence.value("canonicalSha256").toString().toLatin1(), fixtureHash);
    QCOMPARE(evidence.value("sourceHead").toString(), QStringLiteral("cc70699c25f7f396907cf937c95f41daa9ff64b7"));
    QFile binary(QCoreApplication::applicationFilePath());
    QVERIFY(binary.open(QIODevice::ReadOnly));
    QCOMPARE(evidence.value("binarySha256").toString().toLatin1(), digest(binary.readAll()));
    const auto taskBytes = bytes(QStringLiteral("task.json"));
    const auto artifactBytes = bytes(QStringLiteral("artifact.json"));
    QCOMPARE(digest(taskBytes), evidence.value("taskSha256").toString().toLatin1());
    QCOMPARE(digest(artifactBytes), evidence.value("artifactSha256").toString().toLatin1());
    const auto taskJson = QJsonDocument::fromJson(taskBytes).object();
    const auto artifactJson = QJsonDocument::fromJson(artifactBytes).object();
    MarineTask task;
    PlanningArtifact artifact;
    QString error;
    QVERIFY2(MarineTaskJsonCodec::load(taskJson, task, error), qPrintable(error));
    QCOMPARE(task.id, "V05-10-R1-" + scenario.toStdString());
    QVERIFY2(PlanningArtifactCodec::load(artifactJson, task, artifact, error), qPrintable(error));
    QVERIFY(!artifact.stale);
    QVERIFY(PlanningArtifactCodec::matchesCurrentInput(artifact, task));
    QCOMPARE(artifact.identity.toJson(), evidence.value("taskIdentity").toObject());
    QCOMPARE(serialized(artifact, task), artifactJson);

    CoveragePlanningProblem realized;
    std::optional<GeoReference> reference;
    CoveragePlanningError buildError;
    QVERIFY(CoverageTaskAdapter::buildProblem(task, realized, reference, buildError));
    QCOMPARE(realized.swathWidthM, expected.swathWidthM);
    QCOMPARE(realized.safety.hardSafetyMarginM, expected.safety.hardSafetyMarginM);
    QCOMPARE(realized.safety.preferredSafetyMarginM, expected.safety.preferredSafetyMarginM);
    QCOMPARE(realized.executionSafety.executionMarginM, expected.executionSafety.executionMarginM);
    QCOMPARE(realized.coverageRequirement, expected.coverageRequirement);
    QCOMPARE(realized.sweepAngleMode, expected.sweepAngleMode);
    QCOMPARE(realized.requestedSweepAngleDeg, expected.requestedSweepAngleDeg);
    QVERIFY(realized.region.noGoRegions.empty());
    const auto center = [](const Polygon2D& polygon) {
        Point2D result;
        for (const auto& vertex : polygon.vertices) {
            result.xM += vertex.xM / polygon.vertices.size();
            result.yM += vertex.yM / polygon.vertices.size();
        }
        return result;
    };
    const auto actualCenter = center(realized.region.coverageBoundary);
    const auto wantedCenter = center(expected.region.coverageBoundary);
    const Point2D offset{actualCenter.xM - wantedCenter.xM, actualCenter.yM - wantedCenter.yM};
    for (const auto pair : {std::pair{&realized.region.coverageBoundary, &expected.region.coverageBoundary},
                            std::pair{&realized.region.navigationBoundary, &expected.region.navigationBoundary}}) {
        QCOMPARE(pair.first->vertices.size(), pair.second->vertices.size());
        for (const auto& actual : pair.first->vertices) {
            double nearestM = std::numeric_limits<double>::infinity();
            for (const auto& wanted : pair.second->vertices) {
                nearestM = std::min(nearestM,
                                    std::hypot(actual.xM - wanted.xM - offset.xM, actual.yM - wanted.yM - offset.yM));
            }
            QVERIFY2(nearestM <= 1e-3,
                     qPrintable(QStringLiteral("Fixture vertex difference %1 m").arg(nearestM, 0, 'g', 12)));
        }
    }
    const auto solution = AutoCoveragePlanner{}.plan(realized);
    QCOMPARE(solution.status, artifact.result.status);
    QCOMPARE(solution.outcome.readiness, artifact.result.outcome.readiness);
    QCOMPARE(solution.outcome.tier, artifact.result.outcome.tier);
    QVERIFY(solution.outcome.coverageQuality && solution.outcome.coverageQuality->passesRequirement);
    QCOMPARE(solution.path.size(), artifact.result.path.size());
    for (std::size_t index = 0; index < solution.path.size(); ++index) {
        const auto actual = reference->toLocal(artifact.result.path[index]);
        QVERIFY(actual.has_value());
        QVERIFY(std::hypot(actual->xM - solution.path[index].xM, actual->yM - solution.path[index].yM) <= 1e-3);
    }
    verifyAllowed(artifact, task);
}

std::pair<std::vector<Point2D>, std::vector<PathLegRole>> lanePath(double width, const std::vector<double>& lanes)
{
    std::vector<Point2D> path;
    std::vector<PathLegRole> roles;
    if (lanes.empty()) {
        return {path, roles};
    }
    path.push_back({0, lanes.front()});
    for (std::size_t i = 0; i < lanes.size(); ++i) {
        const bool leftToRight = i % 2 == 0;
        path.push_back({leftToRight ? width : 0.0, lanes[i]});
        roles.push_back(PathLegRole::Coverage);
        if (i + 1 < lanes.size()) {
            path.push_back({leftToRight ? width : 0.0, lanes[i + 1]});
            roles.push_back(PathLegRole::Transit);
        }
    }
    return {path, roles};
}

class ForbiddenPlanner final : public ICoveragePlanner
{
public:
    explicit ForbiddenPlanner(std::string plannerId = CoverageStrategySemantics::AutoPlannerId,
                              std::string version = CoverageStrategySemantics::AutoPlannerVersion)
        : _id(std::move(plannerId)), _version(std::move(version))
    {}

    mutable int calls = 0;

    std::string id() const override { return _id; }

    std::string semanticVersion() const override { return _version; }

    std::string displayName() const override { return "Restore must not call this planner"; }

    CoveragePlanningSolution plan(const CoveragePlanningProblem&) const override
    {
        ++calls;
        return {};
    }

private:
    std::string _id;
    std::string _version;
};
}  // namespace

void IntegratedPlanningResultTest::_testT01()
{
    const auto p = problem();
    const auto result = AutoCoveragePlanner{}.plan(p);
    QCOMPARE(result.status, PlanningStatus::Success);
    QCOMPARE(result.outcome.readiness, MissionReadiness::Ready);
    QCOMPARE(result.outcome.tier, SafetySolutionTier::D0);
    QVERIFY(hasIssue(result, PlanningIssueCode::IngressNotAssessed));
    QCOMPARE(result.outcome.canonicalLegAssessments.size(), result.path.size() - 1);
    verifyAllowed(artifactFor(p, result), taskFor(p));
}

void IntegratedPlanningResultTest::_testT02()
{
    const auto p = problem(0, 1);
    const auto result = AutoCoveragePlanner{}.plan(p);
    QCOMPARE(result.outcome.readiness, MissionReadiness::ReadyWithWarning);
    QCOMPARE(result.outcome.tier, SafetySolutionTier::D1);
    QVERIFY(hasIssue(result, PlanningIssueCode::PreferredSafetyViolated));
    verifyAllowed(artifactFor(p, result), taskFor(p));
    QCOMPARE(p.safety.preferredSafetyMarginM, 1.0);
    QCOMPARE(p.executionSafety.executionMarginM, 0.0);
}

void IntegratedPlanningResultTest::_testT03()
{
    const auto p = problem(2, 2);
    const auto result = AutoCoveragePlanner{}.plan(p);
    QCOMPARE(result.status, PlanningStatus::Success);
    QCOMPARE(result.outcome.readiness, MissionReadiness::ReviewRequired);
    QVERIFY(!result.path.empty());
    QCOMPARE(result.outcome.coverageQuality->status, CoverageQualityStatus::Insufficient);
    QVERIFY(!result.outcome.coverageQuality->residual.uncoveredRegion.empty());
    QVERIFY(hasIssue(result, PlanningIssueCode::CoverageBelowRequirement));
    QVERIFY(hasIssue(result, PlanningIssueCode::CoverageRepairInsufficient));
    verifyRejected(artifactFor(p, result), taskFor(p));
}

void IntegratedPlanningResultTest::_testT04()
{
    auto p = problem();
    p.swathWidthM = 1e150;
    const auto result = AutoCoveragePlanner{}.plan(p);
    QCOMPARE(result.status, PlanningStatus::Success);
    QCOMPARE(result.outcome.readiness, MissionReadiness::ReviewRequired);
    QCOMPARE(result.outcome.coverageQuality->status, CoverageQualityStatus::AssessmentError);
    QCOMPARE(evaluateSafetyCandidate(buildSafetyTrackRegions(p.region, p.safety, p.executionSafety), result.path).error,
             CoveragePlanningError::None);
    QVERIFY(hasIssue(result, PlanningIssueCode::CoverageAssessmentFailed));
    QVERIFY(!hasIssue(result, PlanningIssueCode::CoverageBelowRequirement));
    QVERIFY(!result.outcome.repair.attempted);
    QVERIFY(!result.outcome.repair.applied);
    verifyRejected(artifactFor(p, result), taskFor(p));
}

void IntegratedPlanningResultTest::_testT05()
{
    auto p = problem();
    p.swathWidthM = 1e150;
    const auto result = AutoCoveragePlanner{}.plan(p);
    verifyRoundTrip(p, result);
    const auto task = taskFor(p);
    const auto json = complexObject(serialized(artifactFor(p, result), task), task);
    const auto quality = json.value("outcome").toObject().value("coverageQuality").toObject();
    QVERIFY(quality.value("targetAreaM2").toObject().value("available").toBool());
    for (const char* name :
         {"coveredAreaM2", "uncoveredAreaM2", "coverageRatio", "criticalUncoveredAreaM2", "criticalCoverageCore",
          "uncoveredRegion", "criticalUncoveredRegion", "boundaryShortfallRegion"}) {
        QVERIFY(!quality.value(name).toObject().value("available").toBool());
        QVERIFY(quality.value(name).toObject().value("value").isNull());
    }
    MarinePlanContext context(planController());
    context.addTask(task);
    auto planner = std::make_shared<ForbiddenPlanner>();
    QVERIFY(context.plannerRegistry().registerPlanner(planner));
    CoverageInspectionComplexItem item(planController(), false, &context);
    QString error;
    QVERIFY2(item.load(json, 0, error), qPrintable(error));
    QCOMPARE(planner->calls, 0);
    QCOMPARE(item.planningState(), CoverageInspectionComplexItem::Planned);
    QCOMPARE(item.planningResult().outcome.coverageQuality->availability, result.outcome.coverageQuality->availability);
    QJsonArray saved;
    item.save(saved);
    QCOMPARE(saved.first().toObject(), json);
}

void IntegratedPlanningResultTest::_testT06()
{
    auto p = problem(0, 0, 30);
    p.region.navigationBoundary = rectangle(-5, -5, 25, 25);
    const auto result = AutoCoveragePlanner{}.plan(p);
    QCOMPARE(result.status, PlanningStatus::Failed);
    QCOMPARE(result.outcome.readiness, MissionReadiness::DiagnosticOnly);
    QCOMPARE(result.outcome.tier, SafetySolutionTier::D2);
    QVERIFY(result.path.empty());
    QVERIFY(result.outcome.diagnosticCandidate);
    const auto geometry = buildCoverageGeometry(p.region);
    const auto& diagnostic = *result.outcome.diagnosticCandidate;
    QCOMPARE(diagnostic.legAssessments.size(), diagnostic.path.size() - 1);
    for (std::size_t i = 1; i < diagnostic.path.size(); ++i) {
        QVERIFY(Geometry::segmentInsidePolygonRegionForValidatedGeometry(geometry.geometry.rawNavigationFreeSpace,
                                                                         diagnostic.path[i - 1], diagnostic.path[i]));
    }
    QVERIFY(hasIssue(result, PlanningIssueCode::ExecutionReserveUnavailable));
    QVERIFY(hasIssue(result, PlanningIssueCode::UnsafeDiagnosticCandidate));
    verifyRejected(artifactFor(p, result), taskFor(p));
    // Auto sweep still resolves once, and Auto delegates once; diagnostics do not change H/P/E.
    p.sweepAngleMode = SweepAngleMode::Auto;
    const auto automatic = AutoCoveragePlanner{}.plan(p);
    QCOMPARE(automatic.outcome.tier, SafetySolutionTier::D2);
    QVERIFY(!automatic.plannerSource->escalated);
    QCOMPARE(automatic.plannerSource->requestedSweepMode, SweepAngleMode::Auto);
    verifyRoundTrip(p, automatic);
    auto holed = unsupportedProblem();
    holed.safety = {20, 20};
    holed.executionSafety.executionMarginM = 0;
    const auto bcd = AutoCoveragePlanner{}.plan(holed);
    QCOMPARE(bcd.outcome.tier, SafetySolutionTier::D2);
    QVERIFY(bcd.plannerSource->escalated);
    QVERIFY(hasIssue(bcd, PlanningIssueCode::HardSafetyUnavailable));
    const auto raw = buildCoverageGeometry(holed.region).geometry.rawNavigationFreeSpace;
    const auto& route = bcd.outcome.diagnosticCandidate->path;
    for (std::size_t i = 1; i < route.size(); ++i) {
        QVERIFY(Geometry::segmentInsidePolygonRegionForValidatedGeometry(raw, route[i - 1], route[i]));
    }
    QVERIFY(bcd.repairCandidates.empty());
    QVERIFY(!bcd.outcome.repair.attempted);
    QCOMPARE(holed.safety.hardSafetyMarginM, 20.0);
    verifyRoundTrip(holed, bcd);
}

void IntegratedPlanningResultTest::_testT07()
{
    const auto p = unsupportedProblem();
    const auto result = SimpleMonotoneCoveragePlanner{}.plan(p);
    QCOMPARE(result.status, PlanningStatus::Failed);
    QCOMPARE(result.outcome.tier, SafetySolutionTier::D3);
    QCOMPARE(result.outcome.readiness, MissionReadiness::DiagnosticOnly);
    QVERIFY(result.path.empty());
    QVERIFY(!result.outcome.diagnosticCandidate);
    QVERIFY(!result.outcome.diagnosticOverlays.empty());
    QVERIFY(hasIssue(result, PlanningIssueCode::NavigationRegionUnsupported));
    verifyRejected(artifactFor(p, result), taskFor(p));
}

void IntegratedPlanningResultTest::_testT08()
{
    for (int defect = 0; defect < 4; ++defect) {
        auto p = problem();
        if (defect == 0) {
            p.swathWidthM = std::numeric_limits<double>::quiet_NaN();
        }
        if (defect == 1) {
            p.safety.hardSafetyMarginM = -1;
        }
        if (defect == 2) {
            p.region.coverageBoundary.vertices.clear();
        }
        if (defect == 3) {
            p.region.noGoRegions = {rectangle(-1, -1, 2, 2)};
        }
        const auto result = AutoCoveragePlanner{}.plan(p);
        QCOMPARE(result.status, PlanningStatus::InvalidInput);
        QCOMPARE(result.outcome.readiness, MissionReadiness::None);
        QVERIFY(!result.outcome.tier);
        QVERIFY(result.path.empty());
        QVERIFY(!result.outcome.diagnosticCandidate);
        QVERIFY(result.repairCandidates.empty());
    }
    auto invalid = problem();
    invalid.region.noGoRegions = {rectangle(-1, -1, 2, 2)};
    const auto task = taskFor(invalid);  // Valid v3 schema, rejected planning topology.
    MarinePlanContext context(planController());
    context.addTask(task);
    auto planner = std::make_shared<ForbiddenPlanner>();
    QVERIFY(context.plannerRegistry().registerPlanner(planner));
    CoverageInspectionComplexItem item(planController(), false, &context);
    item.setTaskId(QString::fromStdString(task.id));
    QVERIFY(!item.plan());
    QCOMPARE(planner->calls, 0);
    QCOMPARE(item.planningState(), CoverageInspectionComplexItem::Planned);
    QCOMPARE(item.planningResult().status, PlanningStatus::InvalidInput);
    QJsonArray saved;
    item.save(saved);
    QCOMPARE(saved.size(), 1);
    CoverageInspectionComplexItem restored(planController(), false, &context);
    QString error;
    QVERIFY2(restored.load(saved.first().toObject(), 0, error), qPrintable(error));
    QCOMPARE(restored.planningState(), CoverageInspectionComplexItem::Planned);
    QVERIFY(!restored.planningArtifact()->stale);
    QCOMPARE(restored.planningResult().outcome.readiness, MissionReadiness::None);
    verifyRejected(*restored.planningArtifact(), task);
    QCOMPARE(planner->calls, 0);
}

void IntegratedPlanningResultTest::_testT09()
{
    for (const auto& p : {problem(), problem(0, 1), problem(2, 2), problem(0, 0, 20), unsupportedProblem()}) {
        const auto first = SimpleMonotoneCoveragePlanner{}.plan(p);
        const auto second = SimpleMonotoneCoveragePlanner{}.plan(p);
        QCOMPARE(first.outcome.issues, second.outcome.issues);
        QVERIFY(std::ranges::is_sorted(first.outcome.issues, {}, &PlanningIssue::code));
        verifyRoundTrip(p, first);  // Includes typed reference bounds validation.
    }
    std::set<PlanningIssueCode> observed;
    auto failedAssessment = problem();
    failedAssessment.swathWidthM = 1e150;
    auto reserve = problem(0, 0, 30);
    reserve.region.navigationBoundary = rectangle(-5, -5, 25, 25);
    auto hard = unsupportedProblem();
    hard.safety = {20, 20};
    hard.executionSafety.executionMarginM = 0;
    for (const auto& fixture : {problem(), problem(0, 1), problem(2.4, 2.4), reserve, hard, failedAssessment}) {
        const auto result = AutoCoveragePlanner{}.plan(fixture);
        for (const auto& issue : result.outcome.issues) {
            observed.insert(issue.code);
        }
    }
    const auto unsupported = SimpleMonotoneCoveragePlanner{}.plan(unsupportedProblem());
    for (const auto& issue : unsupported.outcome.issues) {
        observed.insert(issue.code);
    }
    // Publication unit cases supply the actual failing primitive's structured geometry/reason.
    // They certify no route and must never invent a connector between the supplied components.
    for (int components = 1; components <= 2; ++components) {
        auto failure = unsupported;
        failure.outcome = {};
        failure.error = CoveragePlanningError::SafeTransitNotFound;
        failure.message = "Supported raw navigation connection remains unresolved";
        PolygonRegionSet2D raw{{.outerBoundary = rectangle(0, 0, 5, 5)}};
        if (components == 2) {
            raw.push_back({.outerBoundary = rectangle(10, 0, 15, 5)});
        }
        publishUnresolvedOutcome(failure, raw, raw, false);
        QVERIFY(failure.path.empty());
        QVERIFY(!failure.outcome.diagnosticCandidate);
        for (const auto& issue : failure.outcome.issues) {
            observed.insert(issue.code);
        }
        QString error;
        const auto geo = CoverageTaskAdapter::toPlanningResult(failure, *GeoReference::create(GeoPoint{38, 121, 0}));
        QVERIFY2(PlanningArtifactCodec::validateResult(geo, nullptr, error), qPrintable(error));
    }
    for (int code = 0; code <= static_cast<int>(PlanningIssueCode::CoverageAssessmentFailed); ++code) {
        QVERIFY2(observed.contains(static_cast<PlanningIssueCode>(code)), qPrintable(QString::number(code)));
    }
}

void IntegratedPlanningResultTest::_testT10()
{
    const auto p = problem(2.4, 2.4);
    const auto first = AutoCoveragePlanner{}.plan(p);
    const auto second = AutoCoveragePlanner{}.plan(p);
    QCOMPARE(first.outcome.suggestions, second.outcome.suggestions);
    QVERIFY(std::ranges::is_sorted(first.outcome.suggestions, {}, &PlanningSuggestion::code));
    QVERIFY(std::ranges::find(first.outcome.suggestions, PlanningSuggestionCode::ReviewSwathWidth,
                              &PlanningSuggestion::code) != first.outcome.suggestions.end());
    QVERIFY(std::ranges::find(first.outcome.suggestions, PlanningSuggestionCode::InspectRepairCost,
                              &PlanningSuggestion::code) != first.outcome.suggestions.end());
    const auto preferred = AutoCoveragePlanner{}.plan(problem(0, 1));
    QVERIFY(std::ranges::find(preferred.outcome.suggestions, PlanningSuggestionCode::ExpandNavigationArea,
                              &PlanningSuggestion::code) != preferred.outcome.suggestions.end());
    const auto hard = AutoCoveragePlanner{}.plan(problem(0, 0, 20));
    QVERIFY(std::ranges::find(hard.outcome.suggestions, PlanningSuggestionCode::ReviewHardNavigationFeasibility,
                              &PlanningSuggestion::code) != hard.outcome.suggestions.end());
    QCOMPARE(p.safety.hardSafetyMarginM, 2.4);
    QCOMPARE(p.safety.preferredSafetyMarginM, 2.4);
}

void IntegratedPlanningResultTest::_testT11()
{
    const auto p = problem(1, 2);
    const auto result = AutoCoveragePlanner{}.plan(p);
    QCOMPARE(result.outcome.readiness, MissionReadiness::ReadyWithWarning);
    QVERIFY(result.outcome.repair.attempted);
    QVERIFY(result.outcome.repair.applied);
    QCOMPARE(result.outcome.repair.reason, CoverageRepairReason::AppliedPolicyPass);
    const auto& runtime = result.repairCandidates[*result.outcome.selectedCandidateIndex];
    QCOMPARE(result.outcome.repair.components.size(), runtime.steps.size());
    for (std::size_t i = 0; i < runtime.steps.size(); ++i) {
        const auto& published = result.outcome.repair.components[i];
        const auto& actual = runtime.steps[i];
        QCOMPARE(published.componentId, actual.componentId);
        QCOMPARE(published.entryIndex, actual.entryIndex);
        QCOMPARE(published.reverse, actual.reverse);
        QCOMPARE(published.transitionCostM, actual.transitionCostM);
        QCOMPARE(published.after.uncoveredAreaM2, actual.after.quality.uncoveredAreaM2);
        QCOMPARE(published.pathLengthAfterM, actual.after.metrics.pathLengthM);
        QCOMPARE(published.turnCountAfter, actual.after.metrics.turnCount);
        QVERIFY(published.pathLengthAfterM > published.pathLengthBeforeM);
        QVERIFY(!published.before.passesRequirement);
    }
    QVERIFY(result.outcome.repair.components.back().after.passesRequirement);
    verifyRoundTrip(p, result);
}

void IntegratedPlanningResultTest::_testT12()
{
    auto p = problem();
    p.coverageRequirement = CoverageRequirement::Standard;
    const auto result = AutoCoveragePlanner{}.plan(p);
    QVERIFY(!result.outcome.repair.attempted);
    QVERIFY(!result.outcome.repair.applied);
    QCOMPARE(result.outcome.repair.reason, CoverageRepairReason::InitialPolicyPass);
    QVERIFY(!hasIssue(result, PlanningIssueCode::CoverageRepairApplied));
    QCOMPARE(result.path.size(), std::size_t{8});
    const auto& initial = result.repairCandidates[*result.outcome.selectedCandidateIndex].candidate;
    QCOMPARE(result.path.size(), initial.path.size());
    for (std::size_t i = 0; i < result.path.size(); ++i) {
        QCOMPARE(result.path[i].xM, initial.path[i].xM);
        QCOMPARE(result.path[i].yM, initial.path[i].yM);
    }
}

void IntegratedPlanningResultTest::_testT13()
{
    const std::vector<std::shared_ptr<ICoveragePlanner>> productionPlanners{
        std::make_shared<SimpleMonotoneCoveragePlanner>(), std::make_shared<BoustrophedonCoveragePlanner>(),
        std::make_shared<AutoCoveragePlanner>()};
    for (const auto& production : productionPlanners) {
        for (const auto& p : {problem(), problem(0, 1), problem(2, 2), problem(0, 0, 20), unsupportedProblem()}) {
            const auto result = production->plan(p);
            verifyRoundTrip(p, result);
            const auto task = taskFor(p, result.plannerSource->requestedPlannerId);
            const auto json = complexObject(serialized(artifactFor(p, result), task), task);
            MarinePlanContext context(planController());
            context.addTask(task);
            auto planner = std::make_shared<ForbiddenPlanner>(production->id(), production->semanticVersion());
            QVERIFY(context.plannerRegistry().registerPlanner(planner));
            QCOMPARE(context.plannerRegistry().planner(task.planner.plannerId).get(), planner.get());
            CoverageInspectionComplexItem item(planController(), false, &context);
            QString error;
            QVERIFY2(item.load(json, 0, error), qPrintable(error));
            QCOMPARE(planner->calls, 0);
            QCOMPARE(item.planningState(), CoverageInspectionComplexItem::Planned);
            QCOMPARE(item.planningResult().outcome.readiness, result.outcome.readiness);
            QJsonArray saved;
            item.save(saved);
            QCOMPARE(saved.first().toObject(), json);
        }
    }
}

void IntegratedPlanningResultTest::_testT14()
{
    const auto p = problem();
    auto task = taskFor(p);
    const auto artifact = artifactFor(p, AutoCoveragePlanner{}.plan(p));
    auto json = complexObject(serialized(artifact, task), task);
    for (int mutation = 0; mutation < 6; ++mutation) {
        auto changed = task;
        auto edited = json;
        if (mutation == 0) {
            changed.safety.preferredSafetyMarginM = 1;
        }
        if (mutation == 1) {
            changed.coverage.swathWidthM = 6;
        }
        if (mutation == 2) {
            for (auto& point : changed.region.coverageBoundary.vertices) {
                point.latitudeDeg += 10;
                point.longitudeDeg += 30;
            }
            changed.region.navigationBoundary = changed.region.coverageBoundary;
        }
        if (mutation == 3) {
            auto identity = edited.value("inputIdentity").toObject();
            identity.insert("planningVersion", "p2.v0.5.planning.1");
            edited.insert("inputIdentity", identity);
        }
        if (mutation == 4) {
            auto identity = edited.value("inputIdentity").toObject();
            identity.insert("strategyVersion", "simple-monotone.future");
            edited.insert("inputIdentity", identity);
            auto source = edited.value("plannerSource").toObject();
            source.insert("strategySemanticVersion", "simple-monotone.future");
            edited.insert("plannerSource", source);
            auto outcome = edited.value("outcome").toObject();
            auto quality = outcome.value("coverageQuality").toObject();
            quality.insert("strategyVersion", "simple-monotone.future");
            outcome.insert("coverageQuality", quality);
            edited.insert("outcome", outcome);
        }
        if (mutation == 5) {
            auto identity = edited.value("inputIdentity").toObject();
            identity.insert("policyVersion", "coverage-quality.unsupported");
            edited.insert("inputIdentity", identity);
            auto outcome = edited.value("outcome").toObject();
            auto quality = outcome.value("coverageQuality").toObject();
            quality.insert("policySemanticVersion", "coverage-quality.unsupported");
            outcome.insert("coverageQuality", quality);
            edited.insert("outcome", outcome);
        }
        MarinePlanContext context(planController());
        context.addTask(changed);
        auto planner = std::make_shared<ForbiddenPlanner>();
        QVERIFY(context.plannerRegistry().registerPlanner(planner));
        CoverageInspectionComplexItem item(planController(), false, &context);
        QString error;
        QVERIFY2(item.load(edited, 0, error), qPrintable(error));
        QVERIFY(item.planningArtifact()->stale);
        QCOMPARE(item.planningState(), CoverageInspectionComplexItem::Unplanned);
        QCOMPARE(item.planningResult().outcome.readiness, MissionReadiness::None);
        QVERIFY(item.planningResult().path.empty());
        QCOMPARE(planner->calls, 0);
        QJsonArray saved;
        item.save(saved);
        QCOMPARE(saved.first().toObject(), edited);
        verifyRejected(*item.planningArtifact(), changed);
    }
}

void IntegratedPlanningResultTest::_testT15()
{
    const auto p = problem();
    const auto task = taskFor(p);
    auto json = complexObject(serialized(artifactFor(p, AutoCoveragePlanner{}.plan(p)), task), task);
    json.insert("resultContract", "InfrastructureOnly");
    json.insert("planningStatus", "success");
    QJsonArray roles;
    for (const auto& value : json.value("legRoles").toArray()) {
        roles.append(value.toString().toLower());
    }
    json.insert("legRoles", roles);
    json.remove("outcome");
    json.remove("plannerSource");
    json.remove("planningError");
    MarinePlanContext context(planController());
    context.addTask(task);
    CoverageInspectionComplexItem item(planController(), false, &context);
    QString error;
    QVERIFY2(item.load(json, 0, error), qPrintable(error));
    QCOMPARE(item.planningState(), CoverageInspectionComplexItem::Unplanned);
    QCOMPARE(item.planningResult().outcome.readiness, MissionReadiness::None);
    QVERIFY(item.planningResult().path.empty());
    QJsonArray saved;
    item.save(saved);
    QCOMPARE(saved.first().toObject(), json);
    verifyRejected(*item.planningArtifact(), task);
}

void IntegratedPlanningResultTest::_testT16()
{
    const auto p = problem();
    const auto task = taskFor(p);
    const auto original = serialized(artifactFor(p, AutoCoveragePlanner{}.plan(p)), task);
    for (int defect = 0; defect < 11; ++defect) {
        auto json = original;
        auto out = json.value("outcome").toObject();
        if (defect == 0) {
            json.insert("planningStatus", "Failed");
        }
        if (defect == 1) {
            out.insert("tier", "D2");
        }
        if (defect == 2) {
            auto quality = out.value("coverageQuality").toObject();
            quality.insert("status", "AssessmentError");
            out.insert("coverageQuality", quality);
        }
        if (defect == 3) {
            out.insert("readiness", "DiagnosticOnly");
        }
        if (defect == 4) {
            json = serialized(artifactFor(problem(0, 0, 20), AutoCoveragePlanner{}.plan(problem(0, 0, 20))),
                              taskFor(problem(0, 0, 20)));
            out = json.value("outcome").toObject();
            out.insert("diagnosticCandidate", QJsonValue(QJsonValue::Null));
        }
        if (defect == 5) {
            out.insert("canonicalLegAssessments",
                       QJsonArray{"HardUnsafe", "PreferredSafe", "PreferredSafe", "PreferredSafe", "PreferredSafe",
                                  "PreferredSafe", "PreferredSafe"});
        }
        if (defect == 6) {
            json.insert("legRoles", QJsonArray{});
        }
        if (defect == 7) {
            out.insert("canonicalLegAssessments", QJsonArray{});
        }
        if (defect == 8) {
            auto issues = out.value("issues").toArray();
            auto issue = issues.first().toObject();
            issue.insert("reference", QJsonObject{{"kind", "CanonicalPathLegRange"},
                                                  {"index", 0},
                                                  {"firstLeg", 999},
                                                  {"legCount", 1},
                                                  {"residual", "Uncovered"}});
            issues[0] = issue;
            out.insert("issues", issues);
        }
        if (defect == 9) {
            auto repair = out.value("repair").toObject();
            repair.insert("attempted", true);
            repair.insert("applied", true);
            out.insert("repair", repair);
        }
        if (defect == 10) {
            auto quality = out.value("coverageQuality").toObject();
            quality.insert("coveredAreaM2", QJsonObject{{"available", false}, {"value", 0}});
            out.insert("coverageQuality", quality);
        }
        json.insert("outcome", out);
        PlanningArtifact parsed;
        QString error;
        QVERIFY2(!PlanningArtifactCodec::load(json, task, parsed, error), qPrintable(QString::number(defect)));
    }
}

void IntegratedPlanningResultTest::_testT17()
{
    const auto p = problem();
    const auto task = taskFor(p);
    const auto ready = artifactFor(p, AutoCoveragePlanner{}.plan(p));
    verifyAllowed(ready, task);
    verifyAllowed(artifactFor(problem(0, 1), AutoCoveragePlanner{}.plan(problem(0, 1))), taskFor(problem(0, 1)));
    verifyRejected(artifactFor(problem(2, 2), AutoCoveragePlanner{}.plan(problem(2, 2))), taskFor(problem(2, 2)));
    verifyRejected(artifactFor(problem(0, 0, 20), AutoCoveragePlanner{}.plan(problem(0, 0, 20))),
                   taskFor(problem(0, 0, 20)));
    for (int defect = 0; defect < 8; ++defect) {
        auto artifact = ready;
        if (defect == 0) {
            artifact.stale = true;
        }
        if (defect == 1) {
            artifact.resultContract = PlanningResultContract::InfrastructureOnly;
        }
        if (defect == 2) {
            artifact.result.outcome.readiness = MissionReadiness::None;
        }
        if (defect == 3) {
            artifact.result.status = PlanningStatus::InvalidInput;
        }
        if (defect == 4) {
            artifact.result.path[1] = artifact.result.path[0];
        }
        if (defect == 5) {
            artifact.result.outcome.canonicalLegAssessments[0] = SafetyLegClass::HardUnsafe;
        }
        if (defect == 6) {
            artifact.identity.semantics.planningVersion = "p2.v0.5.planning.1";
        }
        if (defect == 7) {
            artifact.result.outcome.coverageQuality->status = CoverageQualityStatus::AssessmentError;
        }
        verifyRejected(artifact, task);
    }
    QObject parent;
    QList<MissionItem*> items;
    int sequence = 3;
    QString error;
    QVERIFY(!ArduPilotMissionAdapter::appendWaypoints(ready.result, items, &parent, sequence, error));
    QVERIFY(items.empty());
    QCOMPARE(sequence, 3);
}

void IntegratedPlanningResultTest::_testT18()
{
    const auto p = problem(0, 0, 20);
    const auto artifact = artifactFor(p, AutoCoveragePlanner{}.plan(p));
    QVERIFY(artifact.result.path.empty());
    QVERIFY(artifact.result.outcome.diagnosticCandidate);
    QVERIFY(!artifact.result.outcome.diagnosticCandidate->path.empty());
    verifyRejected(artifact, taskFor(p));
}

void IntegratedPlanningResultTest::_testT19()
{
    for (int holes = 0; holes < 2; ++holes) {
        auto p = problem();
        if (holes) {
            p.region.navigationBoundary = rectangle(-5, -5, 25, 25);
            p.region.noGoRegions = {rectangle(8, 8, 12, 12)};
        }
        const auto result = AutoCoveragePlanner{}.plan(p);
        const auto& source = *result.plannerSource;
        QCOMPARE(source.requestedPlannerId, std::string(CoverageStrategySemantics::AutoPlannerId));
        QCOMPARE(source.resolvedStrategy.strategyId, std::string(holes ? CoverageStrategySemantics::BoustrophedonId
                                                                       : CoverageStrategySemantics::SimpleMonotoneId));
        QCOMPARE(source.escalated, bool(holes));
        QCOMPARE(hasIssue(result, PlanningIssueCode::PlannerEscalated), bool(holes));
        QCOMPARE(source.selectedSweepAngleDeg, result.selectedSweepAngleDeg);
        verifyRoundTrip(p, result);
    }
}

void IntegratedPlanningResultTest::_testT20()
{
    for (auto p : {problem(), problem(2, 2), problem(0, 0, 20), unsupportedProblem()}) {
        const auto first = SimpleMonotoneCoveragePlanner{}.plan(p);
        const auto task = taskFor(p, first.plannerSource->requestedPlannerId);
        const auto original = serialized(artifactFor(p, first), task);
        QVERIFY(!original.isEmpty());
        for (int iteration = 0; iteration < 3; ++iteration) {
            for (auto* ring : {&p.region.coverageBoundary, &p.region.navigationBoundary}) {
                std::rotate(ring->vertices.begin(), ring->vertices.begin() + 1, ring->vertices.end());
                std::ranges::reverse(ring->vertices);
            }
            for (auto& ring : p.region.noGoRegions) {
                std::ranges::reverse(ring.vertices);
            }
            const auto result = SimpleMonotoneCoveragePlanner{}.plan(p);
            QCOMPARE(serialized(artifactFor(p, result), taskFor(p, result.plannerSource->requestedPlannerId)),
                     original);
        }
    }
}

void IntegratedPlanningResultTest::_testM00CanonicalReady()
{
    verifyScenarioIdentity(V05ExecutionFixtures::M00Canonical, V05ExecutionFixtures::M00Sha256);
    const auto p = V05ExecutionFixtures::problem(false);
    const auto result = AutoCoveragePlanner{}.plan(p);
    QCOMPARE(result.status, PlanningStatus::Success);
    QCOMPARE(result.outcome.readiness, MissionReadiness::Ready);
    QCOMPARE(result.outcome.tier, SafetySolutionTier::D0);
    QVERIFY(result.plannerSource);
    QCOMPARE(result.plannerSource->resolvedStrategy.strategyId,
             std::string(CoverageStrategySemantics::SimpleMonotoneId));
    QVERIFY(!result.plannerSource->escalated);
    QVERIFY(!result.outcome.repair.attempted);
    QVERIFY(!result.outcome.repair.applied);
    QCOMPARE(result.outcome.repair.reason, CoverageRepairReason::InitialPolicyPass);
    QVERIFY(result.outcome.coverageQuality && result.outcome.coverageQuality->passesRequirement);
    QVERIFY(hasIssue(result, PlanningIssueCode::IngressNotAssessed));
    const auto task = taskFor(p);
    const auto artifact = artifactFor(p, result);
    verifyAllowed(artifact, task);
    verifyRoundTrip(p, result);
    verifySITLBinding(QStringLiteral("M00"), V05ExecutionFixtures::M00Canonical, V05ExecutionFixtures::M00Sha256, p);
}

void IntegratedPlanningResultTest::_testM01ConcaveMonotone()
{
    constexpr auto canonical =
        "M01|C=L(0,0;20,0;20,10;10,10;10,20;0,20)|N=rect(-10,-10,30,30)|O=[]|swath=4|H=0|P=0|E=0|req=Strict|sweep="
        "Manual90|planner=Auto";
    verifyScenarioIdentity(canonical, "3f0f9aceb82cf3dd9f0ebc488cf100c96e3d862610d4a050564d86b98a59fb06");

    auto p = problem();
    p.region.coverageBoundary = {.vertices = {{0, 0}, {20, 0}, {20, 10}, {10, 10}, {10, 20}, {0, 20}}};
    p.region.navigationBoundary = rectangle(-10, -10, 30, 30);
    p.swathWidthM = 4;
    const auto result = AutoCoveragePlanner{}.plan(p);
    QCOMPARE(result.status, PlanningStatus::Success);
    QVERIFY(result.plannerSource);
    QCOMPARE(result.plannerSource->resolvedStrategy.strategyId,
             std::string(CoverageStrategySemantics::SimpleMonotoneId));
    QVERIFY(!result.plannerSource->escalated);
    QVERIFY(!result.path.empty());
    verifyRoundTrip(p, result);
}

void IntegratedPlanningResultTest::_testM02TopologyEscalation()
{
    constexpr auto canonical =
        "M02|C=U(0,0;10,0;10,10;7,10;7,3;3,3;3,10;0,10)|N=rect(-5,-5,15,15)|O=[]|swath=5|H=0|P=0|E=0|req=Strict|sweep="
        "Manual90|planner=Auto";
    verifyScenarioIdentity(canonical, "7372f8486e1e3ec12447ba05ce553666a9487c540b8b7d406546c47892939d44");

    auto p = problem();
    p.region.coverageBoundary = {.vertices = {{0, 0}, {10, 0}, {10, 10}, {7, 10}, {7, 3}, {3, 3}, {3, 10}, {0, 10}}};
    p.region.navigationBoundary = rectangle(-5, -5, 15, 15);
    const auto result = AutoCoveragePlanner{}.plan(p);
    QVERIFY(result.plannerSource);
    QCOMPARE(result.plannerSource->resolvedStrategy.strategyId,
             std::string(CoverageStrategySemantics::BoustrophedonId));
    QVERIFY(result.plannerSource->escalated);
    QCOMPARE(result.plannerSource->resolutionReason, PlannerResolutionReason::TargetNonMonotoneForSelectedSweep);
    verifyRoundTrip(p, result);
}

void IntegratedPlanningResultTest::_testM03NavigationOutsideCoverage()
{
    constexpr auto canonical =
        "M03|C=rect(0,0,1,30)|N=rect(-10,-5,11,35)|O=[rect(-5,9,-0.5,11)]|swath=10|H=0|P=0|E=2|req=Strict|sweep="
        "Manual90|planner=SimpleMonotone";
    verifyScenarioIdentity(canonical, "289f4330ce183cc6811f7450cf5e59df0ba939d63ea168724d8ccc2bdda2139d");

    auto p = problem();
    p.region.coverageBoundary = rectangle(0, 0, 1, 30);
    p.region.navigationBoundary = rectangle(-10, -5, 11, 35);
    p.region.noGoRegions = {rectangle(-5, 9, -0.5, 11)};
    p.swathWidthM = 10;
    p.executionSafety.executionMarginM = 2;
    const auto result = SimpleMonotoneCoveragePlanner{}.plan(p);
    QCOMPARE(result.status, PlanningStatus::Success);
    const auto geometry = buildCoverageGeometry(p.region);
    QCOMPARE(geometry.error, CoveragePlanningError::None);
    bool transitOutsideTarget = false;
    for (std::size_t leg = 0; leg < result.legRoles.size(); ++leg) {
        const bool insideTarget = Geometry::segmentInsidePolygonRegion(geometry.geometry.coverageTarget,
                                                                       result.path[leg], result.path[leg + 1]);
        if (result.legRoles[leg] == PathLegRole::Coverage) {
            QVERIFY(insideTarget);
        } else if (!insideTarget) {
            transitOutsideTarget = true;
        }
    }
    QVERIFY(transitOutsideTarget);
    verifyRoundTrip(p, result);
}

void IntegratedPlanningResultTest::_testM04HardSafePreferredWarning()
{
    verifyScenarioIdentity(V05ExecutionFixtures::M04Canonical, V05ExecutionFixtures::M04Sha256);
    const auto p = V05ExecutionFixtures::problem(true);
    const auto result = AutoCoveragePlanner{}.plan(p);
    QCOMPARE(result.status, PlanningStatus::Success);
    QCOMPARE(result.outcome.readiness, MissionReadiness::ReadyWithWarning);
    QCOMPARE(result.outcome.tier, SafetySolutionTier::D1);
    QVERIFY(result.outcome.coverageQuality);
    QVERIFY(result.outcome.coverageQuality->passesRequirement);
    QVERIFY(hasIssue(result, PlanningIssueCode::PreferredSafetyViolated));
    verifyAllowed(artifactFor(p, result), taskFor(p));
    verifyRoundTrip(p, result);
    verifySITLBinding(QStringLiteral("M04"), V05ExecutionFixtures::M04Canonical, V05ExecutionFixtures::M04Sha256, p);
}

void IntegratedPlanningResultTest::_testM05RawDiagnosticOnly()
{
    constexpr auto canonical =
        "M05|C=rect(0,0,20,20)|N=rect(-5,-5,25,25)|O=[]|swath=5|H=0|P=0|E=30|req=Strict|sweep=Manual90|planner=Auto";
    verifyScenarioIdentity(canonical, "98fda72f67f30797e74244c2f5ccebc4ada0b9845a3ed89897a7f58931a9a6ae");

    auto p = problem(0, 0, 30);
    p.region.navigationBoundary = rectangle(-5, -5, 25, 25);
    const auto result = AutoCoveragePlanner{}.plan(p);
    QCOMPARE(result.status, PlanningStatus::Failed);
    QCOMPARE(result.outcome.readiness, MissionReadiness::DiagnosticOnly);
    QCOMPARE(result.outcome.tier, SafetySolutionTier::D2);
    QVERIFY(result.path.empty());
    QVERIFY(result.outcome.diagnosticCandidate);
    const auto raw = buildCoverageGeometry(p.region).geometry.rawNavigationFreeSpace;
    const auto& diagnostic = *result.outcome.diagnosticCandidate;
    for (std::size_t leg = 1; leg < diagnostic.path.size(); ++leg) {
        QVERIFY(Geometry::segmentInsidePolygonRegionForValidatedGeometry(raw, diagnostic.path[leg - 1],
                                                                         diagnostic.path[leg]));
    }
    verifyRejected(artifactFor(p, result), taskFor(p));
    verifyRoundTrip(p, result);
}

void IntegratedPlanningResultTest::_testM06StandardStrictSameGeometry()
{
    constexpr auto canonical =
        "M06|T=rect(0,0,100,100)|path=lanes(y=1..97step2,98.6)|swath=2|compare=Standard_vs_Strict|strategy=simple-"
        "monotone.v1";
    verifyScenarioIdentity(canonical, "24a97a4708f316f457f1acfde827f26541f2643412bac2f16a5f45ab4c55986e");

    std::vector<double> lanes;
    for (double y = 1; y <= 97; y += 2) {
        lanes.push_back(y);
    }
    lanes.push_back(98.6);
    const auto [path, roles] = lanePath(100, lanes);
    const PolygonRegionSet2D target{{.outerBoundary = rectangle(0, 0, 100, 100)}};
    const PlannerStrategyIdentity strategy{.strategyId = CoverageStrategySemantics::SimpleMonotoneId,
                                           .semanticVersion = CoverageStrategySemantics::SimpleMonotoneVersion};
    const auto standard = evaluateCoverageQuality(target, path, roles, 2, CoverageRequirement::Standard, strategy);
    const auto strict = evaluateCoverageQuality(target, path, roles, 2, CoverageRequirement::Strict, strategy);
    QCOMPARE(standard.targetAreaM2, strict.targetAreaM2);
    QCOMPARE(standard.coveredAreaM2, strict.coveredAreaM2);
    QCOMPARE(standard.uncoveredAreaM2, strict.uncoveredAreaM2);
    QCOMPARE(standard.coverageRatio, strict.coverageRatio);
    QCOMPARE(standard.criticalUncoveredAreaM2, strict.criticalUncoveredAreaM2);
    QCOMPARE(standard.status, CoverageQualityStatus::Acceptable);
    QVERIFY(standard.passesRequirement);
    QCOMPARE(strict.status, CoverageQualityStatus::Insufficient);
    QVERIFY(!strict.passesRequirement);
    QCOMPARE(standard.policySemanticVersion, std::string(CoverageQualityPolicySemanticVersion));
    QCOMPARE(strict.policySemanticVersion, std::string(CoverageQualityPolicySemanticVersion));
}

void IntegratedPlanningResultTest::_testM07RepairSuccess()
{
    constexpr auto canonical =
        "M07|C=rect(0,0,20,20)|N=C|O=[]|swath=5|H=1|P=2|E=0|req=Strict|sweep=Manual90|planner=Auto";
    verifyScenarioIdentity(canonical, "47de0de79db47251ac34af0a0bd41d48f26ea701603bb380bd10884e83dada9d");

    const auto p = problem(1, 2);
    const auto result = AutoCoveragePlanner{}.plan(p);
    QCOMPARE(result.status, PlanningStatus::Success);
    QCOMPARE(result.outcome.readiness, MissionReadiness::ReadyWithWarning);
    QVERIFY(result.outcome.repair.attempted);
    QVERIFY(result.outcome.repair.applied);
    QCOMPARE(result.outcome.repair.reason, CoverageRepairReason::AppliedPolicyPass);
    QVERIFY(!result.outcome.repair.components.empty());
    QVERIFY(result.outcome.repair.components.back().after.passesRequirement);
    verifyAllowed(artifactFor(p, result), taskFor(p));
    verifyRoundTrip(p, result);
}

void IntegratedPlanningResultTest::_testM08RepairExhausted()
{
    constexpr auto canonical =
        "M08|C=rect(0,0,20,20)|N=C|O=[]|swath=5|H=2|P=2|E=0|req=Strict|sweep=Manual90|planner=Auto";
    verifyScenarioIdentity(canonical, "d59601e14d74fb4f1559f215229bb0e0d045e28dd7ecf7ddcc14efbaeb96b8b3");

    const auto p = problem(2, 2);
    const auto result = AutoCoveragePlanner{}.plan(p);
    QCOMPARE(result.status, PlanningStatus::Success);
    QCOMPARE(result.outcome.readiness, MissionReadiness::ReviewRequired);
    QVERIFY(!result.path.empty());
    QVERIFY(result.outcome.coverageQuality);
    QCOMPARE(result.outcome.coverageQuality->status, CoverageQualityStatus::Insufficient);
    QVERIFY(result.outcome.repair.attempted);
    QCOMPARE(result.outcome.repair.applied, !result.outcome.repair.components.empty());
    QVERIFY(hasIssue(result, PlanningIssueCode::CoverageBelowRequirement));
    if (result.outcome.repair.applied) {
        QVERIFY(hasIssue(result, PlanningIssueCode::CoverageRepairApplied));
        QVERIFY(hasIssue(result, PlanningIssueCode::CoverageRepairInsufficient));
        QVERIFY(!result.outcome.repair.components.back().after.passesRequirement);
    }
    QVERIFY(result.plannerSource);
    QCOMPARE(result.plannerSource->resolvedStrategy.strategyId,
             std::string(CoverageStrategySemantics::SimpleMonotoneId));
    QVERIFY(!result.plannerSource->escalated);
    verifyRejected(artifactFor(p, result), taskFor(p));
    verifyRoundTrip(p, result);
}

void IntegratedPlanningResultTest::_testM09UnsupportedAndInvalid()
{
    constexpr auto canonical =
        "M09|validUnsupported=C=rect(0,0,20,20),N=rect(-5,-5,25,25),O=[rect(8,8,12,12)],E=100,planner=SimpleMonotone|"
        "invalid=O=rect(-1,-1,2,2),planner=Auto";
    verifyScenarioIdentity(canonical, "e116c1ed81aa5837d202254d757d5accf1786eb85efd31a1957f4c4c466bdbe7");

    const auto supportedInputFailure = unsupportedProblem();
    const auto unsupported = SimpleMonotoneCoveragePlanner{}.plan(supportedInputFailure);
    QCOMPARE(unsupported.status, PlanningStatus::Failed);
    QCOMPARE(unsupported.outcome.readiness, MissionReadiness::DiagnosticOnly);
    QCOMPARE(unsupported.outcome.tier, SafetySolutionTier::D3);
    QVERIFY(unsupported.path.empty());
    QVERIFY(!unsupported.outcome.diagnosticCandidate);
    QVERIFY(!unsupported.outcome.diagnosticOverlays.empty());
    verifyRejected(artifactFor(supportedInputFailure, unsupported), taskFor(supportedInputFailure));

    auto invalid = problem();
    invalid.region.noGoRegions = {rectangle(-1, -1, 2, 2)};
    const auto invalidResult = AutoCoveragePlanner{}.plan(invalid);
    QCOMPARE(invalidResult.status, PlanningStatus::InvalidInput);
    QCOMPARE(invalidResult.outcome.readiness, MissionReadiness::None);
    QVERIFY(!invalidResult.outcome.tier);
    QVERIFY(invalidResult.path.empty());
    QVERIFY(!invalidResult.outcome.diagnosticCandidate);
}

void IntegratedPlanningResultTest::_testCoverageRequirementBinding()
{
    const auto p = problem(2, 2);
    const auto task = taskFor(p);
    auto forged = artifactFor(p, AutoCoveragePlanner{}.plan(p));
    QCOMPARE(forged.result.outcome.coverageQuality->status, CoverageQualityStatus::Insufficient);
    const auto makeStandard = [](GeoCoverageQualityEvaluation& quality) {
        quality.requirement = CoverageRequirement::Standard;
        if (quality.status == CoverageQualityStatus::Insufficient &&
            quality.coverageRatio >= StandardCoveragePolicy::minimumCoverageRatio &&
            quality.criticalUncoveredAreaM2 <= quality.numericalToleranceM2) {
            quality.status = CoverageQualityStatus::Acceptable;
            quality.passesRequirement = true;
        }
    };
    auto& out = forged.result.outcome;
    makeStandard(*out.coverageQuality);
    for (auto& step : out.repair.components) {
        makeStandard(step.before);
        makeStandard(step.after);
    }
    QCOMPARE(out.coverageQuality->status, CoverageQualityStatus::Acceptable);
    out.readiness = MissionReadiness::Ready;
    out.repair.reason = CoverageRepairReason::AppliedPolicyPass;
    std::erase_if(out.issues, [](const auto& issue) {
        return issue.code == PlanningIssueCode::CoverageBelowRequirement ||
               issue.code == PlanningIssueCode::CoverageRepairInsufficient;
    });
    QString error;
    // The forgery is internally coherent. Only binding it to the Strict Task reveals the defect.
    QVERIFY2(PlanningArtifactCodec::validateResult(forged.result, nullptr, error), qPrintable(error));
    auto otherTask = task;
    otherTask.coverage.coverageRequirement = CoverageRequirement::Standard;
    const auto malformed = serialized(forged, otherTask);  // Stale data does not use a different Task's policy.
    QVERIFY(!malformed.isEmpty());
    QCOMPARE(malformed.value("inputIdentity").toObject(), forged.identity.toJson());
    QVERIFY(!PlanningArtifactCodec::validateResult(forged.result, &task, error));
    QJsonObject json;
    QVERIFY(!PlanningArtifactCodec::save(forged, task, json, error));
    PlanningArtifact loaded;
    QVERIFY(!PlanningArtifactCodec::load(malformed, task, loaded, error));
    verifyRejected(forged, task);
}

void IntegratedPlanningResultTest::_testRepairEvaluationConsistency()
{
    const auto p = problem(1, 2);
    const auto task = taskFor(p);
    const auto original = artifactFor(p, AutoCoveragePlanner{}.plan(p));
    QVERIFY(original.result.outcome.repair.applied);
    const auto json = serialized(original, task);
    QVERIFY(!json.isEmpty());
    const auto changeArea = [](GeoCoverageQualityEvaluation& quality) {
        quality.targetAreaM2 *= 2;
        quality.coveredAreaM2 = quality.targetAreaM2 - quality.uncoveredAreaM2;
        quality.coverageRatio = quality.coveredAreaM2 / quality.targetAreaM2;
    };
    for (int defect = 0; defect < 8; ++defect) {
        auto damaged = original;
        auto& last = damaged.result.outcome.repair.components.back();
        if (defect == 0) {
            changeArea(last.after);  // Individually valid, but target 800 disagrees with canonical target 400.
        }
        if (defect == 1) {
            changeArea(last.before);
        }
        if (defect == 2) {
            last.before.requirement = CoverageRequirement::Standard;
        }
        if (defect == 3) {
            last.before.strategy.semanticVersion = "other-strategy";
        }
        if (defect == 4) {
            last.before.policySemanticVersion = "other-policy";
        }
        if (defect == 5) {
            last.after.residual.criticalCoverageCore.clear();
        }
        if (defect == 6) {
            last.after.coveredAreaM2 -= 1e-6;
            last.after.uncoveredAreaM2 += 1e-6;
            last.after.coverageRatio = last.after.coveredAreaM2 / last.after.targetAreaM2;
        }
        if (defect == 7) {
            last.after.strictFallbackTriggered = !last.after.strictFallbackTriggered;
        }
        auto malformed = json;
        auto outcome = malformed.value("outcome").toObject();
        auto repair = outcome.value("repair").toObject();
        auto components = repair.value("components").toArray();
        auto component = components.last().toObject();
        const char* field = defect >= 1 && defect <= 4 ? "before" : "after";
        auto quality = component.value(field).toObject();
        const auto& modified = defect >= 1 && defect <= 4 ? last.before : last.after;
        if (defect <= 1 || defect == 6) {
            for (const auto& [name, value] : {std::pair{"targetAreaM2", modified.targetAreaM2},
                                              {"coveredAreaM2", modified.coveredAreaM2},
                                              {"uncoveredAreaM2", modified.uncoveredAreaM2},
                                              {"coverageRatio", modified.coverageRatio}}) {
                quality.insert(name, QJsonObject{{"available", true}, {"value", value}});
            }
        } else if (defect == 2) {
            quality.insert("requirement", "Standard");
        } else if (defect == 3) {
            quality.insert("strategyVersion", "other-strategy");
        } else if (defect == 4) {
            quality.insert("policySemanticVersion", "other-policy");
        } else if (defect == 5) {
            quality.insert("criticalCoverageCore", QJsonObject{{"available", true}, {"value", QJsonArray{}}});
        } else {
            quality.insert("strictFallbackTriggered", modified.strictFallbackTriggered);
        }
        component.insert(field, quality);
        components[components.size() - 1] = component;
        repair.insert("components", components);
        outcome.insert("repair", repair);
        malformed.insert("outcome", outcome);
        QString error;
        QVERIFY2(!PlanningArtifactCodec::validateResult(damaged.result, &task, error),
                 qPrintable(QString::number(defect)));
        QJsonObject rejected;
        QVERIFY(!PlanningArtifactCodec::save(damaged, task, rejected, error));
        PlanningArtifact loaded;
        QVERIFY(!PlanningArtifactCodec::load(malformed, task, loaded, error));
        verifyRejected(damaged, task);
    }

    // Exercise the actual multi-step repair primitive, then publish its selected facts.
    auto holed = problem();
    holed.swathWidthM = 2;
    holed.region.navigationBoundary = rectangle(-5, -5, 25, 25);
    holed.region.noGoRegions = {rectangle(8, 8, 12, 12)};
    auto result = BoustrophedonCoveragePlanner{}.plan(holed);
    const auto geometry = buildCoverageGeometry(holed.region);
    const auto safety = buildSafetyTrackRegions(holed.region, holed.safety, holed.executionSafety);
    CoverageRepairCandidate initial;
    initial.path = {{0, 5}, {20, 5}};
    initial.legRoles = {PathLegRole::Coverage};
    initial.metrics = *calculatePlanningPathMetrics(initial.path, initial.legRoles);
    initial.quality = evaluateCoverageQuality(geometry.geometry.coverageTarget, initial.path, initial.legRoles, 2,
                                              holed.coverageRequirement, result.plannerSource->resolvedStrategy);
    const auto runtime =
        repairCoverageCandidate(geometry.geometry.coverageTarget, safety.regions.hardExecutionTrackRegion, safety, 2,
                                holed.coverageRequirement, result.plannerSource->resolvedStrategy, initial);
    QCOMPARE(runtime.steps.size(), std::size_t{2});
    result.outcome = {};
    result.path = runtime.candidate.path;
    result.legRoles = runtime.candidate.legRoles;
    result.coverageLengthM = runtime.candidate.metrics.coverageLengthM;
    result.transitLengthM = runtime.candidate.metrics.transitLengthM;
    result.pathLengthM = runtime.candidate.metrics.pathLengthM;
    result.turnCount = runtime.candidate.metrics.turnCount;
    result.outcome.coverageQuality = runtime.candidate.quality;
    publishCanonicalOutcome(result, evaluateSafetyCandidate(safety, result.path), 0, runtime, initial.metrics, false);
    const auto chainTask = taskFor(holed, result.plannerSource->requestedPlannerId);
    const auto chain = artifactFor(holed, result);
    const auto chainJson = serialized(chain, chainTask);
    QVERIFY(!chainJson.isEmpty());
    auto broken = chain;
    auto& before = broken.result.outcome.repair.components[1].before;
    before.uncoveredAreaM2 += 0.1;
    before.coveredAreaM2 -= 0.1;
    before.coverageRatio = before.coveredAreaM2 / before.targetAreaM2;
    auto malformed = chainJson;
    auto outcome = malformed.value("outcome").toObject();
    auto repair = outcome.value("repair").toObject();
    auto components = repair.value("components").toArray();
    auto second = components[1].toObject();
    auto quality = second.value("before").toObject();
    quality.insert("coveredAreaM2", QJsonObject{{"available", true}, {"value", before.coveredAreaM2}});
    quality.insert("uncoveredAreaM2", QJsonObject{{"available", true}, {"value", before.uncoveredAreaM2}});
    quality.insert("coverageRatio", QJsonObject{{"available", true}, {"value", before.coverageRatio}});
    second.insert("before", quality);
    components[1] = second;
    repair.insert("components", components);
    outcome.insert("repair", repair);
    malformed.insert("outcome", outcome);
    QString error;
    QVERIFY(!PlanningArtifactCodec::validateResult(broken.result, &chainTask, error));
    QJsonObject rejected;
    QVERIFY(!PlanningArtifactCodec::save(broken, chainTask, rejected, error));
    PlanningArtifact loaded;
    QVERIFY(!PlanningArtifactCodec::load(malformed, chainTask, loaded, error));
    verifyRejected(broken, chainTask);
}

void IntegratedPlanningResultTest::_testFailedCurrentOutcome()
{
    const std::vector<std::shared_ptr<ICoveragePlanner>> planners{std::make_shared<SimpleMonotoneCoveragePlanner>(),
                                                                  std::make_shared<BoustrophedonCoveragePlanner>(),
                                                                  std::make_shared<AutoCoveragePlanner>()};
    for (const auto& production : planners) {
        for (int failure = 0; failure < 3; ++failure) {
            auto p = problem();
            p.swathWidthM = 1e-12;
            if (failure == 1) {
                p.sweepAngleMode = SweepAngleMode::Auto;
            }
            if (failure == 2) {
                p.swathWidthM = 5;
                p.safety = {1e150, 1e150};
            }
            auto normalized = p;
            QCOMPARE(CoverageProblemValidator::validateAndNormalize(normalized), CoveragePlanningError::None);
            const auto result = production->plan(p);
            QCOMPARE(result.status, PlanningStatus::Failed);
            QCOMPARE(result.outcome.readiness, MissionReadiness::DiagnosticOnly);
            QCOMPARE(result.outcome.tier, SafetySolutionTier::D3);
            QVERIFY(!result.outcome.diagnosticOverlays.empty());
            QVERIFY(result.path.empty());
            QVERIFY(!result.outcome.diagnosticCandidate);
            QVERIFY(result.repairCandidates.empty());
            const auto task = taskFor(p, production->id());
            QVERIFY(task.schemaValid());
            verifyRejected(artifactFor(p, result, production->id()), task);
            MarinePlanContext context(planController());
            context.addTask(task);
            QVERIFY(context.plannerRegistry().registerPlanner(production));
            CoverageInspectionComplexItem item(planController(), false, &context);
            item.setTaskId(QString::fromStdString(task.id));
            QVERIFY(!item.plan());
            QCOMPARE(item.planningState(), CoverageInspectionComplexItem::Planned);
            QCOMPARE(item.planningResult().outcome.tier, SafetySolutionTier::D3);
            QCOMPARE(item.planningArtifact()->resultContract, PlanningResultContract::IntegratedV05);
            QJsonArray saved;
            item.save(saved);
            QCOMPARE(saved.size(), 1);
            auto spy = std::make_shared<ForbiddenPlanner>(production->id(), production->semanticVersion());
            MarinePlanContext restoreContext(planController());
            restoreContext.addTask(task);
            QVERIFY(restoreContext.plannerRegistry().registerPlanner(spy));
            CoverageInspectionComplexItem restored(planController(), false, &restoreContext);
            QString error;
            QVERIFY2(restored.load(saved.first().toObject(), 0, error), qPrintable(error));
            QCOMPARE(restored.planningState(), CoverageInspectionComplexItem::Planned);
            QVERIFY(!restored.planningArtifact()->stale);
            QCOMPARE(restored.planningResult().outcome.tier, SafetySolutionTier::D3);
            QCOMPARE(spy->calls, 0);
            QJsonArray resaved;
            restored.save(resaved);
            QCOMPARE(resaved, saved);
            verifyRejected(*restored.planningArtifact(), task);
        }
    }
}

void IntegratedPlanningResultTest::_testPlannerSourceBinding_data()
{
    QTest::addColumn<int>("defect");
    QTest::newRow("manual-source-auto") << 0;
    QTest::newRow("manual-selected-angle") << 1;
    QTest::newRow("actual-zero-route-task-ninety") << 2;
    QTest::newRow("direct-simple-resolved-bcd") << 3;
    QTest::newRow("direct-bcd-resolved-simple") << 4;
    QTest::newRow("manual-with-active-auto-version") << 5;
    QTest::newRow("auto-without-sweep-version") << 6;
    QTest::newRow("auto-source-manual") << 7;
}

void IntegratedPlanningResultTest::_testPlannerSourceBinding()
{
    QFETCH(int, defect);
    auto p = problem();
    if (defect >= 6) {
        p.sweepAngleMode = SweepAngleMode::Auto;
    }
    std::shared_ptr<ICoveragePlanner> planner = std::make_shared<AutoCoveragePlanner>();
    if (defect == 3) {
        planner = std::make_shared<SimpleMonotoneCoveragePlanner>();
    } else if (defect == 4) {
        planner = std::make_shared<BoustrophedonCoveragePlanner>();
    }
    const auto task = taskFor(p, planner->id());
    const auto original = planner->plan(p);
    QCOMPARE(original.status, PlanningStatus::Success);
    QCOMPARE(original.outcome.readiness, MissionReadiness::Ready);
    auto damaged = artifactFor(p, original);
    if (defect == 2) {
        auto other = p;
        other.requestedSweepAngleDeg = 0;
        const auto otherResult = planner->plan(other);
        QCOMPARE(otherResult.status, PlanningStatus::Success);
        QCOMPARE(otherResult.selectedSweepAngleDeg, 0.0);
        QCOMPARE(original.selectedSweepAngleDeg, 90.0);
        QVERIFY(otherResult.path.front().xM != original.path.front().xM ||
                otherResult.path.front().yM != original.path.front().yM);
        damaged = artifactFor(other, otherResult);
        damaged.identity = *PlanningInputIdentity::fromTask(task, damaged.identity.semantics);
    } else if (defect == 0 || defect == 7) {
        damaged.result.plannerSource->requestedSweepMode = defect == 0 ? SweepAngleMode::Auto : SweepAngleMode::Manual;
    } else if (defect == 1) {
        damaged.result.selectedSweepAngleDeg = 0;
        damaged.result.plannerSource->selectedSweepAngleDeg = 0;
    } else if (defect == 3 || defect == 4) {
        const PlannerStrategyIdentity other =
            defect == 3 ? PlannerStrategyIdentity{CoverageStrategySemantics::BoustrophedonId,
                                                  CoverageStrategySemantics::BoustrophedonVersion}
                        : PlannerStrategyIdentity{CoverageStrategySemantics::SimpleMonotoneId,
                                                  CoverageStrategySemantics::SimpleMonotoneVersion};
        damaged.result.plannerSource->resolvedStrategy = other;
        damaged.result.outcome.coverageQuality->strategy = other;
        auto semantics = damaged.identity.semantics;
        semantics.resolvedStrategy = QString::fromStdString(other.strategyId);
        semantics.strategyVersion = QString::fromStdString(other.semanticVersion);
        damaged.identity = *PlanningInputIdentity::fromTask(task, semantics);
    } else {
        damaged.result.plannerSource->sweepSemanticVersion =
            defect == 5 ? CoverageStrategySemantics::GlobalSweepVersion : "";
    }
    QString error;
    QVERIFY(damaged.identity.matchesSupported(task));
    QVERIFY(PlanningArtifactCodec::validateResult(damaged.result, nullptr, error));
    QVERIFY(!PlanningArtifactCodec::validateResult(damaged.result, &task, error));
    QJsonObject rejected;
    QVERIFY(!PlanningArtifactCodec::save(damaged, task, rejected, error));
    // Serialize unbound historical facts to exercise the actual parser with the forged current identity.
    auto changedTask = task;
    changedTask.coverage.swathWidthM += 1;
    QJsonObject malformed;
    QVERIFY2(PlanningArtifactCodec::save(damaged, changedTask, malformed, error), qPrintable(error));
    PlanningArtifact loaded;
    QVERIFY(!PlanningArtifactCodec::load(malformed, task, loaded, error));
    verifyRejected(damaged, task);
}

void IntegratedPlanningResultTest::_testPlannerSourceCurrentAndStale()
{
    for (const double angle : {-270.0, -180.0, 270.0, 360.0}) {
        auto p = problem();
        p.requestedSweepAngleDeg = angle;
        const auto result = AutoCoveragePlanner{}.plan(p);
        QCOMPARE(result.status, PlanningStatus::Success);
        verifyRoundTrip(p, result);
        verifyAllowed(artifactFor(p, result), taskFor(p));
    }
    for (const auto mode : {SweepAngleMode::Manual, SweepAngleMode::Auto}) {
        auto p = problem();
        p.sweepAngleMode = mode;
        for (const bool bcd : {false, true}) {
            if (bcd) {
                p.region.navigationBoundary = rectangle(-5, -5, 25, 25);
                p.region.noGoRegions = {rectangle(8, 8, 12, 12)};
            }
            const auto result = AutoCoveragePlanner{}.plan(p);
            QCOMPARE(result.status, PlanningStatus::Success);
            QCOMPARE(result.plannerSource->resolvedStrategy.strategyId,
                     std::string(bcd ? CoverageStrategySemantics::BoustrophedonId
                                     : CoverageStrategySemantics::SimpleMonotoneId));
            verifyRoundTrip(p, result);
        }
        const auto task = taskFor(p);
        auto future = artifactFor(p, AutoCoveragePlanner{}.plan(p));
        future.result.plannerSource->sweepSemanticVersion = "global-sweep.future";
        QString error;
        QVERIFY(!PlanningArtifactCodec::validateResult(future.result, &task, error));
        QVERIFY(!PlanningArtifactCodec::matchesCurrentInput(future, task));
        QJsonObject raw;
        QVERIFY2(PlanningArtifactCodec::save(future, task, raw, error), qPrintable(error));
        PlanningArtifact loaded;
        QVERIFY2(PlanningArtifactCodec::load(raw, task, loaded, error), qPrintable(error));
        QVERIFY(loaded.stale);
        QCOMPARE(serialized(loaded, task), raw);
        verifyRejected(loaded, task);
        MarinePlanContext context(planController());
        context.addTask(task);
        auto spy = std::make_shared<ForbiddenPlanner>();
        QVERIFY(context.plannerRegistry().registerPlanner(spy));
        CoverageInspectionComplexItem item(planController(), false, &context);
        const auto json = complexObject(raw, task);
        QVERIFY2(item.load(json, 0, error), qPrintable(error));
        QCOMPARE(item.planningState(), CoverageInspectionComplexItem::Unplanned);
        QCOMPARE(item.planningResult().outcome.readiness, MissionReadiness::None);
        QVERIFY(item.planningResult().path.empty());
        QCOMPARE(spy->calls, 0);
        QJsonArray resaved;
        item.save(resaved);
        QCOMPARE(resaved.first().toObject(), json);
    }
}

UT_REGISTER_TEST(IntegratedPlanningResultTest, TestLabel::Unit, TestLabel::MissionManager)
