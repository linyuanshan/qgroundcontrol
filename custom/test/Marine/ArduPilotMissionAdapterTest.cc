#include "ArduPilotMissionAdapterTest.h"

#include <QtCore/QList>
#include <QtCore/QObject>

#include <cmath>

#include "ArduPilotMissionAdapter.h"
#include "AutoCoveragePlanner.h"
#include "CoverageTaskAdapter.h"
#include "MissionItem.h"

using namespace Marine;

namespace {

MarineTask validTask()
{
    MarineTask task;
    task.id = "mission-gate-fixture";
    task.region.coverageBoundary.vertices = {
        {38.1, 121.1, 0}, {38.1, 121.101, 0}, {38.101, 121.101, 0}, {38.101, 121.1, 0}};
    task.region.navigationBoundary = task.region.coverageBoundary;
    task.coverage.swathWidthM = 5;
    task.safety = {0, 0};
    task.planner.executionSafety.executionMarginM = 0;
    return task;
}

PlanningArtifact validArtifact()
{
    const auto task = validTask();
    CoveragePlanningProblem problem;
    std::optional<GeoReference> reference;
    CoveragePlanningError error{};
    if (!CoverageTaskAdapter::buildProblem(task, problem, reference, error)) {
        return {};
    }
    const auto solution = AutoCoveragePlanner{}.plan(problem);
    auto result = CoverageTaskAdapter::toPlanningResult(solution, *reference);
    const auto& source = *result.plannerSource;
    PlanningSemantics semantics;
    semantics.resolvedStrategy = QString::fromStdString(source.resolvedStrategy.strategyId);
    semantics.strategyVersion = QString::fromStdString(source.resolvedStrategy.semanticVersion);
    return {std::move(result), *PlanningInputIdentity::fromTask(task, semantics), false,
            PlanningResultContract::IntegratedV05};
}

}  // namespace

void ArduPilotMissionAdapterTest::_testAppendWaypoints()
{
    QObject parent;
    QList<MissionItem*> items;
    int sequenceNumber = 7;
    QString errorString = QStringLiteral("stale error");
    const auto artifact = validArtifact();
    const auto& result = artifact.result;

    QVERIFY2(
        ArduPilotMissionAdapter::appendWaypoints(artifact, validTask(), items, &parent, sequenceNumber, errorString),
        qPrintable(errorString));

    QCOMPARE(errorString, QString());
    QCOMPARE(items.size(), static_cast<qsizetype>(result.path.size()));
    QCOMPARE(sequenceNumber, 7 + static_cast<int>(result.path.size()));
    QCOMPARE(result.legRoles.size(), result.path.size() - 1);
    for (int index = 0; index < items.size(); ++index) {
        const MissionItem* item = items.at(index);
        QCOMPARE(item->sequenceNumber(), 7 + index);
        QCOMPARE(item->command(), MAV_CMD_NAV_WAYPOINT);
        QCOMPARE(item->frame(), MAV_FRAME_GLOBAL_RELATIVE_ALT);
        QCOMPARE(item->param1(), 0.0);
        QCOMPARE(item->param2(), 0.0);
        QCOMPARE(item->param3(), 0.0);
        QVERIFY(std::isnan(item->param4()));
        QCOMPARE(item->param5(), result.path.at(static_cast<std::size_t>(index)).latitudeDeg);
        QCOMPARE(item->param6(), result.path.at(static_cast<std::size_t>(index)).longitudeDeg);
        QCOMPARE(item->param7(), 0.0);
        QVERIFY(item->autoContinue());
        QVERIFY(!item->isCurrentItem());
        QCOMPARE(item->parent(), &parent);
    }
}

void ArduPilotMissionAdapterTest::_testLegacyPathWithoutRoles()
{
    QObject parent;
    QList<MissionItem*> items;
    int sequenceNumber = 3;
    QString errorString;
    auto artifact = validArtifact();
    auto& result = artifact.result;
    result.legRoles.clear();

    QVERIFY(
        !ArduPilotMissionAdapter::appendWaypoints(artifact, validTask(), items, &parent, sequenceNumber, errorString));
    QVERIFY(items.isEmpty());
    QCOMPARE(sequenceNumber, 3);
}

void ArduPilotMissionAdapterTest::_testRejectsUnsuccessfulResult()
{
    QObject parent;
    QList<MissionItem*> items;
    items.append(new MissionItem(&parent));
    auto artifact = validArtifact();
    auto& result = artifact.result;
    result.status = PlanningStatus::Failed;
    int sequenceNumber = 4;
    QString errorString;

    QVERIFY(
        !ArduPilotMissionAdapter::appendWaypoints(artifact, validTask(), items, &parent, sequenceNumber, errorString));
    QCOMPARE(items.size(), 1);
    QCOMPARE(sequenceNumber, 4);
    QVERIFY(!errorString.isEmpty());
}

void ArduPilotMissionAdapterTest::_testRejectsInvalidPath()
{
    QObject parent;
    QList<MissionItem*> items;
    int sequenceNumber = 4;
    QString errorString;
    auto artifact = validArtifact();
    auto& result = artifact.result;
    result.path.clear();

    QVERIFY(
        !ArduPilotMissionAdapter::appendWaypoints(artifact, validTask(), items, &parent, sequenceNumber, errorString));
    QVERIFY(items.isEmpty());
    QCOMPARE(sequenceNumber, 4);
    QVERIFY(!errorString.isEmpty());

    artifact = validArtifact();
    result.path.at(1).latitudeDeg = 91.0;
    errorString.clear();
    QVERIFY(
        !ArduPilotMissionAdapter::appendWaypoints(artifact, validTask(), items, &parent, sequenceNumber, errorString));
    QVERIFY(items.isEmpty());
    QCOMPARE(sequenceNumber, 4);
    QVERIFY(!errorString.isEmpty());
}

UT_REGISTER_TEST(ArduPilotMissionAdapterTest, TestLabel::Unit, TestLabel::MissionManager)
