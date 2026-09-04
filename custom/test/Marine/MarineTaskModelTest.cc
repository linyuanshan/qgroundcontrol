#include "MarineTaskModelTest.h"

#include <QtCore/QRegularExpression>

#include "MarineTask.h"

using namespace Marine;

void MarineTaskModelTest::_testDefaults()
{
    const MarineTask task;

    QVERIFY(task.type == MarineTaskType::CoverageInspection);
    QVERIFY(task.name.empty());
    QVERIFY(task.vehicleId.empty());
    QVERIFY(task.planner.plannerId.empty());
    QCOMPARE(task.coverage.swathWidthM, 0.0);
    QCOMPARE(task.coverage.safetyMarginM, 0.0);
    QVERIFY(task.coverage.sweepAngleMode == SweepAngleMode::Auto);
    QCOMPARE(task.coverage.sweepAngleDeg, 0.0);
    QVERIFY(task.sensors.cameraEnabled);
    QVERIFY(task.sensors.cameraRecord);
    QVERIFY(task.sensors.sonarEnabled);
    QVERIFY(task.sensors.sonarRecord);
}

void MarineTaskModelTest::_testTaskId()
{
    MarineTask task;
    const std::string originalId = task.id;
    const QRegularExpression uuidPattern(
        QStringLiteral("^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$"));

    QVERIFY(uuidPattern.match(QString::fromStdString(task.id)).hasMatch());
    QVERIFY(MarineTask().id != task.id);

    task.name = "Harbor inspection";
    task.vehicleId = "usv-1";
    task.coverage.swathWidthM = 5.0;
    task.planner.plannerId = "marine.coverage.mock";
    task.region.outerBoundary.vertices.push_back({47.0, 8.0, 0.0});

    QVERIFY(task.id == originalId);
}

void MarineTaskModelTest::_testWorkRegion()
{
    MarineTask task;
    task.region.outerBoundary.vertices = {
        {47.0, 8.0, 0.0},
        {47.0, 8.1, 0.0},
        {47.1, 8.1, 0.0},
    };
    task.region.noGoRegions.push_back({{
        {47.02, 8.02, 0.0},
        {47.02, 8.03, 0.0},
        {47.03, 8.03, 0.0},
    }});

    QCOMPARE(task.region.outerBoundary.vertices.size(), std::size_t{3});
    QCOMPARE(task.region.noGoRegions.size(), std::size_t{1});
    QCOMPARE(task.region.noGoRegions.front().vertices.size(), std::size_t{3});
    QCOMPARE(task.region.noGoRegions.front().vertices.front().latitudeDeg, 47.02);
}

void MarineTaskModelTest::_testConfiguration()
{
    MarineTask task;
    task.vehicleId = "usv-1";
    task.coverage.swathWidthM = 4.5;
    task.coverage.safetyMarginM = 1.25;
    task.coverage.sweepAngleMode = SweepAngleMode::Manual;
    task.coverage.sweepAngleDeg = 75.0;
    task.sensors.cameraEnabled = false;
    task.sensors.cameraRecord = false;
    task.sensors.sonarEnabled = true;
    task.sensors.sonarRecord = false;

    QVERIFY(task.vehicleId == "usv-1");
    QCOMPARE(task.coverage.swathWidthM, 4.5);
    QCOMPARE(task.coverage.safetyMarginM, 1.25);
    QVERIFY(task.coverage.sweepAngleMode == SweepAngleMode::Manual);
    QCOMPARE(task.coverage.sweepAngleDeg, 75.0);
    QVERIFY(!task.sensors.cameraEnabled);
    QVERIFY(!task.sensors.cameraRecord);
    QVERIFY(task.sensors.sonarEnabled);
    QVERIFY(!task.sensors.sonarRecord);
}

void MarineTaskModelTest::_testValidity()
{
    MarineTask task;

    QVERIFY(!task.isValid());
    task.region.outerBoundary.vertices.push_back({47.0, 8.0, 0.0});
    task.region.outerBoundary.vertices.push_back({47.0, 8.1, 0.0});
    QVERIFY(!task.isValid());
    task.region.outerBoundary.vertices.push_back({47.1, 8.1, 0.0});
    QVERIFY(task.isValid());

    task.id.clear();
    QVERIFY(!task.isValid());
}

UT_REGISTER_TEST_LIGHTWEIGHT(MarineTaskModelTest, TestLabel::Unit)
