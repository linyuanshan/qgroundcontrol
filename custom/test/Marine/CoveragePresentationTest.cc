#include "CoveragePresentationTest.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QJsonArray>
#include <QtCore/QScopeGuard>
#include <QtPositioning/QGeoPolygon>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlExpression>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtTest/QSignalSpy>

#include <cmath>
#include <memory>

#include "AutoCoveragePlanner.h"
#include "CoverageGeometry.h"
#include "CoverageInspectionComplexItem.h"
#include "CoveragePlanningPresentation.h"
#include "CoverageRepair.h"
#include "CoverageSafety.h"
#include "CoverageTaskAdapter.h"
#include "IntegratedPlanningResult.h"
#include "MarinePlanContext.h"
#include "MissionController.h"
#include "PlanMasterController.h"
#include "PlanningArtifactCodec.h"
#include "PlanningPathMetrics.h"
#include "QGCPalette.h"
#include "SimpleMonotoneCoveragePlanner.h"

using namespace Marine;

namespace {
QObject* findVisual(QQuickItem* item, const QString& name)
{
    if (!item) {
        return nullptr;
    }
    if (item->objectName() == name) {
        return item;
    }
    for (auto* child : item->childItems()) {
        if (auto* found = findVisual(child, name)) {
            return found;
        }
    }
    return nullptr;
}

Polygon2D rectangle(double low, double high)
{
    return {.vertices = {{low, low}, {high, low}, {high, high}, {low, high}}};
}

CoveragePlanningProblem problem(int scenario)
{
    CoveragePlanningProblem p;
    p.region.coverageBoundary = rectangle(0, 20);
    p.region.navigationBoundary = p.region.coverageBoundary;
    p.safety = {0, 0};
    p.executionSafety.executionMarginM = 0;
    p.swathWidthM = 5;
    p.coverageRequirement = CoverageRequirement::Strict;
    p.sweepAngleMode = SweepAngleMode::Manual;
    p.requestedSweepAngleDeg = 90;
    if (scenario == 1) {
        p.safety = {0, 1};
    }
    if (scenario == 2) {
        p.safety = {2, 2};
    }
    if (scenario == 3) {
        p.swathWidthM = 1e150;
    }
    if (scenario == 4) {
        p.executionSafety.executionMarginM = 30;
        p.region.navigationBoundary = rectangle(-5, 25);
    }
    if (scenario == 5) {
        p.region.noGoRegions = {rectangle(8, 12)};
        p.region.navigationBoundary = rectangle(-5, 25);
        p.executionSafety.executionMarginM = 100;
    }
    if (scenario == 6) {
        p.safety = {1, 2};
    }
    if (scenario == 7) {
        p.safety = {-1, 0};
    }
    return p;
}

MarineTask taskFor(const CoveragePlanningProblem& p, const std::string& id)
{
    MarineTask t;
    t.planner.plannerId = id;
    t.safety = p.safety;
    t.planner.executionSafety = p.executionSafety;
    t.coverage = {p.swathWidthM, p.coverageRequirement, p.sweepAngleMode, p.requestedSweepAngleDeg};
    const auto ref = GeoReference::create(GeoPoint{38, 121, 0});
    const auto polygon = [&ref](const Polygon2D& source) {
        GeoPolygon target;
        for (const auto& point : source.vertices) {
            target.vertices.push_back(*ref->toGeo(point));
        }
        return target;
    };
    t.region.coverageBoundary = polygon(p.region.coverageBoundary);
    t.region.navigationBoundary = polygon(p.region.navigationBoundary);
    for (const auto& o : p.region.noGoRegions) {
        t.region.noGoRegions.push_back(polygon(o));
    }
    return t;
}

PlanningArtifact artifactFor(const MarineTask& task, const CoveragePlanningSolution& solution)
{
    PlanningSemantics semantics;
    if (solution.plannerSource) {
        semantics.resolvedStrategy = QString::fromStdString(solution.plannerSource->resolvedStrategy.strategyId);
        semantics.strategyVersion = QString::fromStdString(solution.plannerSource->resolvedStrategy.semanticVersion);
    }
    return {CoverageTaskAdapter::toPlanningResult(solution, *GeoReference::create(GeoPoint{38, 121, 0})),
            *PlanningInputIdentity::fromTask(task, semantics), false, PlanningResultContract::IntegratedV05};
}

QJsonObject savedComplex(const MarineTask& task, const PlanningArtifact& artifact, QString& error)
{
    QJsonObject json;
    if (!PlanningArtifactCodec::save(artifact, task, json, error)) {
        return {};
    }
    json.insert("type", "ComplexItem");
    json.insert("complexItemType", "coverageInspection");
    json.insert("taskId", QString::fromStdString(task.id));
    return json;
}

QString qmlErrors(const QQmlComponent& component)
{
    return component.errorString();
}
}  // namespace

void CoveragePresentationTest::_testResultMatrix_data()
{
    QTest::addColumn<int>("scenario");
    QTest::addColumn<QString>("readiness");
    QTest::addColumn<QString>("tier");
    QTest::addColumn<bool>("allowed");
    QTest::newRow("T09-01-Ready") << 0 << QString("Ready") << QString("D0") << true;
    QTest::newRow("T09-02-Warning") << 1 << QString("ReadyWithWarning") << QString("D1") << true;
    QTest::newRow("T09-03-Review") << 2 << QString("ReviewRequired") << QString("D0") << false;
    QTest::newRow("T09-06-AssessmentError") << 3 << QString("ReviewRequired") << QString("D0") << false;
    QTest::newRow("T09-04-D2") << 4 << QString("DiagnosticOnly") << QString("D2") << false;
    QTest::newRow("T09-05-D3") << 5 << QString("DiagnosticOnly") << QString("D3") << false;
    QTest::newRow("T09-08-Repair") << 6 << QString("ReadyWithWarning") << QString("D1") << true;
}

