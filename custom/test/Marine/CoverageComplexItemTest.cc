#include "CoverageComplexItemTest.h"

#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtTest/QSignalSpy>

#include <cmath>
#include <limits>
#include <memory>

#include "BoustrophedonCoveragePlanner.h"
#include "CoverageInspectionComplexItem.h"
#include "CoverageProblemValidator.h"
#include "LawnmowerCoveragePlanner.h"
#include "MarinePlanContext.h"
#include "MissionItem.h"
#include "MockCoveragePlanner.h"
#include "QGCMapPolygon.h"

using namespace Marine;

namespace {

MarineTask validTask()
{
    MarineTask task;
    task.planner.plannerId = "marine.coverage.mock";
    task.coverage.swathWidthM = 5.0;
    task.region.outerBoundary.vertices = {
        {47.3977, 8.5455, 0.0},
        {47.3977, 8.5465, 0.0},
        {47.3987, 8.5465, 0.0},
        {47.3987, 8.5455, 0.0},
    };
    return task;
}

GeoPolygon noGoRectangle()
{
    return GeoPolygon{.vertices = {
                          {47.39805, 8.54585, 0.0},
                          {47.39805, 8.54615, 0.0},
                          {47.39835, 8.54615, 0.0},
                          {47.39835, 8.54585, 0.0},
                      }};
}

QJsonObject legacyV1Artifact(const QString& taskId)
{
    return {
        {QStringLiteral("version"), 1},
        {QStringLiteral("type"), QStringLiteral("ComplexItem")},
        {QStringLiteral("complexItemType"), QStringLiteral("coverageInspection")},
        {QStringLiteral("taskId"), taskId},
        {QStringLiteral("planningStatus"), QStringLiteral("success")},
        {QStringLiteral("planningMessage"), QStringLiteral("Legacy lawnmower artifact")},
        {QStringLiteral("generatedPath"),
         QJsonArray{
             QJsonArray{47.3978, 8.5456, 0.0},
             QJsonArray{47.3985, 8.5456, 0.0},
             QJsonArray{47.3985, 8.5459, 0.0},
             QJsonArray{47.3978, 8.5459, 0.0},
         }},
        {QStringLiteral("pathLengthM"), 180.0},
        {QStringLiteral("selectedSweepAngleDeg"), 0.0},
        {QStringLiteral("turnCount"), 2},
    };
}

QJsonObject roleRunArtifact(const QString& taskId, const std::vector<PathLegRole>& roles)
{
    QJsonArray path;
    for (std::size_t pointIndex = 0; pointIndex <= roles.size(); ++pointIndex) {
        path.append(QJsonArray{47.3978 + (static_cast<double>(pointIndex) * 0.00001), 8.5456, 0.0});
    }

    QJsonArray roleNames;
    int coverageLegCount = 0;
    for (const PathLegRole role : roles) {
        if (role == PathLegRole::Coverage) {
            roleNames.append(QStringLiteral("coverage"));
            ++coverageLegCount;
        } else {
            roleNames.append(QStringLiteral("transit"));
        }
    }
    const int transitLegCount = static_cast<int>(roles.size()) - coverageLegCount;

    return {
        {QStringLiteral("version"), 2},
        {QStringLiteral("type"), QStringLiteral("ComplexItem")},
        {QStringLiteral("complexItemType"), QStringLiteral("coverageInspection")},
        {QStringLiteral("taskId"), taskId},
        {QStringLiteral("planningStatus"), QStringLiteral("success")},
        {QStringLiteral("planningMessage"), QStringLiteral("Role run fixture")},
        {QStringLiteral("generatedPath"), path},
        {QStringLiteral("legRoles"), roleNames},
        {QStringLiteral("coverageLengthM"), static_cast<double>(coverageLegCount)},
        {QStringLiteral("transitLengthM"), static_cast<double>(transitLegCount)},
        {QStringLiteral("pathLengthM"), static_cast<double>(roles.size())},
        {QStringLiteral("selectedSweepAngleDeg"), 0.0},
        {QStringLiteral("cellCount"), 1},
        {QStringLiteral("turnCount"), 0},
    };
}

void verifyRoleRunsReconstructCanonicalPath(const CoverageInspectionComplexItem& item, int& coverageRunCount,
                                            int& transitRunCount)
{
    const QVariantList runs = item.generatedPathRoleRuns();
    QVERIFY(!runs.isEmpty());

    QVariantList reconstructedPath;
    std::vector<PathLegRole> reconstructedRoles;
    coverageRunCount = 0;
    transitRunCount = 0;

    for (const QVariant& runValue : runs) {
        const QVariantMap run = runValue.toMap();
        const QString roleName = run.value(QStringLiteral("role")).toString();
        const QVariantList runPath = run.value(QStringLiteral("path")).toList();
        QVERIFY(runPath.size() >= 2);

        PathLegRole role = PathLegRole::Coverage;
        if (roleName == QStringLiteral("coverage")) {
            ++coverageRunCount;
        } else {
            QCOMPARE(roleName, QStringLiteral("transit"));
            role = PathLegRole::Transit;
            ++transitRunCount;
        }

        if (reconstructedPath.isEmpty()) {
            reconstructedPath = runPath;
        } else {
            QCOMPARE(reconstructedPath.last(), runPath.first());
            for (qsizetype pointIndex = 1; pointIndex < runPath.size(); ++pointIndex) {
                reconstructedPath.append(runPath[pointIndex]);
            }
        }
        reconstructedRoles.insert(reconstructedRoles.end(), static_cast<std::size_t>(runPath.size() - 1), role);
    }

    QCOMPARE(reconstructedPath, item.generatedPath());
    QCOMPARE(reconstructedRoles, item.planningResult().legRoles);
}

void verifyMissionItems(const QList<MissionItem*>& missionItems, const PlanningResult& result, int firstSequence)
{
    QCOMPARE(missionItems.size(), static_cast<qsizetype>(result.path.size()));
    for (int index = 0; index < missionItems.size(); ++index) {
        const MissionItem* missionItem = missionItems.at(index);
        const GeoPoint& point = result.path.at(static_cast<std::size_t>(index));
        QCOMPARE(missionItem->sequenceNumber(), firstSequence + index);
        QCOMPARE(missionItem->command(), MAV_CMD_NAV_WAYPOINT);
        QCOMPARE(missionItem->param5(), point.latitudeDeg);
        QCOMPARE(missionItem->param6(), point.longitudeDeg);
        QCOMPARE(missionItem->param7(), 0.0);
    }
}

}  // namespace

