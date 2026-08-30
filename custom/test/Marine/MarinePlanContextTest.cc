#include "MarinePlanContextTest.h"

#include <memory>

#include "MarinePlanContext.h"
#include "MockCoveragePlanner.h"
#include "PlanMasterController.h"

using namespace Marine;

void MarinePlanContextTest::_testControllerOwnsContext()
{
    PlanMasterController controller;
    const auto context = new MarinePlanContext(&controller);

    QCOMPARE(context->parent(), &controller);
}

void MarinePlanContextTest::_testAddAndFind()
{
    MarinePlanContext context(nullptr);
    MarineTask task;
    task.name = "Harbor inspection";

    context.addTask(task);
    const std::string storedName = task.name;
    task.name = "Changed outside context";

    MarineTask* storedTask = context.task(task.id);
    QVERIFY(storedTask);
    QCOMPARE(storedTask->id, task.id);
    QCOMPARE(storedTask->name, storedName);

    const MarinePlanContext& constContext = context;
    QVERIFY(constContext.task(task.id));
    QVERIFY(!constContext.task("missing-task"));
}

void MarinePlanContextTest::_testDuplicateIdReplaces()
{
    MarinePlanContext context(nullptr);
    MarineTask original;
    original.name = "Original";
    MarineTask replacement = original;
    replacement.name = "Replacement";

    context.addTask(original);
    context.addTask(replacement);

    const MarineTask* storedTask = context.task(original.id);
    QVERIFY(storedTask);
    QCOMPARE(storedTask->name, replacement.name);
}

void MarinePlanContextTest::_testEmptyIdIgnored()
{
    MarinePlanContext context(nullptr);
    MarineTask task;
    task.id.clear();

    context.addTask(task);

    QVERIFY(!context.task(""));
}

void MarinePlanContextTest::_testRemoveAndClear()
{
    MarinePlanContext context(nullptr);
    MarineTask first;
    MarineTask second;
    context.addTask(first);
    context.addTask(second);

    context.removeTask(first.id);
    QVERIFY(!context.task(first.id));
    QVERIFY(context.task(second.id));

    context.removeTask("missing-task");
    context.clearTasks();
    QVERIFY(!context.task(second.id));
    context.clearTasks();
}

void MarinePlanContextTest::_testContextsAreIsolated()
{
    MarinePlanContext firstContext(nullptr);
    MarinePlanContext secondContext(nullptr);
    MarineTask task;

    firstContext.addTask(task);

    QVERIFY(firstContext.task(task.id));
    QVERIFY(!secondContext.task(task.id));
}

void MarinePlanContextTest::_testPlannerRegistriesAreIsolated()
{
    MarinePlanContext firstContext(nullptr);
    MarinePlanContext secondContext(nullptr);
    const auto planner = std::make_shared<MockCoveragePlanner>();

    QVERIFY(firstContext.plannerRegistry().registerPlanner(planner));
    QVERIFY(firstContext.plannerRegistry().planner(planner->id()));
    QVERIFY(!secondContext.plannerRegistry().planner(planner->id()));

    const MarinePlanContext& constContext = firstContext;
    QVERIFY(constContext.plannerRegistry().planner(planner->id()));
}

UT_REGISTER_TEST_LIGHTWEIGHT(MarinePlanContextTest, TestLabel::Unit)