void CoveragePresentationTest::_testResultMatrix()
{
    QFETCH(int, scenario);
    QFETCH(QString, readiness);
    QFETCH(QString, tier);
    QFETCH(bool, allowed);
    const auto p = problem(scenario);
    const bool simple = scenario == 5;
    const auto solution = simple ? SimpleMonotoneCoveragePlanner{}.plan(p) : AutoCoveragePlanner{}.plan(p);
    const auto task =
        taskFor(p, simple ? CoverageStrategySemantics::SimpleMonotoneId : CoverageStrategySemantics::AutoPlannerId);
    MarinePlanContext context(planController());
    context.addTask(task);
    // Registry deliberately has no planner: exact restore and presentation must not solve.
    CoverageInspectionComplexItem item(planController(), false, &context);
    QString error;
    const auto json = savedComplex(task, artifactFor(task, solution), error);
    QVERIFY2(!json.isEmpty(), qPrintable(error));
    QVERIFY2(item.load(json, 1, error), qPrintable(error));
    QCOMPARE(item.planningPresentation().value("readiness").toString(), readiness);
    QCOMPARE(item.planningPresentation().value("tier").toString(), tier);
    QCOMPARE(item.uploadAllowed(), allowed);
    QJsonArray restored;
    item.save(restored);
    QCOMPARE(restored.first().toObject(), json);
    QQmlEngine engine;
    engine.addImportPath("qrc:/qml");
    QQmlComponent component(&engine, QUrl(item.property("editorQml").toString()));
    QVERIFY2(component.isReady(), qPrintable(qmlErrors(component)));
    std::unique_ptr<QObject> editor(
        component.createWithInitialProperties({{"missionItem", QVariant::fromValue(&item)}, {"availableWidth", 500}}));
    QVERIFY2(editor != nullptr, qPrintable(qmlErrors(component)));
    auto* state = editor->findChild<QObject*>("marine_readiness");
    QVERIFY(state);
    QVERIFY(!state->property("text").toString().isEmpty());
    QVERIFY(editor->findChild<QObject*>("marine_ingressNotice"));
    const auto display = item.planningPresentation();
    if (scenario == 2 || scenario == 3) {
        QVERIFY(!display.value("canonicalRuns").toList().isEmpty());
        QVERIFY(!item.generatedPath().isEmpty());
    }
    if (scenario == 3) {
        const auto q = display.value("quality").toMap();
        QCOMPARE(q.value("status").toString(), QString("AssessmentError"));
        QVERIFY(!q.value("coverageRatio").toMap().value("available").toBool());
        QVERIFY(q.value("coverageRatio").toMap().value("value").isNull());
        QVERIFY(findVisual(qobject_cast<QQuickItem*>(editor.get()), "marine_issue_CoverageAssessmentFailed"));
        QVERIFY(!findVisual(qobject_cast<QQuickItem*>(editor.get()), "marine_issue_CoverageBelowRequirement"));
    }
    if (scenario == 4 || scenario == 5) {
        QVERIFY(display.value("canonicalRuns").toList().isEmpty());
        QVERIFY(item.generatedPath().isEmpty());
    }
    if (scenario == 4) {
        QVERIFY(!display.value("diagnosticCandidate").toMap().value("runs").toList().isEmpty());
    }
    if (scenario == 5) {
        QVERIFY(!display.value("diagnosticOverlays").toList().isEmpty());
    }
    if (scenario == 6) {
        const auto repair = display.value("repair").toMap();
        QVERIFY(repair.value("applied").toBool());
        QCOMPARE(repair.value("components").toList().size(),
                 static_cast<qsizetype>(solution.outcome.repair.components.size()));
        const auto c = repair.value("components").toList().front().toMap();
        QCOMPARE(c.value("componentId").toUInt(), solution.outcome.repair.components.front().componentId);
        QCOMPARE(c.value("transitionCostM").toDouble(), solution.outcome.repair.components.front().transitionCostM);
    }
}

void CoveragePresentationTest::_testRepairPresentation_data()
{
    QTest::addColumn<bool>("applied");
    QTest::newRow("T09-08-applied-stored") << true;
    QTest::newRow("T09-08-attempted-not-applied-stored") << false;
}