void CoverageComplexItemTest::init()
{
    OfflineMissionTest::init();
    _marineContext = new MarinePlanContext(planController());
    QVERIFY(_marineContext->plannerRegistry().registerPlanner(std::make_shared<MockCoveragePlanner>()));
    QVERIFY(_marineContext->plannerRegistry().registerPlanner(std::make_shared<LawnmowerCoveragePlanner>()));
    QVERIFY(_marineContext->plannerRegistry().registerPlanner(std::make_shared<BoustrophedonCoveragePlanner>()));
    _item = new CoverageInspectionComplexItem(planController(), false, _marineContext);
}

void CoverageComplexItemTest::cleanup()
{
    _item = nullptr;
    _marineContext = nullptr;
    OfflineMissionTest::cleanup();
}

void CoverageComplexItemTest::_testDefaults()
{
    QCOMPARE(_item->taskId(), QString());
    QCOMPARE(_item->planningState(), CoverageInspectionComplexItem::Unplanned);
    QVERIFY(_item->generatedPath().isEmpty());
    QCOMPARE(_item->planningResult().status, PlanningStatus::Failed);
    QCOMPARE(_item->coverageLengthM(), 0.0);
    QCOMPARE(_item->transitLengthM(), 0.0);
    QCOMPARE(_item->cellCount(), 0);
    QVERIFY(!_item->dirty());
    QVERIFY(!_item->specifiesCoordinate());
    QCOMPARE(_item->readyForSaveState(), VisualMissionItem::NotReadyForSaveData);
    QCOMPARE(_item->patternName(), QStringLiteral("Coverage Inspection"));
    QCOMPARE(_item->commandName(), QStringLiteral("Coverage Inspection"));
    QCOMPARE(_item->sequenceNumber(), 0);
    QCOMPARE(_item->lastSequenceNumber(), 0);
}

void CoverageComplexItemTest::_testPlanning()
{
    const MarineTask task = validTask();
    _marineContext->addTask(task);
    QSignalSpy stateSpy(_item, &CoverageInspectionComplexItem::planningStateChanged);
    QSignalSpy pathSpy(_item, &CoverageInspectionComplexItem::generatedPathChanged);
    QSignalSpy lastSequenceSpy(_item, &CoverageInspectionComplexItem::lastSequenceNumberChanged);

    _item->setTaskId(QString::fromStdString(task.id));
    _item->setDirty(false);
    QVERIFY(_item->plan());

    QCOMPARE(_item->planningState(), CoverageInspectionComplexItem::Planned);
    QCOMPARE(_item->planningResult().status, PlanningStatus::Success);
    QCOMPARE(_item->generatedPath().size(), 3);
    QCOMPARE(_item->generatedPath().first().value<QGeoCoordinate>(), _item->entryCoordinate());
    QCOMPARE(_item->generatedPath().last().value<QGeoCoordinate>(), _item->exitCoordinate());
    QVERIFY(_item->complexDistance() > 0.0);
    QVERIFY(_item->specifiesCoordinate());
    QCOMPARE(_item->readyForSaveState(), VisualMissionItem::ReadyForSave);
    QCOMPARE(_item->lastSequenceNumber(), 2);
    QVERIFY(_item->dirty());
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(pathSpy.count(), 1);
    QCOMPARE(lastSequenceSpy.count(), 1);

    QList<MissionItem*> items;
    _item->appendMissionItems(items, this);
    QCOMPARE(items.size(), 3);
    for (int index = 0; index < items.size(); ++index) {
        QCOMPARE(items.at(index)->sequenceNumber(), index);
        QCOMPARE(items.at(index)->command(), MAV_CMD_NAV_WAYPOINT);
    }
}

void CoverageComplexItemTest::_testLawnmowerPlanning()
{
    MarineTask task = validTask();
    task.planner.plannerId = "marine.coverage.lawnmower";
    task.coverage.swathWidthM = 30.0;
    task.coverage.safetyMarginM = 1.0;
    task.coverage.sweepAngleMode = SweepAngleMode::Manual;
    task.coverage.sweepAngleDeg = 0.0;
    _marineContext->addTask(task);
    _item->setTaskId(QString::fromStdString(task.id));

    QVERIFY2(_item->plan(), _item->planningResult().message.c_str());

    const PlanningResult& result = _item->planningResult();
    QCOMPARE(_item->planningState(), CoverageInspectionComplexItem::Planned);
    QCOMPARE(result.status, PlanningStatus::Success);
    QVERIFY(result.path.size() >= 4);
    QCOMPARE(result.path.size() % 2, std::size_t{0});
    QCOMPARE(result.legRoles.size(), result.path.size() - 1);
    for (std::size_t legIndex = 0; legIndex < result.legRoles.size(); ++legIndex) {
        QCOMPARE(result.legRoles[legIndex], (legIndex % 2) == 0 ? PathLegRole::Coverage : PathLegRole::Transit);
    }
    QCOMPARE(result.selectedSweepAngleDeg, 0.0);
    QCOMPARE(result.turnCount, static_cast<int>((result.path.size() / 2) - 1));
    QCOMPARE(_item->plannerId(), QStringLiteral("marine.coverage.lawnmower"));
    QCOMPARE(_item->selectedSweepAngleDeg(), result.selectedSweepAngleDeg);
    QCOMPARE(_item->turnCount(), result.turnCount);
    QCOMPARE(_item->planningMessage(), QString::fromStdString(result.message));
    QCOMPARE(_item->generatedPath().size(), static_cast<qsizetype>(result.path.size()));
    QCOMPARE(_item->complexDistance(), result.pathLengthM);
    QVERIFY(result.message.find("Manual") != std::string::npos);
}

void CoverageComplexItemTest::_testLawnmowerUiNoGoRejection()
{
    MarineTask task = validTask();
    task.planner.plannerId = "marine.coverage.lawnmower";
    task.coverage.swathWidthM = 20.0;
    _marineContext->addTask(task);
    _item->setTaskId(QString::fromStdString(task.id));

    QVERIFY(_item->addNoGoRegion());
    QGCMapPolygon* noGoPolygon = _item->noGoPolygons()->value<QGCMapPolygon*>(0);
    QVERIFY(noGoPolygon != nullptr);
    for (const GeoPoint& point : noGoRectangle().vertices) {
        noGoPolygon->appendVertex(QGeoCoordinate(point.latitudeDeg, point.longitudeDeg));
    }
    QTRY_VERIFY(_item->noGoRegionsReady());
    QTRY_COMPARE(_marineContext->task(task.id)->region.noGoRegions.size(), std::size_t{1});

    QVERIFY(!_item->plan());
    QCOMPARE(_item->plannerId(), QStringLiteral("marine.coverage.lawnmower"));
    QCOMPARE(_item->planningResult().message,
             CoverageProblemValidator::messageForError(CoveragePlanningError::UnsupportedNoGoRegion));
}

