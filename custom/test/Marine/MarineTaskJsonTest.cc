#include "MarineTaskJsonTest.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>

#include "MarineTaskJsonCodec.h"

using namespace Marine;

namespace {

MarineTask createTask()
{
    MarineTask task;
    task.id = "9a20e6c8-1234-4567-89ab-123456789abc";
    task.name = "Harbor inspection";
    task.vehicleId = "usv-1";
    task.region.outerBoundary.vertices = {
        {38.1, 121.1, 0.0},
        {38.1, 121.2, 0.0},
        {38.2, 121.2, 0.0},
    };
    task.coverage.swathWidthM = 10.0;
    task.coverage.safetyMarginM = 2.0;
    task.coverage.sweepAngleMode = SweepAngleMode::Manual;
    task.coverage.sweepAngleDeg = 37.5;
    task.planner.plannerId = "marine.coverage.mock";
    task.sensors.cameraEnabled = true;
    task.sensors.cameraRecord = false;
    task.sensors.sonarEnabled = true;
    task.sensors.sonarRecord = false;
    return task;
}

QJsonObject saveTask(const MarineTask& task)
{
    QJsonObject json;
    QString errorString;
    if (!MarineTaskJsonCodec::save(task, json, errorString)) {
        QTest::qFail(qPrintable(errorString), __FILE__, __LINE__);
    }
    return json;
}

MarineTask loadTask(const QJsonObject& json)
{
    MarineTask task;
    QString errorString;
    if (!MarineTaskJsonCodec::load(json, task, errorString)) {
        QTest::qFail(qPrintable(errorString), __FILE__, __LINE__);
    }
    return task;
}

}  // namespace

void MarineTaskJsonTest::_testRoundTrip()
{
    const MarineTask source = createTask();
    const QJsonObject json = saveTask(source);
    const MarineTask loaded = loadTask(json);

    QCOMPARE(json.value("version").toInt(), 1);
    QCOMPARE(json.value("type").toString(), QStringLiteral("coverageInspection"));
    QVERIFY(loaded.id == source.id);
    QVERIFY(loaded.name == source.name);
    QVERIFY(loaded.type == source.type);
    QVERIFY(loaded.vehicleId == source.vehicleId);
    QCOMPARE(loaded.region.outerBoundary.vertices.size(), source.region.outerBoundary.vertices.size());
    QCOMPARE(loaded.region.outerBoundary.vertices[1].latitudeDeg, 38.1);
    QCOMPARE(loaded.region.outerBoundary.vertices[1].longitudeDeg, 121.2);
    QCOMPARE(loaded.coverage.swathWidthM, source.coverage.swathWidthM);
    QCOMPARE(loaded.coverage.safetyMarginM, source.coverage.safetyMarginM);
    QCOMPARE(loaded.coverage.sweepAngleMode, source.coverage.sweepAngleMode);
    QCOMPARE(loaded.coverage.sweepAngleDeg, source.coverage.sweepAngleDeg);
    QVERIFY(loaded.planner.plannerId == source.planner.plannerId);
}

void MarineTaskJsonTest::_testNoGoRegions()
{
    MarineTask source = createTask();
    source.region.noGoRegions.push_back({{
        {38.12, 121.12, 0.0},
        {38.12, 121.14, 0.0},
        {38.14, 121.14, 0.0},
    }});

    const MarineTask loaded = loadTask(saveTask(source));

    QCOMPARE(loaded.region.noGoRegions.size(), std::size_t{1});
    QCOMPARE(loaded.region.noGoRegions.front().vertices.size(), std::size_t{3});
    QCOMPARE(loaded.region.noGoRegions.front().vertices.front().latitudeDeg, 38.12);
    QCOMPARE(loaded.region.noGoRegions.front().vertices.front().longitudeDeg, 121.12);
}

void MarineTaskJsonTest::_testSensorConfig()
{
    const MarineTask source = createTask();
    const MarineTask loaded = loadTask(saveTask(source));

    QCOMPARE(loaded.sensors.cameraEnabled, true);
    QCOMPARE(loaded.sensors.cameraRecord, false);
    QCOMPARE(loaded.sensors.sonarEnabled, true);
    QCOMPARE(loaded.sensors.sonarRecord, false);
}

void MarineTaskJsonTest::_testUnknownField()
{
    QJsonObject json = saveTask(createTask());
    json.insert("futureField", QJsonObject{{"enabled", true}});
    QJsonObject region = json.value("region").toObject();
    region.insert("futureRegionField", QJsonArray{1, 2, 3});
    json.insert("region", region);

    QString errorString;
    MarineTask loaded;
    QVERIFY2(MarineTaskJsonCodec::load(json, loaded, errorString), qPrintable(errorString));
    QVERIFY(loaded.isValid());
}

void MarineTaskJsonTest::_testUnsupportedVersion()
{
    const QJsonObject json{{"version", 2}, {"futureField", true}};

    QString errorString;
    MarineTask loaded;
    QVERIFY(!MarineTaskJsonCodec::load(json, loaded, errorString));
    QCOMPARE(errorString, QStringLiteral("Unsupported marine task version"));
}

void MarineTaskJsonTest::_testMissingId()
{
    QJsonObject json = saveTask(createTask());
    json.remove("id");

    QString errorString;
    MarineTask loaded;
    QVERIFY(!MarineTaskJsonCodec::load(json, loaded, errorString));
    QVERIFY(!errorString.isEmpty());
}

void MarineTaskJsonTest::_testMissingRegion()
{
    QJsonObject json = saveTask(createTask());
    json.remove("region");

    QString errorString;
    MarineTask loaded;
    QVERIFY(!MarineTaskJsonCodec::load(json, loaded, errorString));
    QVERIFY(!errorString.isEmpty());
}

UT_REGISTER_TEST_LIGHTWEIGHT(MarineTaskJsonTest, TestLabel::Unit)