void CoveragePresentationTest::_testRepairPresentation()
{
    QFETCH(bool, applied);
    const auto p = problem(applied ? 6 : 2);
    auto solution = AutoCoveragePlanner{}.plan(p);
    QCOMPARE(solution.status, PlanningStatus::Success);
    QVERIFY(solution.outcome.selectedCandidateIndex);
    QVERIFY(solution.outcome.coverageQuality);
    if (!applied) {
        // Reapply the real repair operation to its retained Insufficient candidate. Already selected
        // boundary components cannot improve it again (CoverageRepairTest's Equivalent-trial oracle).
        QCOMPARE(solution.outcome.coverageQuality->status, CoverageQualityStatus::Insufficient);
        const auto selected = *solution.outcome.selectedCandidateIndex;
        const auto& candidate = solution.repairCandidates.at(selected).candidate;
        const auto geometry = buildCoverageGeometry(p.region);
        const auto safety = buildSafetyTrackRegions(p.region, p.safety, p.executionSafety);
        QCOMPARE(geometry.error, CoveragePlanningError::None);
        QCOMPARE(safety.error, CoveragePlanningError::None);
        const auto assessment = evaluateSafetyCandidate(safety, candidate.path);
        QCOMPARE(assessment.error, CoveragePlanningError::None);
        const auto& active = candidate.preferredSafe ? safety.regions.preferredExecutionTrackRegion
                                                     : safety.regions.hardExecutionTrackRegion;
        const auto repeated = repairCoverageCandidate(geometry.geometry.coverageTarget, active, safety, p.swathWidthM,
                                                      p.coverageRequirement, candidate.quality.strategy, candidate);
        QVERIFY(repeated.attempted);
        QVERIFY(repeated.steps.empty());
        QCOMPARE(repeated.candidate.path.size(), solution.path.size());
        for (std::size_t i = 0; i < solution.path.size(); ++i) {
            QCOMPARE(repeated.candidate.path[i].xM, solution.path[i].xM);
            QCOMPARE(repeated.candidate.path[i].yM, solution.path[i].yM);
        }
        QCOMPARE(repeated.candidate.legRoles, solution.legRoles);
        QCOMPARE(repeated.candidate.metrics.pathLengthM, solution.pathLengthM);
        QCOMPARE(repeated.candidate.metrics.turnCount, solution.turnCount);
        // Publish this actual no-application operation, not a manually edited applied/attempted flag.
        solution.outcome.repair = {};
        publishCanonicalOutcome(solution, assessment, selected, repeated, candidate.metrics, false);
    }
    QVERIFY(solution.outcome.repair.attempted);
    QCOMPARE(solution.outcome.repair.applied, applied);
    QCOMPARE(solution.outcome.repair.reason,
             applied ? CoverageRepairReason::AppliedPolicyPass : CoverageRepairReason::NoUsefulRepair);
    const auto task = taskFor(p, CoverageStrategySemantics::AutoPlannerId);
    MarinePlanContext context(planController());
    context.addTask(task);
    CoverageInspectionComplexItem item(planController(), false, &context);
    QString error;
    const auto json = savedComplex(task, artifactFor(task, solution), error);
    QVERIFY2(!json.isEmpty(), qPrintable(error));
    QVERIFY2(item.load(json, 1, error), qPrintable(error));
    QJsonArray restored;
    item.save(restored);
    QCOMPARE(restored.first().toObject(), json);
    const auto stored = json.value("outcome").toObject().value("repair").toObject();
    const auto repair = item.planningPresentation().value("repair").toMap();
    QCOMPARE(repair.size(), 4);
    for (const auto* key : {"attempted", "applied", "reason"}) {
        QCOMPARE(repair.value(key), stored.value(key).toVariant());
    }
    const auto expectedComponents = stored.value("components").toArray();
    const auto components = repair.value("components").toList();
    QCOMPARE(components.size(), expectedComponents.size());
    QCOMPARE(components.isEmpty(), !applied);
    const auto outcome = json.value("outcome").toObject();
    for (const auto* kind : {"issues", "suggestions"}) {
        const auto shown = item.planningPresentation().value(kind).toList();
        const auto saved = outcome.value(kind).toArray();
        QCOMPARE(shown.size(), saved.size());
        for (qsizetype i = 0; i < shown.size(); ++i) {
            QCOMPARE(shown[i].toMap().value("code").toString(), saved[i].toObject().value("code").toString());
        }
    }

    const auto coordinates = [](const QJsonArray& path) {
        QList<QGeoCoordinate> points;
        for (const auto& value : path) {
            const auto tuple = value.toArray();
            points.append(QGeoCoordinate(tuple[0].toDouble(), tuple[1].toDouble(), tuple[2].toDouble()));
        }
        return points;
    };
    const auto compareQuality = [&coordinates](const QVariantMap& view, const QJsonObject& fact) {
        QCOMPARE(view.size(), 20);
        const QMap<QString, QString> metadata{{"status", "status"},
                                              {"error", "error"},
                                              {"requirement", "requirement"},
                                              {"strategy", "strategyId"},
                                              {"strategyVersion", "strategyVersion"},
                                              {"policyVersion", "policySemanticVersion"},
                                              {"passesRequirement", "passesRequirement"},
                                              {"strictFallbackTriggered", "strictFallbackTriggered"},
                                              {"wholeTargetStrictFallback", "wholeTargetStrictFallback"}};
        for (auto i = metadata.cbegin(); i != metadata.cend(); ++i) {
            QCOMPARE(view.value(i.key()), fact.value(i.value()).toVariant());
        }
        for (const auto* key : {"targetAreaM2", "coveredAreaM2", "uncoveredAreaM2", "coverageRatio",
                                "criticalUncoveredAreaM2", "numericalToleranceM2"}) {
            const auto actual = view.value(key).toMap();
            const auto expected = fact.value(key).toObject();
            QCOMPARE(actual.size(), 2);
            QCOMPARE(actual.value("available").toBool(), expected.value("available").toBool());
            QCOMPARE(actual.value("value").isNull(), expected.value("value").isNull());
            if (expected.value("available").toBool()) {
                QCOMPARE(actual.value("value").toDouble(), expected.value("value").toDouble());
            } else {
                QVERIFY(!actual.value("value").isValid());
            }
        }
        const QMap<QString, QString> geometries{{"criticalCoverageCore", "criticalCoverageCore"},
                                                {"uncovered", "uncoveredRegion"},
                                                {"criticalUncovered", "criticalUncoveredRegion"},
                                                {"boundaryShortfall", "boundaryShortfallRegion"},
                                                {"fallbackComponents", "strictFallbackTargetComponents"}};
        for (auto i = geometries.cbegin(); i != geometries.cend(); ++i) {
            const auto actual = view.value(i.key()).toMap();
            const auto expected = fact.value(i.value()).toObject();
            QCOMPARE(actual.size(), 2);
            QCOMPARE(actual.value("available").toBool(), expected.value("available").toBool());
            QCOMPARE(actual.value("value").isNull(), expected.value("value").isNull());
            if (!expected.value("available").toBool()) {
                QVERIFY(!actual.value("value").isValid());
                continue;
            }
            const auto regions = actual.value("value").toList();
            const auto storedRegions = expected.value("value").toArray();
            QCOMPARE(regions.size(), storedRegions.size());
            for (qsizetype index = 0; index < regions.size(); ++index) {
                const auto region = regions[index].toMap();
                const auto source = storedRegions[index].toObject();
                const auto outer = coordinates(source.value("outer").toArray());
                const QGeoPolygon polygon(region.value("geoShape").value<QGeoShape>());
                QCOMPARE(region.value("componentIndex").toULongLong(), static_cast<qulonglong>(index));
                QCOMPARE(region.value("labelCoordinate").value<QGeoCoordinate>(), outer.front());
                QCOMPARE(polygon.perimeter(), outer);
                const auto holes = source.value("holes").toArray();
                QCOMPARE(polygon.holesCount(), holes.size());
                for (qsizetype hole = 0; hole < holes.size(); ++hole) {
                    QCOMPARE(polygon.holePath(hole), coordinates(holes[hole].toArray()));
                }
            }
        }
    };
    for (qsizetype i = 0; i < components.size(); ++i) {
        const auto view = components[i].toMap();
        const auto fact = expectedComponents[i].toObject();
        QCOMPARE(view.size(), 11);
        for (const auto* key : {"componentId", "entryIndex", "reverse", "transitionCostM", "pathLengthBeforeM",
                                "pathLengthAfterM", "turnCountBefore", "turnCountAfter"}) {
            QCOMPARE(view.value(key), fact.value(key).toVariant());
        }
        const auto path = view.value("path").toList();
        const auto expectedPath = coordinates(fact.value("componentPath").toArray());
        QCOMPARE(path.size(), expectedPath.size());
        for (qsizetype point = 0; point < path.size(); ++point) {
            QCOMPARE(path[point].value<QGeoCoordinate>(), expectedPath[point]);
        }
        compareQuality(view.value("before").toMap(), fact.value("before").toObject());
        compareQuality(view.value("after").toMap(), fact.value("after").toObject());
    }

    QQmlEngine engine;
    engine.addImportPath("qrc:/qml");
    QQmlComponent component(&engine, QUrl(item.property("editorQml").toString()));
    QVERIFY2(component.isReady(), qPrintable(qmlErrors(component)));
    std::unique_ptr<QObject> editor(
        component.createWithInitialProperties({{"missionItem", QVariant::fromValue(&item)}, {"availableWidth", 500}}));
    QVERIFY2(editor != nullptr, qPrintable(qmlErrors(component)));
    const auto translated = [](const char* text) {
        return QCoreApplication::translate("CoverageInspectionEditor", text);
    };
    const auto yesNo = [&translated](bool value) { return translated(value ? "Yes" : "No"); };
    auto* summary = editor->findChild<QObject*>("marine_repairSummary");
    QVERIFY(summary);
    QTRY_VERIFY(summary->property("visible").toBool() && summary->property("height").toDouble() > 0);
    QCOMPARE(summary->property("text").toString(), translated("Repair attempted: %1; applied: %2\nReason: %3")
                                                       .arg(yesNo(stored.value("attempted").toBool()))
                                                       .arg(yesNo(stored.value("applied").toBool()))
                                                       .arg(stored.value("reason").toString()));
    const auto measurement = [&translated](const QJsonObject& value) {
        return value.value("available").toBool()
                   ? QJSValue(value.value("value").toDouble()).toString() + QString::fromUtf8(" m²")
                   : translated("Unavailable");
    };
    // Reliable repair quality normally has available numeric facts. Exercise the actual editor's
    // unavailable formatting too, without inventing an invalid persisted repair component.
    QQmlExpression unavailable(engine.rootContext(), editor.get(),
                               "measurement({available: false, value: null}, ' m²')");
    QCOMPARE(unavailable.evaluate().toString(), translated("Unavailable"));
    QVERIFY(!unavailable.hasError());
    for (const auto& value : expectedComponents) {
        const auto fact = value.toObject();
        const auto name = QString("marine_repairComponent_%1").arg(fact.value("componentId").toInt());
        QTRY_VERIFY(findVisual(qobject_cast<QQuickItem*>(editor.get()), name));
        auto* label = findVisual(qobject_cast<QQuickItem*>(editor.get()), name);
        QVERIFY(label->property("visible").toBool());
        QVERIFY(label->property("height").toDouble() > 0);
        QCOMPARE(label->property("text").toString(),
                 translated("Component %1; entry %2; reverse %3; transit cost %4 m\nUncovered before / after: %5 / "
                            "%6\nLength before / after: %7 / %8 m; turns: %9 / %10")
                     .arg(fact.value("componentId").toInt())
                     .arg(fact.value("entryIndex").toInt())
                     .arg(yesNo(fact.value("reverse").toBool()))
                     .arg(fact.value("transitionCostM").toDouble(), 0, 'f', 2)
                     .arg(measurement(fact.value("before").toObject().value("uncoveredAreaM2").toObject()))
                     .arg(measurement(fact.value("after").toObject().value("uncoveredAreaM2").toObject()))
                     .arg(fact.value("pathLengthBeforeM").toDouble())
                     .arg(fact.value("pathLengthAfterM").toDouble())
                     .arg(fact.value("turnCountBefore").toInt())
                     .arg(fact.value("turnCountAfter").toInt()));
    }
    if (!applied) {
        QVERIFY(!findVisual(qobject_cast<QQuickItem*>(editor.get()), "marine_repairComponent_0"));
    }
}