void CoverageComplexItemTest::_testBoustrophedonNoGoPlanning()
{
    MarineTask task = validTask();
    task.planner.plannerId = "marine.coverage.bcd";
    task.coverage.swathWidthM = 20.0;
    task.coverage.safetyMarginM = 0.0;
    task.coverage.sweepAngleMode = SweepAngleMode::Manual;
    task.coverage.sweepAngleDeg = 90.0;
    _marineContext->addTask(task);
    _item->setTaskId(QString::fromStdString(task.id));

    QVERIFY(_item->addNoGoRegion());
    QGCMapPolygon* noGoPolygon = _item->noGoPolygons()->value<QGCMapPolygon*>(0);
    QVERIFY(noGoPolygon != nullptr);
    for (const GeoPoint& point : noGoRectangle().vertices) {
        noGoPolygon->appendVertex(QGeoCoordinate(point.latitudeDeg, point.longitudeDeg));
    }
    QTRY_VERIFY(_item->noGoRegionsReady());
    QTRY_COMPARE(_marineContext->task(task.id)->region.noGoRegions.size(), std::size_t{1});

    QVERIFY2(_item->plan(), _item->planningResult().message.c_str());

    const PlanningResult& result = _item->planningResult();
    QCOMPARE(_item->planningState(), CoverageInspectionComplexItem::Planned);
    QCOMPARE(result.status, PlanningStatus::Success);
    QVERIFY(!result.path.empty());
    QCOMPARE(result.legRoles.size(), result.path.size() - 1);
    QVERIFY(std::isfinite(result.pathLengthM));
    QCOMPARE(result.pathLengthM, result.coverageLengthM + result.transitLengthM);
    QVERIFY(result.cellCount >= 1);

    int coverageRunCount = 0;
    int transitRunCount = 0;
    verifyRoleRunsReconstructCanonicalPath(*_item, coverageRunCount, transitRunCount);
    QVERIFY(coverageRunCount >= 1);
    QVERIFY(transitRunCount >= 1);

    QList<MissionItem*> missionItems;
    _item->appendMissionItems(missionItems, this);
    QCOMPARE(missionItems.size(), static_cast<qsizetype>(result.path.size()));
    for (const MissionItem* missionItem : missionItems) {
        QCOMPARE(missionItem->command(), MAV_CMD_NAV_WAYPOINT);
    }
}

void CoverageComplexItemTest::_testGeneratedPathRoleRuns()
{
    QVERIFY(_item->generatedPathRoleRuns().isEmpty());

    const MarineTask task = validTask();
    _marineContext->addTask(task);
    const QString taskId = QString::fromStdString(task.id);
    QString errorString;

    const std::vector<PathLegRole> alternatingRoles = {
        PathLegRole::Coverage, PathLegRole::Coverage, PathLegRole::Transit, PathLegRole::Transit, PathLegRole::Coverage,
    };
    QVERIFY2(_item->load(roleRunArtifact(taskId, alternatingRoles), 0, errorString), qPrintable(errorString));
    const QVariantList alternatingRuns = _item->generatedPathRoleRuns();
    QCOMPARE(alternatingRuns.size(), 3);
    QCOMPARE(alternatingRuns[0].toMap().value(QStringLiteral("role")).toString(), QStringLiteral("coverage"));
    QCOMPARE(alternatingRuns[0].toMap().value(QStringLiteral("path")).toList().size(), 3);
    QCOMPARE(alternatingRuns[1].toMap().value(QStringLiteral("role")).toString(), QStringLiteral("transit"));
    QCOMPARE(alternatingRuns[1].toMap().value(QStringLiteral("path")).toList().size(), 3);
    QCOMPARE(alternatingRuns[2].toMap().value(QStringLiteral("role")).toString(), QStringLiteral("coverage"));
    QCOMPARE(alternatingRuns[2].toMap().value(QStringLiteral("path")).toList().size(), 2);
    QCOMPARE(alternatingRuns[0].toMap().value(QStringLiteral("path")).toList().last(),
             alternatingRuns[1].toMap().value(QStringLiteral("path")).toList().first());
    QCOMPARE(alternatingRuns[1].toMap().value(QStringLiteral("path")).toList().last(),
             alternatingRuns[2].toMap().value(QStringLiteral("path")).toList().first());
    int coverageRunCount = 0;
    int transitRunCount = 0;
    verifyRoleRunsReconstructCanonicalPath(*_item, coverageRunCount, transitRunCount);
    QCOMPARE(coverageRunCount, 2);
    QCOMPARE(transitRunCount, 1);

    errorString.clear();
    QVERIFY2(_item->load(roleRunArtifact(taskId, std::vector<PathLegRole>(4, PathLegRole::Coverage)), 0, errorString),
             qPrintable(errorString));
    QCOMPARE(_item->generatedPathRoleRuns().size(), 1);
    QCOMPARE(_item->generatedPathRoleRuns().first().toMap().value(QStringLiteral("role")).toString(),
             QStringLiteral("coverage"));

    errorString.clear();
    QVERIFY2(_item->load(roleRunArtifact(taskId, std::vector<PathLegRole>(4, PathLegRole::Transit)), 0, errorString),
             qPrintable(errorString));
    QCOMPARE(_item->generatedPathRoleRuns().size(), 1);
    QCOMPARE(_item->generatedPathRoleRuns().first().toMap().value(QStringLiteral("role")).toString(),
             QStringLiteral("transit"));
}

void CoverageComplexItemTest::_testPlanningFailures()
{
    _item->setTaskId(QStringLiteral("missing-task"));
    QVERIFY(!_item->plan());
    QCOMPARE(_item->planningState(), CoverageInspectionComplexItem::Unplanned);
    QVERIFY(_item->generatedPath().isEmpty());
    QVERIFY(!_item->planningResult().message.empty());

    MarineTask task = validTask();
    task.planner.plannerId = "missing-planner";
    _marineContext->addTask(task);
    _item->setTaskId(QString::fromStdString(task.id));
    QVERIFY(!_item->plan());
    QCOMPARE(_item->planningResult().status, PlanningStatus::Failed);
    QVERIFY(!_item->planningResult().message.empty());

    task.region.outerBoundary.vertices.clear();
    task.planner.plannerId = "marine.coverage.mock";
    _marineContext->addTask(task);
    QVERIFY(!_item->plan());
    QCOMPARE(_item->planningResult().status, PlanningStatus::InvalidInput);
}

