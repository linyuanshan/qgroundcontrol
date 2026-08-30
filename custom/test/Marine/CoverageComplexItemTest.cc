#include "CoverageComplexItemTest.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QVariantList>
#include <QtTest/QSignalSpy>

#include <memory>

#include "CoverageInspectionComplexItem.h"
#include "MarinePlanContext.h"
#include "MockCoveragePlanner.h"

using namespace Marine;

namespace {

MarineTask validTask()
{
    MarineTask task;
    task.planner.plannerId = "marine.coverage.mock";
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
    QVERIFY(_item->dirty());
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(pathSpy.count(), 1);
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

    QJsonObject negativePathLength = validObject;
    negativePathLength.insert(QStringLiteral("pathLengthM"), -1.0);
    errorString.clear();
    QVERIFY(!loadedItem->load(negativePathLength, 0, errorString));
    QVERIFY(!errorString.isEmpty());
}

UT_REGISTER_TEST(CoverageComplexItemTest, TestLabel::Unit, TestLabel::MissionManager)