void CoveragePresentationTest::_testIndependentEditingAndStale()
{
    auto task = taskFor(problem(0), CoverageStrategySemantics::AutoPlannerId);
    MarinePlanContext context(planController());
    context.addTask(task);
    QVERIFY(context.plannerRegistry().registerPlanner(std::make_shared<AutoCoveragePlanner>()));
    CoverageInspectionComplexItem item(planController(), false, &context);
    item.setTaskId(QString::fromStdString(task.id));
    QVERIFY(item.plan());
    QSignalSpy changed(&item, &CoverageInspectionComplexItem::planningResultChanged);
    QSignalSpy admission(&item, &VisualMissionItem::uploadReadinessChanged);
    item.setTaskName("Rename");
    item.setSonarRecord(false);
    QVERIFY(item.uploadAllowed());
    QVERIFY(!item.resultStale());
    item.setEditingRegion(1);
    QVERIFY(item.navigationPolygon()->interactive());
    QVERIFY(!item.workRegionPolygon()->interactive());
    QVERIFY(item.uploadAllowed());
    const auto oldC = context.task(task.id)->region.coverageBoundary.vertices.front();
    item.navigationPolygon()->adjustVertex(0, QGeoCoordinate(37.999, 120.999));
    // QGC defers pathChanged; even before that notification the backend must reject edited geometry.
    QVERIFY(!item.uploadAllowed());
    QCOMPARE(context.task(task.id)->region.coverageBoundary.vertices.front().latitudeDeg, oldC.latitudeDeg);
    QTRY_VERIFY(item.resultStale());
    QVERIFY(!item.uploadAllowed());
    QVERIFY(!item.planningPresentation().value("hasCurrentResult").toBool());
    QVERIFY(item.generatedPath().isEmpty());
    QVERIFY(!changed.isEmpty());
    QVERIFY(!admission.isEmpty());
    const auto oldN = item.navigationPolygon()->coordinateList();
    item.workRegionPolygon()->adjustVertex(0, QGeoCoordinate(38.00001, 121.00001));
    QTRY_COMPARE(context.task(task.id)->region.coverageBoundary.vertices.front().latitudeDeg, 38.00001);
    QCOMPARE(item.navigationPolygon()->coordinateList(), oldN);
    item.setSafetyMarginM(3);
    QCOMPARE(item.preferredSafetyMarginM(), 0.0);
    QVERIFY(!context.task(task.id)->schemaValid());
    item.setPreferredSafetyMarginM(4);
    item.setExecutionMarginM(1);
    item.setCoverageRequirement("Standard");
    QCOMPARE(item.safetyMarginM(), 3.0);
    QCOMPARE(item.preferredSafetyMarginM(), 4.0);
    QCOMPARE(item.executionMarginM(), 1.0);
    QCOMPARE(context.task(task.id)->coverage.coverageRequirement, CoverageRequirement::Standard);
    task = taskFor(problem(0), CoverageStrategySemantics::AutoPlannerId);
    context.addTask(task);
    item.setTaskId(QString::fromStdString(task.id));
    QVERIFY(item.plan());
    item.navigationPolygon()->setVertexDrag(true);
    QVERIFY(item.resultStale());
    QVERIFY(!item.uploadAllowed());
    item.navigationPolygon()->setVertexDrag(false);
    QVERIFY(item.plan());
    QVERIFY(item.addNoGoRegion());
    QVERIFY(!item.uploadAllowed());
    QVERIFY(!item.noGoRegionsReady());
    QCOMPARE(item.editingRegion(), 2);

    auto outside = problem(0);
    outside.region.navigationBoundary = rectangle(-5, 25);
    outside.region.noGoRegions = {rectangle(21, 24)};
    task = taskFor(outside, CoverageStrategySemantics::AutoPlannerId);
    context.addTask(task);
    item.setTaskId(QString::fromStdString(task.id));
    QCOMPARE(item.noGoPolygons()->count(), 1);
    const auto& restoredNoGo = context.task(task.id)->region.noGoRegions.front().vertices;
    QCOMPARE(restoredNoGo.size(), task.region.noGoRegions.front().vertices.size());
    for (std::size_t i = 0; i < restoredNoGo.size(); ++i) {
        QCOMPARE(restoredNoGo[i].latitudeDeg, task.region.noGoRegions.front().vertices[i].latitudeDeg);
        QCOMPARE(restoredNoGo[i].longitudeDeg, task.region.noGoRegions.front().vertices[i].longitudeDeg);
    }
    QVERIFY(item.plan());
    QVERIFY(item.uploadAllowed());
    auto* polygon = item.noGoPolygons()->value<QGCMapPolygon*>(0);
    polygon->adjustVertex(0, QGeoCoordinate(37.999, 120.999));
    QVERIFY(!item.uploadAllowed());
    QVERIFY(item.resultStale());
    QVERIFY(!item.planningPresentation().value("hasCurrentResult").toBool());
}