void CoverageComplexItemTest::_testInvalidation()
{
    const MarineTask firstTask = validTask();
    MarineTask secondTask = validTask();
    _marineContext->addTask(firstTask);
    _marineContext->addTask(secondTask);
    _item->setTaskId(QString::fromStdString(firstTask.id));
    QVERIFY(_item->plan());

    _item->invalidatePlan();
    QCOMPARE(_item->planningState(), CoverageInspectionComplexItem::Unplanned);
    QVERIFY(_item->generatedPath().isEmpty());
    QCOMPARE(_item->complexDistance(), 0.0);

    QVERIFY(_item->plan());
    MarineTask updatedTask = firstTask;
    updatedTask.coverage.swathWidthM = 12.0;
    _marineContext->addTask(updatedTask);
    QCOMPARE(_item->planningState(), CoverageInspectionComplexItem::Unplanned);
    QVERIFY(_item->generatedPath().isEmpty());
    QList<MissionItem*> invalidatedItems;
    _item->appendMissionItems(invalidatedItems, this);
    QVERIFY(invalidatedItems.isEmpty());

    QVERIFY(_item->plan());
    _item->setTaskId(QString::fromStdString(secondTask.id));
    QCOMPARE(_item->planningState(), CoverageInspectionComplexItem::Unplanned);
    QVERIFY(_item->generatedPath().isEmpty());

    QVERIFY(_item->plan());
    _marineContext->removeTask(secondTask.id);
    QCOMPARE(_item->planningState(), CoverageInspectionComplexItem::Unplanned);
    QVERIFY(_item->generatedPath().isEmpty());

    _marineContext->addTask(secondTask);
    QVERIFY(_item->plan());
    _marineContext->clearTasks();
    QCOMPARE(_item->planningState(), CoverageInspectionComplexItem::Unplanned);
    QVERIFY(_item->generatedPath().isEmpty());
}

void CoverageComplexItemTest::_testQmlRegistration()
{
    const QString editorUrl = QStringLiteral("qrc:/qml/Marine/Plan/CoverageInspectionEditor.qml");
    const QString mapVisualUrl = QStringLiteral("qrc:/qml/Marine/Plan/CoverageInspectionMapVisual.qml");

    QCOMPARE(_item->property("editorQml").toString(), editorUrl);
    QCOMPARE(_item->mapVisualQML(), mapVisualUrl);
    QVERIFY(QFile::exists(QStringLiteral(":/qml/Marine/Plan/CoverageInspectionEditor.qml")));
    QVERIFY(QFile::exists(QStringLiteral(":/qml/Marine/Plan/CoverageInspectionMapVisual.qml")));

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qml"));
    QQmlComponent editorComponent(&engine, QUrl(editorUrl));
    QQmlComponent mapVisualComponent(&engine, QUrl(mapVisualUrl));
    QVERIFY2(editorComponent.isReady(), qPrintable(editorComponent.errorString()));
    QVERIFY2(mapVisualComponent.isReady(), qPrintable(mapVisualComponent.errorString()));

    QFile editorFile(QStringLiteral(":/qml/Marine/Plan/CoverageInspectionEditor.qml"));
    QVERIFY(editorFile.open(QIODevice::ReadOnly));
    const QByteArray editorSource = editorFile.readAll();
    QVERIFY(editorSource.contains("automaticSweepAngle"));
    QVERIFY(editorSource.contains("selectedSweepAngleDeg"));
    QFile mapVisualFile(QStringLiteral(":/qml/Marine/Plan/CoverageInspectionMapVisual.qml"));
    QVERIFY(mapVisualFile.open(QIODevice::ReadOnly));
    const QByteArray mapVisualSource = mapVisualFile.readAll();
    QVERIFY(mapVisualSource.contains("QGCMapPolygonVisuals"));
    QVERIFY(mapVisualSource.contains("workRegionPolygon"));
    QVERIFY(mapVisualSource.contains("noGoPolygons"));
    QVERIFY(mapVisualSource.contains("generatedPathRoleRuns"));
    QVERIFY(mapVisualSource.contains("qgcPal.mapMissionTrajectory"));
    QVERIFY(mapVisualSource.contains("qgcPal.colorGrey"));
    QVERIFY(mapVisualSource.contains("neutralPathComponent"));
    QVERIFY(!editorSource.contains("No-Go regions are read-only"));
    QVERIFY(editorSource.contains("Add No-Go Region"));
    QVERIFY(editorSource.contains("noGoRegionsReady"));
}

void CoverageComplexItemTest::_testQmlTaskProperties()
{
    MarineTask task = validTask();
    task.name = "Harbor inspection";
    task.coverage.swathWidthM = 8.5;
    task.coverage.safetyMarginM = 2.25;
    task.sensors.cameraEnabled = true;
    task.sensors.cameraRecord = false;
    task.sensors.sonarEnabled = false;
    task.sensors.sonarRecord = true;
    task.region.noGoRegions = {GeoPolygon{.vertices = {
                                              {.latitudeDeg = 47.3980, .longitudeDeg = 8.5458, .altitudeM = 0.0},
                                              {.latitudeDeg = 47.3980, .longitudeDeg = 8.5460, .altitudeM = 0.0},
                                              {.latitudeDeg = 47.3982, .longitudeDeg = 8.5460, .altitudeM = 0.0},
                                          }}};
    _marineContext->addTask(task);
    _item->setTaskId(QString::fromStdString(task.id));

    QCOMPARE(_item->taskName(), QStringLiteral("Harbor inspection"));
    QCOMPARE(_item->swathWidthM(), 8.5);
    QCOMPARE(_item->safetyMarginM(), 2.25);
    QVERIFY(_item->cameraEnabled());
    QVERIFY(!_item->cameraRecord());
    QVERIFY(!_item->sonarEnabled());
    QVERIFY(_item->sonarRecord());
    QCOMPARE(_item->outerBoundary().size(), 4);
    const QVariantList noGoRegions = _item->noGoRegions();
    QCOMPARE(noGoRegions.size(), 1);
    QCOMPARE(noGoRegions.constFirst().toList().size(), 3);

    QVERIFY(!_item->plan());
    QCOMPARE(_item->planningResult().status, PlanningStatus::Failed);
    QVERIFY(_item->planningResult().message.find("selected coverage planner") != std::string::npos);
    QSignalSpy taskDataSpy(_item, &CoverageInspectionComplexItem::taskDataChanged);
    _item->setTaskName(QStringLiteral("Updated inspection"));
    _item->setSwathWidthM(11.0);
    _item->setSafetyMarginM(3.0);
    _item->setCameraEnabled(false);
    _item->setCameraRecord(true);
    _item->setSonarEnabled(true);
    _item->setSonarRecord(false);

    const MarineTask* updatedTask = _marineContext->task(task.id);
    QVERIFY(updatedTask != nullptr);
    QCOMPARE(QString::fromStdString(updatedTask->name), QStringLiteral("Updated inspection"));
    QCOMPARE(updatedTask->coverage.swathWidthM, 11.0);
    QCOMPARE(updatedTask->coverage.safetyMarginM, 3.0);
    QVERIFY(!updatedTask->sensors.cameraEnabled);
    QVERIFY(updatedTask->sensors.cameraRecord);
    QVERIFY(updatedTask->sensors.sonarEnabled);
    QVERIFY(!updatedTask->sensors.sonarRecord);
    QCOMPARE(_item->planningState(), CoverageInspectionComplexItem::Unplanned);
    QCOMPARE(taskDataSpy.count(), 7);
}

