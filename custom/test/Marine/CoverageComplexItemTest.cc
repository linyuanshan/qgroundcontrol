#include "CoverageComplexItemTest.h"

#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QVariantList>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtTest/QSignalSpy>

#include <memory>

#include "CoverageInspectionComplexItem.h"
#include "MarinePlanContext.h"
#include "MissionItem.h"
#include "MockCoveragePlanner.h"

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

}  // namespace

void CoverageComplexItemTest::init()
{
    OfflineMissionTest::init();
    _marineContext = new MarinePlanContext(planController());
    QVERIFY(_marineContext->plannerRegistry().registerPlanner(std::make_shared<MockCoveragePlanner>()));
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
    QVERIFY(_item->planningResult().message.find("P2") != std::string::npos);
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

void CoverageComplexItemTest::_testSaveLoad()
{
    const MarineTask task = validTask();
    _marineContext->addTask(task);
    _item->setTaskId(QString::fromStdString(task.id));
    QVERIFY(_item->plan());
    _item->setSequenceNumber(12);
    _item->setDirty(false);

    QJsonArray items;
    _item->save(items);
    QCOMPARE(items.size(), 1);
    const QJsonObject object = items.first().toObject();
    QVERIFY(object.contains(QStringLiteral("taskId")));
    QCOMPARE(object.value(QStringLiteral("planningStatus")).toString(), QStringLiteral("success"));
    QVERIFY(!object.contains(QStringLiteral("task")));
    QVERIFY(!object.contains(QStringLiteral("marine")));

    auto loadedItem = new CoverageInspectionComplexItem(planController(), false, _marineContext);
    QString errorString;
    QVERIFY2(loadedItem->load(object, 12, errorString), qPrintable(errorString));
    QCOMPARE(loadedItem->taskId(), _item->taskId());
    QCOMPARE(loadedItem->planningState(), CoverageInspectionComplexItem::Planned);
    QCOMPARE(loadedItem->planningResult().status, PlanningStatus::Success);
    QCOMPARE(loadedItem->generatedPath(), _item->generatedPath());
    QCOMPARE(loadedItem->complexDistance(), _item->complexDistance());
    QCOMPARE(loadedItem->sequenceNumber(), 12);
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

    QJsonObject emptySuccessfulPath = validObject;
    emptySuccessfulPath.insert(QStringLiteral("generatedPath"), QJsonArray());
    QVERIFY(!loadedItem->load(emptySuccessfulPath, 0, errorString));
    QVERIFY(!errorString.isEmpty());

    QJsonObject unknownStatus = validObject;
    unknownStatus.insert(QStringLiteral("planningStatus"), QStringLiteral("futureStatus"));
    errorString.clear();
    QVERIFY(!loadedItem->load(unknownStatus, 0, errorString));
    QVERIFY(errorString.contains(QStringLiteral("status"), Qt::CaseInsensitive));

    QJsonObject negativePathLength = validObject;
    negativePathLength.insert(QStringLiteral("pathLengthM"), -1.0);
    errorString.clear();
    QVERIFY(!loadedItem->load(negativePathLength, 0, errorString));
    QVERIFY(!errorString.isEmpty());

    QJsonObject brokenTaskReference = validObject;
    brokenTaskReference.insert(QStringLiteral("taskId"), QStringLiteral("missing-task"));
    errorString.clear();
    QVERIFY(!loadedItem->load(brokenTaskReference, 0, errorString));
    QVERIFY(errorString.contains(QStringLiteral("missing-task")));
}

UT_REGISTER_TEST(CoverageComplexItemTest, TestLabel::Unit, TestLabel::MissionManager)