void CoveragePresentationTest::_testActualEditorControls()
{
    const auto task = taskFor(problem(0), CoverageStrategySemantics::AutoPlannerId);
    MarinePlanContext context(planController());
    context.addTask(task);
    QVERIFY(context.plannerRegistry().registerPlanner(std::make_shared<AutoCoveragePlanner>()));
    CoverageInspectionComplexItem item(planController(), false, &context);
    item.setTaskId(QString::fromStdString(task.id));
    QVERIFY(item.plan());
    QQmlEngine engine;
    engine.addImportPath("qrc:/qml");
    QQmlComponent component(&engine, QUrl(item.property("editorQml").toString()));
    std::unique_ptr<QObject> editor(
        component.createWithInitialProperties({{"missionItem", QVariant::fromValue(&item)}, {"availableWidth", 500}}));
    QVERIFY2(editor != nullptr, qPrintable(qmlErrors(component)));
    auto* root = qobject_cast<QQuickItem*>(editor.get());
    QQmlExpression tolerance(engine.rootContext(), editor.get(), "measurement({available: true, value: 1e-12}, ' m²')");
    const auto toleranceText = tolerance.evaluate().toString();
    QVERIFY2(!tolerance.hasError(), qPrintable(tolerance.error().toString()));
    QVERIFY(toleranceText.startsWith("1e-12"));  // A real nonzero tolerance must never read as zero.
    QQmlExpression ratio(engine.rootContext(), editor.get(), "measurement({available: true, value: 0.99451}, '')");
    QCOMPARE(ratio.evaluate().toString(), QString("0.99451"));
    QVERIFY2(!ratio.hasError(), qPrintable(ratio.error().toString()));
    for (const auto& kind :
         {"CanonicalPathLegRange", "DiagnosticCandidateLegRange", "CoverageResidual", "DiagnosticOverlay"}) {
        const QString expression =
            QString("referenceText({kind: '%1', index: 2, firstLeg: 1, legCount: 3, residual: 'CriticalUncovered'})")
                .arg(kind);
        QQmlExpression reference(engine.rootContext(), editor.get(), expression);
        const auto text = reference.evaluate().toString();
        QVERIFY2(!reference.hasError(), qPrintable(reference.error().toString()));
        QVERIFY(text.startsWith(kind));
        QCOMPARE(text.contains("CriticalUncovered"), QString(kind) == "CoverageResidual");
        QCOMPARE(text.contains("1+3"), QString(kind).endsWith("LegRange"));
        QCOMPARE(text.contains(" / 2"), QString(kind) != "CoverageResidual");
    }
    auto* hard = findVisual(root, "marine_hardClearance");
    QVERIFY(hard);
    hard->setProperty("text", "3");
    QVERIFY(QMetaObject::invokeMethod(hard, "textEdited"));
    QVERIFY(!item.uploadAllowed());
    QVERIFY(item.generatedPath().isEmpty());
    QCOMPARE(item.safetyMarginM(), 0.0);  // Pending text has not been committed as Task input.
    QVERIFY(QMetaObject::invokeMethod(hard, "editingFinished"));
    QCOMPARE(item.safetyMarginM(), 3.0);
    QCOMPARE(item.preferredSafetyMarginM(), 0.0);
    QVERIFY(!context.task(task.id)->schemaValid());
    for (const auto& field : {std::pair{"marine_preferredClearance", "4"}, std::pair{"marine_executionReserve", "1"},
                              std::pair{"marine_manualBearing", "35"}}) {
        auto* control = findVisual(root, field.first);
        QVERIFY(control);
        control->setProperty("text", field.second);
        QVERIFY(QMetaObject::invokeMethod(control, "editingFinished"));
    }
    QCOMPARE(item.preferredSafetyMarginM(), 4.0);
    QCOMPARE(item.executionMarginM(), 1.0);
    QCOMPARE(item.sweepAngleDeg(), 35.0);
    auto* requirement = findVisual(root, "marine_requirement");
    auto* mode = findVisual(root, "marine_sweepMode");
    QVERIFY(requirement);
    QVERIFY(mode);
    QVERIFY(QMetaObject::invokeMethod(requirement, "activated", Q_ARG(int, 0)));
    QCOMPARE(item.coverageRequirement(), QString("Standard"));
    QVERIFY(QMetaObject::invokeMethod(mode, "activated", Q_ARG(int, 0)));
    QVERIFY(item.automaticSweepAngle());
    QCOMPARE(item.plannerId(), QString::fromStdString(CoverageStrategySemantics::AutoPlannerId));
    QVERIFY(QMetaObject::invokeMethod(mode, "activated", Q_ARG(int, 1)));
    QVERIFY(!item.automaticSweepAngle());
    hard->setProperty("text", "");
    QVERIFY(QMetaObject::invokeMethod(hard, "editingFinished"));
    QVERIFY(std::isnan(item.safetyMarginM()));
    QCOMPARE(hard->property("text").toString(), QString());
    QCOMPARE(item.preferredSafetyMarginM(), 4.0);
    QCOMPARE(item.executionMarginM(), 1.0);
    context.addTask(task);
    QCOMPARE(hard->property("text").toString(), QString("0"));
    QVERIFY(item.plan());
    QVERIFY(item.uploadAllowed());
}