void CoverageComplexItemTest::_testWorkRegionEditing()
{
    MarineTask task = validTask();
    task.planner.plannerId = "marine.coverage.lawnmower";
    task.coverage.swathWidthM = 30.0;
    task.coverage.safetyMarginM = 1.0;
    _marineContext->addTask(task);
    _item->setTaskId(QString::fromStdString(task.id));

    QGCMapPolygon* polygon = _item->workRegionPolygon();
    QVERIFY(polygon != nullptr);
    QCOMPARE(polygon->count(), 4);
    QCOMPARE(polygon->vertexCoordinate(0).latitude(), task.region.outerBoundary.vertices[0].latitudeDeg);
    QVERIFY2(_item->plan(), _item->planningResult().message.c_str());

    QSignalSpy taskDataSpy(_item, &CoverageInspectionComplexItem::taskDataChanged);
    const QGeoCoordinate editedCoordinate(47.3978, 8.5456);
    polygon->adjustVertex(0, editedCoordinate);

    QTRY_VERIFY(taskDataSpy.count() >= 1);
    const MarineTask* editedTask = _marineContext->task(task.id);
    QVERIFY(editedTask != nullptr);
    QCOMPARE(editedTask->region.outerBoundary.vertices[0].latitudeDeg, editedCoordinate.latitude());
    QCOMPARE(editedTask->region.outerBoundary.vertices[0].longitudeDeg, editedCoordinate.longitude());
    QCOMPARE(editedTask->region.outerBoundary.vertices[0].altitudeM, 0.0);
    QCOMPARE(_item->planningState(), CoverageInspectionComplexItem::Unplanned);

    MarineTask externallyUpdatedTask = *editedTask;
    externallyUpdatedTask.region.outerBoundary.vertices.resize(3);
    QVERIFY(_marineContext->updateTask(externallyUpdatedTask));
    QCOMPARE(polygon->count(), 3);
    QCOMPARE(_item->outerBoundary().size(), 3);
}

void CoverageComplexItemTest::_testNoGoRegionEditing()
{
    MarineTask task = validTask();
    task.planner.plannerId = "marine.coverage.bcd";
    task.coverage.swathWidthM = 20.0;
    task.coverage.sweepAngleMode = SweepAngleMode::Manual;
    task.coverage.sweepAngleDeg = 90.0;
    task.region.noGoRegions = {noGoRectangle()};
    _marineContext->addTask(task);
    _item->setTaskId(QString::fromStdString(task.id));

    QCOMPARE(_item->maximumNoGoRegionCount(), 2);
    QCOMPARE(_item->noGoPolygons()->count(), 1);
    QVERIFY(_item->noGoRegionsReady());
    QGCMapPolygon* firstPolygon = _item->noGoPolygons()->value<QGCMapPolygon*>(0);
    QVERIFY(firstPolygon != nullptr);
    QCOMPARE(firstPolygon->count(), 4);
    QCOMPARE(firstPolygon->vertexCoordinate(0).latitude(), noGoRectangle().vertices[0].latitudeDeg);
    QVERIFY2(_item->plan(), _item->planningResult().message.c_str());

    QVERIFY(_item->addNoGoRegion());
    QCOMPARE(_item->noGoPolygons()->count(), 2);
    QGCMapPolygon* pendingPolygon = _item->noGoPolygons()->value<QGCMapPolygon*>(1);
    QVERIFY(pendingPolygon != nullptr);
    QCOMPARE(pendingPolygon->count(), 0);
    QVERIFY(pendingPolygon->interactive());
    QVERIFY(!firstPolygon->interactive());
    QVERIFY(!_item->noGoRegionsReady());
    QCOMPARE(_marineContext->task(task.id)->region.noGoRegions.size(), std::size_t{1});
    QCOMPARE(_item->planningState(), CoverageInspectionComplexItem::Unplanned);
    QVERIFY(!_item->plan());
    QCOMPARE(_item->planningResult().status, PlanningStatus::InvalidInput);
    QVERIFY(!_item->addNoGoRegion());
    QCOMPARE(_item->noGoPolygons()->count(), 2);

    _item->setNoGoRegionInteractive(0, true);
    QVERIFY(firstPolygon->interactive());
    QVERIFY(!pendingPolygon->interactive());
    _item->setNoGoRegionInteractive(1, true);
    QVERIFY(!firstPolygon->interactive());
    QVERIFY(pendingPolygon->interactive());
    _item->setNoGoRegionInteractive(1, false);
    QVERIFY(!_item->noGoRegionEditing());
    _item->setNoGoRegionInteractive(1, true);

    const QList<QGeoCoordinate> secondCoordinates = {
        {47.39845, 8.54620},
        {47.39845, 8.54635},
        {47.39860, 8.54635},
        {47.39860, 8.54620},
    };
    for (const QGeoCoordinate& coordinate : secondCoordinates) {
        pendingPolygon->appendVertex(coordinate);
    }
    QTRY_VERIFY(_item->noGoRegionsReady());
    QTRY_COMPARE(_marineContext->task(task.id)->region.noGoRegions.size(), std::size_t{2});
    QCOMPARE(_marineContext->task(task.id)->region.noGoRegions[1].vertices[0].latitudeDeg,
             secondCoordinates[0].latitude());
    QVERIFY2(_item->plan(), _item->planningResult().message.c_str());

    const QGeoCoordinate adjustedCoordinate(47.39847, 8.54622);
    pendingPolygon->adjustVertex(0, adjustedCoordinate);
    QTRY_COMPARE(_marineContext->task(task.id)->region.noGoRegions[1].vertices[0].latitudeDeg,
                 adjustedCoordinate.latitude());
    QCOMPARE(_item->planningState(), CoverageInspectionComplexItem::Unplanned);

    QVERIFY(_item->deleteNoGoRegion(1));
    QCOMPARE(_item->noGoPolygons()->count(), 1);
    QCOMPARE(_marineContext->task(task.id)->region.noGoRegions.size(), std::size_t{1});

    MarineTask externalTask = *_marineContext->task(task.id);
    externalTask.region.noGoRegions = {GeoPolygon{.vertices = {
                                                      {47.39840, 8.54570, 0.0},
                                                      {47.39845, 8.54590, 0.0},
                                                      {47.39860, 8.54575, 0.0},
                                                  }}};
    QVERIFY(_marineContext->updateTask(externalTask));
    QCOMPARE(_item->noGoPolygons()->count(), 1);
    QGCMapPolygon* rebuiltPolygon = _item->noGoPolygons()->value<QGCMapPolygon*>(0);
    QVERIFY(rebuiltPolygon != nullptr);
    QCOMPARE(rebuiltPolygon->count(), 3);
    QCOMPARE(rebuiltPolygon->vertexCoordinate(0).latitude(), 47.39840);

    QVERIFY(_item->addNoGoRegion());
    QCOMPARE(_marineContext->task(task.id)->region.noGoRegions.size(), std::size_t{1});
    QVERIFY(_item->deleteNoGoRegion(1));
    QVERIFY(_item->noGoRegionsReady());
}

