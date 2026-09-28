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
#include <functional>
#include <limits>
#include <memory>

#include "BoustrophedonCoveragePlanner.h"
#include "CoverageInspectionComplexItem.h"
#include "CoverageProblemValidator.h"
#include "LawnmowerCoveragePlanner.h"
#include "MarinePlanContext.h"
#include "MissionItem.h"
#include "MockCoveragePlanner.h"
#include "PlanningInputIdentity.h"
#include "QGCMapPolygon.h"

using namespace Marine;

namespace {

MarineTask validTask()
{
    MarineTask task;
    task.planner.plannerId = "marine.coverage.mock";
    task.coverage.swathWidthM = 5.0;
    task.region.coverageBoundary.vertices = {
        {47.3977, 8.5455, 0.0},
        {47.3977, 8.5465, 0.0},
        {47.3987, 8.5465, 0.0},
        {47.3987, 8.5455, 0.0},
    };
    task.region.navigationBoundary = task.region.coverageBoundary;
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

QJsonObject roleRunArtifact(const MarineTask& task, const std::vector<PathLegRole>& roles)
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
        {QStringLiteral("version"), 3},
        {QStringLiteral("resultContract"), QStringLiteral("InfrastructureOnly")},
        {QStringLiteral("inputIdentity"), PlanningInputIdentity::fromTask(task)->toJson()},
        {QStringLiteral("type"), QStringLiteral("ComplexItem")},
        {QStringLiteral("complexItemType"), QStringLiteral("coverageInspection")},
        {QStringLiteral("taskId"), QString::fromStdString(task.id)},
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

class CountingPlanner final : public ICoveragePlanner
{
public:
    mutable int calls = 0;

    std::string id() const final { return "marine.coverage.counting"; }

    std::string displayName() const final { return "Counting test planner"; }

    CoveragePlanningSolution plan(const CoveragePlanningProblem& problem) const final
    {
        ++calls;
        return MockCoveragePlanner{}.plan(problem);
    }
};

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

    QCOMPARE(_item->planningState(), CoverageInspectionComplexItem::Unplanned);
    QCOMPARE(_item->planningResult().status, PlanningStatus::Failed);
    QVERIFY(_item->generatedPath().isEmpty());
    QVERIFY(_item->planningArtifact().has_value());
    QCOMPARE(_item->planningArtifact()->result.status, PlanningStatus::Success);
    QCOMPARE(_item->planningArtifact()->result.path.size(), std::size_t{3});
    QVERIFY(!_item->planningArtifact()->stale);
    QCOMPARE(_item->complexDistance(), 0.0);
    QVERIFY(!_item->specifiesCoordinate());
    QCOMPARE(_item->readyForSaveState(), VisualMissionItem::ReadyForSave);
    QCOMPARE(_item->lastSequenceNumber(), 0);
    QVERIFY(_item->dirty());
    QCOMPARE(stateSpy.count(), 0);
    QCOMPARE(pathSpy.count(), 0);
    QCOMPARE(lastSequenceSpy.count(), 0);

    QList<MissionItem*> items;
    _item->appendMissionItems(items, this);
    QVERIFY(items.isEmpty());
}

void CoverageComplexItemTest::_testLawnmowerPlanning()
{
    MarineTask task = validTask();
    task.planner.plannerId = "marine.coverage.lawnmower";
    task.coverage.swathWidthM = 30.0;
    task.safety.hardSafetyMarginM = 1.0;
    task.safety.preferredSafetyMarginM = 1.0;
    task.coverage.sweepAngleMode = SweepAngleMode::Manual;
    task.coverage.sweepAngleDeg = 0.0;
    _marineContext->addTask(task);
    _item->setTaskId(QString::fromStdString(task.id));

    QVERIFY2(_item->plan(), _item->planningResult().message.c_str());

    QVERIFY(_item->planningArtifact().has_value());
    const PlanningResult& result = _item->planningArtifact()->result;
    QCOMPARE(_item->planningState(), CoverageInspectionComplexItem::Unplanned);
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
    QCOMPARE(_item->selectedSweepAngleDeg(), 0.0);
    QCOMPARE(_item->turnCount(), 0);
    QVERIFY(_item->generatedPath().isEmpty());
    QCOMPARE(_item->complexDistance(), 0.0);
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
    QVERIFY(_item->planningArtifact().has_value());
    QCOMPARE(_item->planningArtifact()->result.message,
             CoverageProblemValidator::messageForError(CoveragePlanningError::UnsupportedNoGoRegion));
}

void CoverageComplexItemTest::_testBoustrophedonNoGoPlanning()
{
    MarineTask task = validTask();
    task.planner.plannerId = "marine.coverage.bcd";
    task.coverage.swathWidthM = 20.0;
    task.safety.hardSafetyMarginM = 0.0;
    task.safety.preferredSafetyMarginM = 0.0;
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

    QVERIFY(_item->planningArtifact().has_value());
    const PlanningResult& result = _item->planningArtifact()->result;
    QCOMPARE(_item->planningState(), CoverageInspectionComplexItem::Unplanned);
    QCOMPARE(result.status, PlanningStatus::Success);
    QVERIFY(!result.path.empty());
    QCOMPARE(result.legRoles.size(), result.path.size() - 1);
    QVERIFY(std::isfinite(result.pathLengthM));
    QCOMPARE(result.pathLengthM, result.coverageLengthM + result.transitLengthM);
    QVERIFY(result.cellCount >= 1);

    QVERIFY(_item->generatedPathRoleRuns().isEmpty());

    QList<MissionItem*> missionItems;
    _item->appendMissionItems(missionItems, this);
    QVERIFY(missionItems.isEmpty());
}

void CoverageComplexItemTest::_testGeneratedPathRoleRuns()
{
    const MarineTask task = validTask();
    _marineContext->addTask(task);
    const std::vector<std::vector<PathLegRole>> fixtures = {
        {PathLegRole::Coverage, PathLegRole::Coverage, PathLegRole::Transit, PathLegRole::Coverage},
        std::vector<PathLegRole>(4, PathLegRole::Coverage),
        std::vector<PathLegRole>(4, PathLegRole::Transit),
    };
    for (const auto& roles : fixtures) {
        QString error;
        QVERIFY2(_item->load(roleRunArtifact(task, roles), 0, error), qPrintable(error));
        QVERIFY(_item->planningArtifact().has_value());
        QCOMPARE(_item->planningArtifact()->result.legRoles, roles);
        QVERIFY(_item->generatedPathRoleRuns().isEmpty());
        QVERIFY(_item->generatedPath().isEmpty());
        QList<MissionItem*> mission;
        _item->appendMissionItems(mission, this);
        QVERIFY(mission.isEmpty());
    }
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

    task.region.coverageBoundary.vertices.clear();
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
    task.safety.hardSafetyMarginM = 2.25;
    task.safety.preferredSafetyMarginM = 2.25;
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
    QVERIFY(_item->planningArtifact().has_value());
    QVERIFY(_item->planningArtifact()->result.message.find("selected coverage planner") != std::string::npos);
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
    QCOMPARE(updatedTask->safety.hardSafetyMarginM, 3.0);
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
    task.safety.hardSafetyMarginM = 1.0;
    task.safety.preferredSafetyMarginM = 1.0;
    _marineContext->addTask(task);
    _item->setTaskId(QString::fromStdString(task.id));

    QGCMapPolygon* polygon = _item->workRegionPolygon();
    QVERIFY(polygon != nullptr);
    QCOMPARE(polygon->count(), 4);
    QCOMPARE(polygon->vertexCoordinate(0).latitude(), task.region.coverageBoundary.vertices[0].latitudeDeg);
    QVERIFY2(_item->plan(), _item->planningResult().message.c_str());

    QSignalSpy taskDataSpy(_item, &CoverageInspectionComplexItem::taskDataChanged);
    const QGeoCoordinate editedCoordinate(47.3978, 8.5456);
    polygon->adjustVertex(0, editedCoordinate);

    QTRY_VERIFY(taskDataSpy.count() >= 1);
    const MarineTask* editedTask = _marineContext->task(task.id);
    QVERIFY(editedTask != nullptr);
    QCOMPARE(editedTask->region.coverageBoundary.vertices[0].latitudeDeg, editedCoordinate.latitude());
    QCOMPARE(editedTask->region.coverageBoundary.vertices[0].longitudeDeg, editedCoordinate.longitude());
    QCOMPARE(editedTask->region.coverageBoundary.vertices[0].altitudeM, 0.0);
    QCOMPARE(editedTask->region.navigationBoundary.vertices[0].latitudeDeg,
             task.region.navigationBoundary.vertices[0].latitudeDeg);
    QCOMPARE(_item->planningState(), CoverageInspectionComplexItem::Unplanned);

    MarineTask externallyUpdatedTask = *editedTask;
    externallyUpdatedTask.region.coverageBoundary.vertices.resize(3);
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

void CoverageComplexItemTest::_testSafetyMarginEditPreservesTaskValidity()
{
    MarineTask task = validTask();
    task.safety.hardSafetyMarginM = 1.0;
    task.safety.preferredSafetyMarginM = 2.0;
    _marineContext->addTask(task);
    _item->setTaskId(QString::fromStdString(task.id));

    _item->setSafetyMarginM(3.0);

    const MarineTask* updatedTask = _marineContext->task(task.id);
    QVERIFY(updatedTask != nullptr);
    QCOMPARE(updatedTask->safety.hardSafetyMarginM, 3.0);
    QCOMPARE(updatedTask->safety.preferredSafetyMarginM, 3.0);
    QVERIFY(updatedTask->schemaValid());
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
    QJsonArray saved;
    _item->save(saved);
    QCOMPARE(saved.size(), 1);
    const QJsonObject object = saved.first().toObject();
    QCOMPARE(object.value("version").toInt(), 3);
    QCOMPARE(object.value("resultContract").toString(), QStringLiteral("InfrastructureOnly"));
    QVERIFY(object.contains("inputIdentity"));
    QVERIFY(!object.contains("task"));
    QVERIFY(!object.contains("marine"));
    // An empty registry proves loading cannot require a planner.
    MarinePlanContext loadContext(planController());
    loadContext.addTask(task);
    CoverageInspectionComplexItem loaded(planController(), false, &loadContext);
    QString error;
    QVERIFY2(loaded.load(object, 12, error), qPrintable(error));
    QVERIFY(loaded.planningArtifact().has_value());
    QVERIFY(!loaded.planningArtifact()->stale);
    QCOMPARE(loaded.planningArtifact()->result.legRoles, _item->planningArtifact()->result.legRoles);
    QCOMPARE(loaded.noGoPolygons()->count(), 1);
    QCOMPARE(loaded.planningState(), CoverageInspectionComplexItem::Unplanned);
    QVERIFY(loaded.planningResult().path.empty());
    QVERIFY(loaded.generatedPath().isEmpty());
    QVERIFY(!loaded.dirty());
    QList<MissionItem*> mission;
    loaded.appendMissionItems(mission, this);
    QVERIFY(mission.isEmpty());
    QJsonArray resaved;
    loaded.save(resaved);
    QCOMPARE(resaved, saved);
    QVERIFY(loaded.load(resaved.first().toObject(), 12, error));
    loaded.appendMissionItems(mission, this);
    QVERIFY(mission.isEmpty());
}

void CoverageComplexItemTest::_testLawnmowerSaveLoad()
{
    MarineTask task = validTask();
    task.planner.plannerId = "marine.coverage.lawnmower";
    task.coverage.swathWidthM = 30.0;
    task.coverage.sweepAngleMode = SweepAngleMode::Manual;
    _marineContext->addTask(task);
    _item->setTaskId(QString::fromStdString(task.id));
    QVERIFY(_item->plan());
    QJsonArray saved;
    _item->save(saved);
    QCOMPARE(saved.size(), 1);
    CoverageInspectionComplexItem loaded(planController(), false, _marineContext);
    QString error;
    QVERIFY2(loaded.load(saved.first().toObject(), 4, error), qPrintable(error));
    QVERIFY(loaded.planningArtifact().has_value());
    QCOMPARE(loaded.planningArtifact()->result.cellCount, 1);
    QCOMPARE(loaded.planningArtifact()->result.legRoles, _item->planningArtifact()->result.legRoles);
    QJsonArray resaved;
    loaded.save(resaved);
    QCOMPARE(resaved, saved);
    QList<MissionItem*> mission;
    loaded.appendMissionItems(mission, this);
    QVERIFY(mission.isEmpty());
}

void CoverageComplexItemTest::_testLegacySchemasRejected()
{
    const MarineTask task = validTask();
    _marineContext->addTask(task);
    _item->setTaskId(QString::fromStdString(task.id));
    for (const int version : {1, 2}) {
        auto object = legacyV1Artifact(_item->taskId());
        object.insert("version", version);
        QString error;
        QVERIFY(!_item->load(object, 8, error));
        QVERIFY(error.contains("Unsupported development schema"));
        QVERIFY(!_item->planningArtifact().has_value());
        QVERIFY(_item->planningResult().path.empty());
    }
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
    QCOMPARE(object.value(QStringLiteral("version")).toInt(), 3);
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

    QJsonObject missingRoleField = validObject;
    missingRoleField.remove(QStringLiteral("legRoles"));
    verifyRejected(missingRoleField);

    QJsonObject unsupportedVersion = validObject;
    unsupportedVersion.insert(QStringLiteral("version"), 4);
    verifyRejected(unsupportedVersion);

    for (const QString& key : {QStringLiteral("resultContract"), QStringLiteral("inputIdentity")}) {
        auto missing = validObject;
        missing.remove(key);
        verifyRejected(missing);
    }

    QJsonObject brokenTaskReference = validObject;
    brokenTaskReference.insert(QStringLiteral("taskId"), QStringLiteral("missing-task"));
    verifyRejected(brokenTaskReference);
}

void CoverageComplexItemTest::_testArtifactIdentity()
{
    auto planner = std::make_shared<CountingPlanner>();
    QVERIFY(_marineContext->plannerRegistry().registerPlanner(planner));
    MarineTask task = validTask();
    task.planner.plannerId = planner->id();
    task.safety.hardSafetyMarginM = 1;
    task.safety.preferredSafetyMarginM = 2;
    task.coverage.sweepAngleMode = SweepAngleMode::Manual;
    _marineContext->addTask(task);
    _item->setTaskId(QString::fromStdString(task.id));
    QVERIFY(_item->plan());
    QCOMPARE(planner->calls, 1);
    QJsonArray saved;
    _item->save(saved);
    QCOMPARE(saved.size(), 1);
    const auto object = saved.first().toObject();
    QString error;
    QVERIFY(_item->load(object, 0, error));
    _item->setTaskName(QStringLiteral("Renamed"));
    _item->setCameraEnabled(false);
    _item->setSonarRecord(false);
    QVERIFY(!_item->planningArtifact()->stale);
    QVERIFY(_item->dirty());
    QCOMPARE(planner->calls, 1);
    const std::vector<std::function<void(MarineTask&)>> changes = {
        [](auto& t) { t.region.coverageBoundary.vertices[0].latitudeDeg += 0.00001; },
        [](auto& t) { t.region.navigationBoundary.vertices[0].longitudeDeg += 0.00001; },
        [](auto& t) { t.region.noGoRegions.push_back(noGoRectangle()); },
        [](auto& t) { t.coverage.swathWidthM += 1; },
        [](auto& t) { t.safety.hardSafetyMarginM += 0.1; },
        [](auto& t) { t.safety.preferredSafetyMarginM += 1; },
        [](auto& t) { t.planner.executionSafety.executionMarginM += 0.1; },
        [](auto& t) { t.coverage.coverageRequirement = CoverageRequirement::Strict; },
        [](auto& t) { t.coverage.sweepAngleMode = SweepAngleMode::Auto; },
        [](auto& t) { t.coverage.sweepAngleDeg += 1; },
        [](auto& t) { t.planner.plannerId = "unknown"; },
    };
    for (const auto& change : changes) {
        _marineContext->addTask(task);
        QVERIFY(_item->load(object, 0, error));
        auto changed = task;
        change(changed);
        _marineContext->addTask(changed);
        QVERIFY(_item->planningArtifact()->stale);
        QCOMPARE(_item->planningState(), CoverageInspectionComplexItem::Unplanned);
        QVERIFY(_item->planningResult().path.empty());
        QJsonArray resaved;
        _item->save(resaved);
        QCOMPARE(resaved, saved);
        QVERIFY(_item->load(object, 0, error));
        QVERIFY(_item->planningArtifact()->stale);
        QList<MissionItem*> mission;
        _item->appendMissionItems(mission, this);
        QVERIFY(mission.isEmpty());
        QCOMPARE(planner->calls, 1);
    }
    _marineContext->addTask(task);
    for (const QString& key : {QStringLiteral("planningVersion"), QStringLiteral("policyVersion"),
                               QStringLiteral("resolvedStrategy"), QStringLiteral("strategyVersion")}) {
        auto changed = object;
        auto identity = changed.value("inputIdentity").toObject();
        identity.insert(key, "unsupported-future-semantics");
        changed.insert("inputIdentity", identity);
        QVERIFY(_item->load(changed, 0, error));
        QVERIFY(_item->planningArtifact()->stale);
        QVERIFY(_item->planningResult().path.empty());
        QCOMPARE(planner->calls, 1);
    }
}

UT_REGISTER_TEST(CoverageComplexItemTest, TestLabel::Unit, TestLabel::MissionManager)