void CoveragePresentationTest::_testCodesReferencesAndSafetyRuns()
{
    PlanningResult result;
    result.path = {{38, 121, 0}, {38.001, 121, 0}, {38.002, 121, 0}, {38.003, 121, 0}};
    result.legRoles = {PathLegRole::Coverage, PathLegRole::Coverage, PathLegRole::Transit};
    result.outcome.canonicalLegAssessments = {SafetyLegClass::PreferredSafe, SafetyLegClass::HardSafeWarning,
                                              SafetyLegClass::HardSafeWarning};
    for (int i = 0; i <= static_cast<int>(PlanningIssueCode::CoverageAssessmentFailed); ++i) {
        result.outcome.issues.push_back(
            {static_cast<PlanningIssueCode>(i), PlanningIssueSeverity::Warning, "source detail",
             PlanningReference{
                 .kind = static_cast<PlanningReferenceKind>(i % 4), .index = 2, .firstLeg = 1, .legCount = 2}});
    }
    for (int i = 0; i <= static_cast<int>(PlanningSuggestionCode::InspectRepairCost); ++i) {
        result.outcome.suggestions.push_back({static_cast<PlanningSuggestionCode>(i),
                                              PlanningIssueCode::PreferredSafetyViolated, "suggestion detail",
                                              PlanningReference{}});
    }
    const auto display = Marine::QGC::planningPresentation(result, true, false);
    const auto runs = display.value("canonicalRuns").toList();
    QCOMPARE(runs.size(), 3);
    QCOMPARE(runs[1].toMap().value("firstLeg").toULongLong(), 1ULL);
    QCOMPARE(runs[1].toMap().value("safetyClass").toString(), QString("HardSafeWarning"));
    QSet<QString> codes;
    for (const auto& v : display.value("issues").toList()) {
        const auto issue = v.toMap();
        QVERIFY(!issue.value("message").toString().isEmpty());
        QCOMPARE(issue.value("sourceMessage").toString(), QString("source detail"));
        QVERIFY(!issue.value("reference").toMap().value("kind").toString().isEmpty());
        codes.insert(issue.value("code").toString());
    }
    QCOMPARE(codes.size(), 15);
    codes.clear();
    for (const auto& v : display.value("suggestions").toList()) {
        const auto suggestion = v.toMap();
        QVERIFY(!suggestion.value("message").toString().isEmpty());
        codes.insert(suggestion.value("code").toString());
    }
    QCOMPARE(codes.size(), 6);
    const auto empty = Marine::QGC::planningPresentation({}, true, false);
    QVERIFY(empty.value("suggestions").toList().isEmpty());
    const auto invalid = AutoCoveragePlanner{}.plan(problem(7));
    const auto invalidDisplay = Marine::QGC::planningPresentation(
        CoverageTaskAdapter::toPlanningResult(invalid, *GeoReference::create(GeoPoint{38, 121, 0})), true, false);
    QCOMPARE(invalidDisplay.value("status").toString(), QString("InvalidInput"));
    QCOMPARE(invalidDisplay.value("readiness").toString(), QString("None"));
}