void CoverageComplexItemTest::_testSweepAngleProperties()
{
    MarineTask task = validTask();
    task.planner.plannerId = "marine.coverage.lawnmower";
    _marineContext->addTask(task);
    _item->setTaskId(QString::fromStdString(task.id));

    QVERIFY(_item->automaticSweepAngle());
    QCOMPARE(_item->sweepAngleDeg(), 0.0);
    QCOMPARE(_item->plannerId(), QStringLiteral("marine.coverage.lawnmower"));

    _item->setAutomaticSweepAngle(false);
    _item->setSweepAngleDeg(37.5);

    const MarineTask* updatedTask = _marineContext->task(task.id);
    QVERIFY(updatedTask != nullptr);
    QCOMPARE(updatedTask->coverage.sweepAngleMode, SweepAngleMode::Manual);
    QCOMPARE(updatedTask->coverage.sweepAngleDeg, 37.5);
    QVERIFY(!_item->automaticSweepAngle());
    QCOMPARE(_item->sweepAngleDeg(), 37.5);

    _item->setAutomaticSweepAngle(true);
    updatedTask = _marineContext->task(task.id);
    QVERIFY(updatedTask != nullptr);
    QCOMPARE(updatedTask->coverage.sweepAngleMode, SweepAngleMode::Auto);
}

void CoverageComplexItemTest::_testSaveLoad()
{
    MarineTask task = validTask();
    task.planner.plannerId = "marine.coverage.bcd";
    task.coverage.swathWidthM = 20.0;
    task.coverage.sweepAngleMode = SweepAngleMode::Manual;
    task.coverage.sweepAngleDeg = 90.0;
    task.region.noGoRegions = {noGoRectangle()};
    _marineContext->addTask(task);
    _item->setTaskId(QString::fromStdString(task.id));
    QVERIFY(_item->plan());
    _item->setSequenceNumber(12);
    _item->setDirty(false);

    QJsonArray items;
    _item->save(items);
    QCOMPARE(items.size(), 1);
    const QJsonObject object = items.first().toObject();
    QCOMPARE(object.value(QStringLiteral("version")).toInt(), 2);
    QVERIFY(object.contains(QStringLiteral("taskId")));
    QCOMPARE(object.value(QStringLiteral("planningStatus")).toString(), QStringLiteral("success"));
    QCOMPARE(object.value(QStringLiteral("legRoles")).toArray().size(),
             static_cast<qsizetype>(_item->planningResult().legRoles.size()));
    QVERIFY(object.contains(QStringLiteral("selectedSweepAngleDeg")));
    QVERIFY(object.contains(QStringLiteral("turnCount")));
    QVERIFY(!object.contains(QStringLiteral("task")));
    QVERIFY(!object.contains(QStringLiteral("marine")));
    QCOMPARE(object.value(QStringLiteral("coverageLengthM")).toDouble(), _item->coverageLengthM());
    QCOMPARE(object.value(QStringLiteral("transitLengthM")).toDouble(), _item->transitLengthM());
    QCOMPARE(object.value(QStringLiteral("cellCount")).toInt(), _item->cellCount());
    const QStringList internalKeys = {
        QStringLiteral("cells"),
        QStringLiteral("slabs"),
        QStringLiteral("adjacency"),
        QStringLiteral("visibilityGraph"),
        QStringLiteral("routes"),
        QStringLiteral("boundaryComponents"),
        QStringLiteral("coverageSegments"),
        QStringLiteral("transitSegments"),
        QStringLiteral("pathSegments"),
    };
    for (const QString& key : internalKeys) {
        QVERIFY(!object.contains(key));
    }

    QJsonArray repeatedSave;
    _item->save(repeatedSave);
    QCOMPARE(repeatedSave.size(), 1);
    QCOMPARE(repeatedSave.first().toObject(), object);

    auto loadedItem = new CoverageInspectionComplexItem(planController(), false, _marineContext);
    QString errorString;
    QVERIFY2(loadedItem->load(object, 12, errorString), qPrintable(errorString));
    QCOMPARE(loadedItem->taskId(), _item->taskId());
    QCOMPARE(loadedItem->planningState(), CoverageInspectionComplexItem::Planned);
    QCOMPARE(loadedItem->planningResult().status, PlanningStatus::Success);
    QCOMPARE(loadedItem->planningResult().legRoles, _item->planningResult().legRoles);
    QCOMPARE(loadedItem->generatedPathRoleRuns(), _item->generatedPathRoleRuns());
    QCOMPARE(loadedItem->coverageLengthM(), _item->coverageLengthM());
    QCOMPARE(loadedItem->transitLengthM(), _item->transitLengthM());
    QCOMPARE(loadedItem->cellCount(), _item->cellCount());
    QCOMPARE(loadedItem->selectedSweepAngleDeg(), _item->selectedSweepAngleDeg());
    QCOMPARE(loadedItem->turnCount(), _item->turnCount());
    QCOMPARE(loadedItem->planningMessage(), _item->planningMessage());
    QCOMPARE(loadedItem->generatedPath(), _item->generatedPath());
    QCOMPARE(loadedItem->complexDistance(), _item->complexDistance());
    QCOMPARE(loadedItem->sequenceNumber(), 12);
    QCOMPARE(loadedItem->noGoPolygons()->count(), 1);
    QGCMapPolygon* loadedNoGo = loadedItem->noGoPolygons()->value<QGCMapPolygon*>(0);
    QVERIFY(loadedNoGo != nullptr);
    QCOMPARE(loadedNoGo->coordinateList(), _item->noGoPolygons()->value<QGCMapPolygon*>(0)->coordinateList());
    QVERIFY(!loadedItem->dirty());

    int coverageRunCount = 0;
    int transitRunCount = 0;
    verifyRoleRunsReconstructCanonicalPath(*loadedItem, coverageRunCount, transitRunCount);
    QVERIFY(coverageRunCount >= 1);
    QVERIFY(transitRunCount >= 1);

    QList<MissionItem*> missionItems;
    loadedItem->appendMissionItems(missionItems, this);
    verifyMissionItems(missionItems, loadedItem->planningResult(), 12);
}