void CoveragePresentationTest::_testProductMapHolesAndPalettes()
{
    auto p = problem(0);
    p.region.coverageBoundary = rectangle(-400, 400);
    p.region.navigationBoundary = p.region.coverageBoundary;
    p.swathWidthM = 100;
    CoveragePlanningSolution solution = SimpleMonotoneCoveragePlanner{}.plan(p);
    solution.path = {{-200, -200}, {200, -200}, {200, 200}, {-200, 200}, {-200, -200}};
    solution.legRoles.assign(4, PathLegRole::Coverage);
    const auto metrics = calculatePlanningPathMetrics(solution.path, solution.legRoles);
    QVERIFY(metrics);
    solution.pathLengthM = metrics->pathLengthM;
    solution.coverageLengthM = metrics->coverageLengthM;
    solution.transitLengthM = metrics->transitLengthM;
    solution.turnCount = metrics->turnCount;
    solution.outcome.coverageQuality = evaluateCoverageQuality(
        buildCoverageGeometry(p.region).geometry.coverageTarget, solution.path, solution.legRoles, p.swathWidthM,
        p.coverageRequirement, solution.plannerSource->resolvedStrategy);
    const auto safety =
        evaluateSafetyCandidate(buildSafetyTrackRegions(p.region, p.safety, p.executionSafety), solution.path);
    QCOMPARE(safety.error, CoveragePlanningError::None);
    publishCanonicalOutcome(solution, safety, 0, {}, *metrics, false);
    const auto task = taskFor(p, CoverageStrategySemantics::SimpleMonotoneId);
    MarinePlanContext context(planController());
    context.addTask(task);
    CoverageInspectionComplexItem item(planController(), false, &context);
    QString error;
    const auto json = savedComplex(task, artifactFor(task, solution), error);
    QVERIFY2(!json.isEmpty(), qPrintable(error));
    QVERIFY2(item.load(json, 1, error), qPrintable(error));
    item.setIsCurrentItem(true);
    const auto oldTheme = QGCPalette::globalTheme();
    const auto restore = qScopeGuard([oldTheme] { QGCPalette::setGlobalTheme(oldTheme); });
    for (const auto theme : {QGCPalette::Light, QGCPalette::Dark}) {
        QGCPalette::setGlobalTheme(theme);
        QQmlEngine engine;
        engine.addImportPath("qrc:/qml");
        engine.rootContext()->setContextProperty("object", &item);
        QQmlComponent component(&engine);
        component.setData(R"QML(
import QtQuick
import QtQuick.Window
import QtLocation
import QtPositioning
Window {
    width: 650; height: 650; visible: true; color: "white"
    Map { id: m; objectName: "renderMap"; anchors.fill: parent
        plugin: Plugin { name: "itemsoverlay" }
        center: QtPositioning.coordinate(38, 121); zoomLevel: 15
    }
    Loader {
        Component.onCompleted: setSource("qrc:/qml/Marine/Plan/CoverageInspectionMapVisual.qml", {map: m, interactive: false})
    }
}
)QML",
                          QUrl("qrc:/qml/marine-render-test.qml"));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object != nullptr, qPrintable(component.errorString()));
        auto* window = qobject_cast<QQuickWindow*>(object.get());
        QVERIFY(window);
        auto* map = object->findChild<QObject*>("renderMap");
        QVERIFY(map);
        QVERIFY(waitForCondition([&] { return map->property("mapReady").toBool(); }, 5000));
        QSignalSpy frames(window, &QQuickWindow::frameSwapped);
        window->update();
        QVERIFY(waitForCondition(
            [&] {
                window->update();
                return frames.size() >= 3;
            },
            5000));
        int holeCount = 0;
        int regionCount = 0;
        for (auto* shape : map->findChildren<QObject*>()) {
            if (!shape->objectName().startsWith("marine_region_CriticalUncovered_")) {
                continue;
            }
            ++regionCount;
            const QGeoPolygon polygon(qvariant_cast<QGeoShape>(shape->property("geoShape")));
            holeCount += static_cast<int>(polygon.holesCount());
        }
        QVERIFY(regionCount > 1);
        QVERIFY(holeCount > 0);
        QList<QRectF> textBounds;
        for (auto* label : map->findChildren<QObject*>("marine_mapLabel")) {
            auto* textItem = qobject_cast<QQuickItem*>(label->property("sourceItem").value<QObject*>());
            QVERIFY(textItem);
            const auto bounds = textItem->mapRectToScene(QRectF(0, 0, textItem->width(), textItem->height()));
            QVERIFY(!bounds.isEmpty());
            for (const auto& earlier : textBounds) {
                QVERIFY2(!bounds.intersects(earlier), "Product map labels overlap");
            }
            textBounds.append(bounds);
        }
        QVERIFY(textBounds.size() >= 6);
        const auto snapshot = window->grabWindow();
        QVERIFY(!snapshot.isNull());
        const auto ref = GeoReference::create(GeoPoint{38, 121, 0});
        engine.rootContext()->setContextProperty(
            "sample",
            QVariant::fromValue(QGeoCoordinate(ref->toGeo({180, 0})->latitudeDeg, ref->toGeo({180, 0})->longitudeDeg)));
        const auto holePoint =
            QQmlExpression(engine.rootContext(), map, "fromCoordinate(sample, false)").evaluate().toPointF();
        engine.rootContext()->setContextProperty(
            "sample",
            QVariant::fromValue(QGeoCoordinate(ref->toGeo({300, 0})->latitudeDeg, ref->toGeo({300, 0})->longitudeDeg)));
        const auto fillPoint =
            QQmlExpression(engine.rootContext(), map, "fromCoordinate(sample, false)").evaluate().toPointF();
        QVERIFY(snapshot.rect().contains(holePoint.toPoint()));
        QVERIFY(snapshot.rect().contains(fillPoint.toPoint()));
        QVERIFY(snapshot.pixelColor(holePoint.toPoint()) != snapshot.pixelColor(fillPoint.toPoint()));
        item.setIsCurrentItem(false);
        frames.clear();
        window->update();
        QVERIFY(waitForCondition(
            [&] {
                window->update();
                return frames.size() >= 3;
            },
            5000));
        const auto baseline = window->grabWindow();
        QVERIFY(!baseline.isNull());
        QCOMPARE(snapshot.pixelColor(holePoint.toPoint()), baseline.pixelColor(holePoint.toPoint()));
        QVERIFY(snapshot.pixelColor(fillPoint.toPoint()) != baseline.pixelColor(fillPoint.toPoint()));
        item.setIsCurrentItem(true);
        const auto directory = QDir::current().absoluteFilePath("build/v05-09-product-render");
        QVERIFY(QDir().mkpath(directory));
        QVERIFY(snapshot.save(directory + (theme == QGCPalette::Light ? "/holes-light.png" : "/holes-dark.png")));
        QVERIFY(baseline.save(directory + (theme == QGCPalette::Light ? "/baseline-light.png" : "/baseline-dark.png")));
        item.invalidatePlan();
        QVERIFY(waitForCondition([&] { return map->findChildren<QObject*>("marine_canonicalRun_0").isEmpty(); }, 3000));
        QVERIFY(item.resultStale());
        QVERIFY2(item.load(json, 1, error), qPrintable(error));
    }
}