void CoverageComplexItemTest::_testLawnmowerSaveLoad()
{
    MarineTask task = validTask();
    task.planner.plannerId = "marine.coverage.lawnmower";
    task.coverage.swathWidthM = 30.0;
    task.coverage.safetyMarginM = 1.0;
    task.coverage.sweepAngleMode = SweepAngleMode::Manual;
    task.coverage.sweepAngleDeg = 0.0;
    _marineContext->addTask(task);
    _item->setTaskId(QString::fromStdString(task.id));
    QVERIFY2(_item->plan(), _item->planningResult().message.c_str());

    QJsonArray items;
    _item->save(items);
    QCOMPARE(items.size(), 1);
    const QJsonObject object = items.first().toObject();
    QCOMPARE(object.value(QStringLiteral("version")).toInt(), 2);

    auto loadedItem = new CoverageInspectionComplexItem(planController(), false, _marineContext);
    QString errorString;
    QVERIFY2(loadedItem->load(object, 4, errorString), qPrintable(errorString));
    QCOMPARE(loadedItem->planningResult().legRoles, _item->planningResult().legRoles);
    QCOMPARE(loadedItem->coverageLengthM(), _item->coverageLengthM());
    QCOMPARE(loadedItem->transitLengthM(), _item->transitLengthM());
    QCOMPARE(loadedItem->complexDistance(), _item->complexDistance());
    QCOMPARE(loadedItem->cellCount(), 1);

    QList<MissionItem*> missionItems;
    loadedItem->appendMissionItems(missionItems, this);
    verifyMissionItems(missionItems, loadedItem->planningResult(), 4);
}

void CoverageComplexItemTest::_testLegacyV1Migration()
{
    MarineTask task = validTask();
    task.planner.plannerId = "marine.coverage.lawnmower";
    task.coverage.swathWidthM = 30.0;
    task.coverage.safetyMarginM = 1.0;
    task.coverage.sweepAngleMode = SweepAngleMode::Manual;
    task.coverage.sweepAngleDeg = 0.0;
    _marineContext->addTask(task);

    const QJsonObject fixture = legacyV1Artifact(QString::fromStdString(task.id));
    QString errorString;
    QVERIFY2(_item->load(fixture, 8, errorString), qPrintable(errorString));
    QCOMPARE(_item->planningState(), CoverageInspectionComplexItem::Planned);
    QCOMPARE(_item->planningMessage(), QStringLiteral("Legacy lawnmower artifact"));
    QCOMPARE(_item->generatedPath().size(), 4);
    QVERIFY(_item->planningResult().legRoles.empty());
    QVERIFY(_item->generatedPathRoleRuns().isEmpty());
    QCOMPARE(_item->coverageLengthM(), 0.0);
    QCOMPARE(_item->transitLengthM(), 0.0);
    QCOMPARE(_item->cellCount(), 0);
    QVERIFY(!_item->dirty());

    QList<MissionItem*> legacyMissionItems;
    _item->appendMissionItems(legacyMissionItems, this);
    verifyMissionItems(legacyMissionItems, _item->planningResult(), 8);

    QJsonArray legacySave;
    _item->save(legacySave);
    QCOMPARE(legacySave.size(), 1);
    const QJsonObject legacyObject = legacySave.first().toObject();
    QCOMPARE(legacyObject.value(QStringLiteral("version")).toInt(), 1);
    QCOMPARE(legacyObject.value(QStringLiteral("generatedPath")), fixture.value(QStringLiteral("generatedPath")));
    QVERIFY(!legacyObject.contains(QStringLiteral("legRoles")));
    QVERIFY(!legacyObject.contains(QStringLiteral("coverageLengthM")));
    QVERIFY(!legacyObject.contains(QStringLiteral("transitLengthM")));
    QVERIFY(!legacyObject.contains(QStringLiteral("cellCount")));

    QVERIFY2(_item->plan(), _item->planningResult().message.c_str());
    QJsonArray migratedSave;
    _item->save(migratedSave);
    QCOMPARE(migratedSave.size(), 1);
    const QJsonObject migratedObject = migratedSave.first().toObject();
    QCOMPARE(migratedObject.value(QStringLiteral("version")).toInt(), 2);
    QCOMPARE(migratedObject.value(QStringLiteral("legRoles")).toArray().size(),
             static_cast<qsizetype>(_item->planningResult().path.size() - 1));
    QVERIFY(migratedObject.value(QStringLiteral("cellCount")).toInt() >= 1);
}

void CoverageComplexItemTest::_testUnplannedSaveLoad()
{
    const MarineTask task = validTask();
    _marineContext->addTask(task);
    _item->setTaskId(QString::fromStdString(task.id));
    _item->setDirty(false);

    QJsonArray items;
    _item->save(items);
    QCOMPARE(items.size(), 1);
    const QJsonObject object = items.first().toObject();
    QCOMPARE(object.value(QStringLiteral("version")).toInt(), 2);
    QCOMPARE(object.value(QStringLiteral("planningStatus")).toString(), QStringLiteral("failed"));
    QVERIFY(object.value(QStringLiteral("generatedPath")).toArray().isEmpty());
    QVERIFY(object.value(QStringLiteral("legRoles")).toArray().isEmpty());
    QCOMPARE(object.value(QStringLiteral("coverageLengthM")).toDouble(), 0.0);
    QCOMPARE(object.value(QStringLiteral("transitLengthM")).toDouble(), 0.0);
    QCOMPARE(object.value(QStringLiteral("pathLengthM")).toDouble(), 0.0);
    QCOMPARE(object.value(QStringLiteral("cellCount")).toInt(), 0);
    QCOMPARE(object.value(QStringLiteral("turnCount")).toInt(), 0);

    auto loadedItem = new CoverageInspectionComplexItem(planController(), false, _marineContext);
    QString errorString;
    QVERIFY2(loadedItem->load(object, 3, errorString), qPrintable(errorString));
    QCOMPARE(loadedItem->planningState(), CoverageInspectionComplexItem::Unplanned);
    QCOMPARE(loadedItem->planningResult().status, PlanningStatus::Failed);
    QVERIFY(loadedItem->planningResult().path.empty());
    QVERIFY(loadedItem->planningResult().legRoles.empty());
    QCOMPARE(loadedItem->cellCount(), 0);
    QVERIFY(!loadedItem->dirty());
}