void CoveragePresentationTest::_testToolbarBindingAndOverride()
{
    auto task = taskFor(problem(0), CoverageStrategySemantics::AutoPlannerId);
    auto* item = qobject_cast<CoverageInspectionComplexItem*>(missionController()->insertComplexMissionItem(
        CoverageInspectionComplexItem::canonicalName, QGeoCoordinate(38, 121), -1, true));
    QVERIFY(item);
    auto* context = planController()->findChild<MarinePlanContext*>(QString(), Qt::FindDirectChildrenOnly);
    QVERIFY(context);
    context->addTask(task);
    item->setTaskId(QString::fromStdString(task.id));
    QVERIFY(!missionController()->uploadAllowed());
    QQmlEngine engine;
    engine.addImportPath("qrc:/qml");
    engine.rootContext()->setContextProperty("actualPlan", planController());
    QQmlComponent harness(&engine);
    harness.setData(R"QML(
import QtQuick
import QGroundControl.PlanView
Item {
    QtObject { id: forwarding
        readonly property var missionController: actualPlan.missionController
        readonly property var geoFenceController: actualPlan.geoFenceController
        readonly property var rallyPointController: actualPlan.rallyPointController
        readonly property bool offline: false
        readonly property bool syncInProgress: actualPlan.syncInProgress
        readonly property bool dirtyForSave: actualPlan.dirtyForSave
        readonly property bool dirtyForUpload: actualPlan.dirtyForUpload
        readonly property bool containsItems: actualPlan.containsItems
        function upload() { actualPlan.sendToVehicle(); }
    }
    PlanToolBarIndicators { id: toolbar; objectName: "testedToolbar"; planMasterController: forwarding }
}
)QML",
                    QUrl("qrc:/qml/marine-toolbar-test.qml"));
    std::unique_ptr<QObject> root(harness.create());
    QVERIFY2(root != nullptr, qPrintable(harness.errorString()));
    auto* button = root->findChild<QObject*>("planToolbar_uploadButton");
    QVERIFY(button);
    QVERIFY(!button->property("enabled").toBool());
    QVERIFY(item->plan());
    QTRY_VERIFY(button->property("enabled").toBool());
    item->setSwathWidthM(6);
    QTRY_VERIFY(!button->property("enabled").toBool());
    button->setProperty("enabled", true);
    const auto reason = missionController()->uploadBlockingReason();
    QVERIFY(!reason.isEmpty());
    expectAppMessage(QRegularExpression(QRegularExpression::escape(reason)));
    auto* toolbar = root->findChild<QObject*>("testedToolbar");
    QVERIFY(toolbar);
    QVERIFY(QMetaObject::invokeMethod(toolbar, "_uploadClicked"));
    verifyExpectedLogMessage();
    QVERIFY(!missionController()->uploadAllowed());
    QVERIFY(!planController()->syncInProgress());
}

UT_REGISTER_TEST(CoveragePresentationTest, TestLabel::Unit, TestLabel::MissionManager)