void CoverageComplexItemTest::_testLoadValidation()
{
    const MarineTask task = validTask();
    _marineContext->addTask(task);
    _item->setTaskId(QString::fromStdString(task.id));
    QVERIFY(_item->plan());

    QJsonArray items;
    _item->save(items);
    const QJsonObject validObject = items.first().toObject();
    auto loadedItem = new CoverageInspectionComplexItem(planController(), false, _marineContext);
    QString errorString;
    QVERIFY2(loadedItem->load(validObject, 7, errorString), qPrintable(errorString));
    const QVariantList originalPath = loadedItem->generatedPath();
    const QString originalTaskId = loadedItem->taskId();

    const auto verifyRejected = [&](const QJsonObject& malformed) {
        errorString.clear();
        QVERIFY(!loadedItem->load(malformed, 99, errorString));
        QVERIFY(!errorString.isEmpty());
        QCOMPARE(loadedItem->generatedPath(), originalPath);
        QCOMPARE(loadedItem->taskId(), originalTaskId);
        QCOMPARE(loadedItem->sequenceNumber(), 7);
    };

    QJsonObject emptySuccessfulPath = validObject;
    emptySuccessfulPath.insert(QStringLiteral("generatedPath"), QJsonArray());
    emptySuccessfulPath.insert(QStringLiteral("legRoles"), QJsonArray());
    verifyRejected(emptySuccessfulPath);

    QJsonObject unknownStatus = validObject;
    unknownStatus.insert(QStringLiteral("planningStatus"), QStringLiteral("futureStatus"));
    verifyRejected(unknownStatus);

    QJsonObject negativePathLength = validObject;
    negativePathLength.insert(QStringLiteral("pathLengthM"), -1.0);
    verifyRejected(negativePathLength);

    QJsonObject negativeCoverageLength = validObject;
    negativeCoverageLength.insert(QStringLiteral("coverageLengthM"), -1.0);
    verifyRejected(negativeCoverageLength);

    QJsonObject wrongRoleCount = validObject;
    wrongRoleCount.insert(QStringLiteral("legRoles"), QJsonArray());
    verifyRejected(wrongRoleCount);

    QJsonObject unknownRole = validObject;
    QJsonArray unknownRoles = unknownRole.value(QStringLiteral("legRoles")).toArray();
    unknownRoles[0] = QStringLiteral("turn");
    unknownRole.insert(QStringLiteral("legRoles"), unknownRoles);
    verifyRejected(unknownRole);

    QJsonObject nonFiniteMetric = validObject;
    nonFiniteMetric.insert(QStringLiteral("coverageLengthM"), std::numeric_limits<double>::infinity());
    verifyRejected(nonFiniteMetric);

    QJsonObject mismatchedMetrics = validObject;
    mismatchedMetrics.insert(QStringLiteral("pathLengthM"),
                             validObject.value(QStringLiteral("pathLengthM")).toDouble() + 1.0);
    verifyRejected(mismatchedMetrics);

    QJsonObject negativeCellCount = validObject;
    negativeCellCount.insert(QStringLiteral("cellCount"), -1);
    verifyRejected(negativeCellCount);

    QJsonObject zeroCellCount = validObject;
    zeroCellCount.insert(QStringLiteral("cellCount"), 0);
    verifyRejected(zeroCellCount);

    QJsonObject fractionalCellCount = validObject;
    fractionalCellCount.insert(QStringLiteral("cellCount"), 1.5);
    verifyRejected(fractionalCellCount);

    QJsonObject invalidAngle = validObject;
    invalidAngle.insert(QStringLiteral("selectedSweepAngleDeg"), 180.0);
    verifyRejected(invalidAngle);

    QJsonObject negativeTurnCount = validObject;
    negativeTurnCount.insert(QStringLiteral("turnCount"), -1);
    verifyRejected(negativeTurnCount);

    QJsonObject failedWithPath = validObject;
    failedWithPath.insert(QStringLiteral("planningStatus"), QStringLiteral("failed"));
    verifyRejected(failedWithPath);

    QJsonObject failedWithRole = validObject;
    failedWithRole.insert(QStringLiteral("planningStatus"), QStringLiteral("failed"));
    failedWithRole.insert(QStringLiteral("generatedPath"), QJsonArray());
    failedWithRole.insert(QStringLiteral("legRoles"), QJsonArray{QStringLiteral("coverage")});
    failedWithRole.insert(QStringLiteral("coverageLengthM"), 0.0);
    failedWithRole.insert(QStringLiteral("transitLengthM"), 0.0);
    failedWithRole.insert(QStringLiteral("pathLengthM"), 0.0);
    failedWithRole.insert(QStringLiteral("cellCount"), 0);
    failedWithRole.insert(QStringLiteral("turnCount"), 0);
    verifyRejected(failedWithRole);

    QJsonObject invalidCoordinate = validObject;
    QJsonArray invalidPath = invalidCoordinate.value(QStringLiteral("generatedPath")).toArray();
    invalidPath[0] = QJsonArray{91.0, 8.5455, 0.0};
    invalidCoordinate.insert(QStringLiteral("generatedPath"), invalidPath);
    verifyRejected(invalidCoordinate);

    QJsonObject missingV2Field = validObject;
    missingV2Field.remove(QStringLiteral("legRoles"));
    verifyRejected(missingV2Field);

    QJsonObject unsupportedVersion = validObject;
    unsupportedVersion.insert(QStringLiteral("version"), 3);
    verifyRejected(unsupportedVersion);

    QJsonObject brokenTaskReference = validObject;
    brokenTaskReference.insert(QStringLiteral("taskId"), QStringLiteral("missing-task"));
    verifyRejected(brokenTaskReference);
}

UT_REGISTER_TEST(CoverageComplexItemTest, TestLabel::Unit, TestLabel::MissionManager)
