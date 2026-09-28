#include "MarineTaskJsonTest.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QStringList>

#include <limits>
#include <utility>

#include "MarineTaskJsonCodec.h"

using namespace Marine;

namespace {

MarineTask createTask()
{
    MarineTask task;
    task.id = "9a20e6c8-1234-4567-89ab-123456789abc";
    task.name = "Harbor inspection";
    task.vehicleId = "usv-1";
    task.region.coverageBoundary.vertices = {
        {38.1, 121.1, 0.0},
        {38.1, 121.2, 0.0},
        {38.2, 121.2, 0.0},
    };
    task.region.navigationBoundary.vertices = {
        {38.0, 121.0, 0.0},
        {38.0, 121.3, 0.0},
        {38.3, 121.3, 0.0},
    };
    task.coverage.swathWidthM = 10.0;
    task.coverage.coverageRequirement = CoverageRequirement::Strict;
    task.safety.hardSafetyMarginM = 2.0;
    task.safety.preferredSafetyMarginM = 3.0;
    task.coverage.sweepAngleMode = SweepAngleMode::Manual;
    task.coverage.sweepAngleDeg = 37.5;
    task.planner.plannerId = "marine.coverage.mock";
    task.planner.executionSafety.executionMarginM = 0.25;
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

    QCOMPARE(json.value("version").toInt(), 3);
    QCOMPARE(json.value("type").toString(), QStringLiteral("coverageInspection"));
    QVERIFY(loaded.id == source.id);
    QVERIFY(loaded.name == source.name);
    QVERIFY(loaded.type == source.type);
    QVERIFY(loaded.vehicleId == source.vehicleId);
    QCOMPARE(loaded.region.coverageBoundary.vertices.size(), source.region.coverageBoundary.vertices.size());
    QCOMPARE(loaded.region.coverageBoundary.vertices[1].latitudeDeg, 38.1);
    QCOMPARE(loaded.region.coverageBoundary.vertices[1].longitudeDeg, 121.2);
    QCOMPARE(loaded.region.navigationBoundary.vertices[1].longitudeDeg, 121.3);
    QCOMPARE(loaded.coverage.swathWidthM, source.coverage.swathWidthM);
    QCOMPARE(loaded.coverage.coverageRequirement, CoverageRequirement::Strict);
    QCOMPARE(loaded.safety.hardSafetyMarginM, 2.0);
    QCOMPARE(loaded.safety.preferredSafetyMarginM, 3.0);
    QCOMPARE(loaded.coverage.sweepAngleMode, source.coverage.sweepAngleMode);
    QCOMPARE(loaded.coverage.sweepAngleDeg, source.coverage.sweepAngleDeg);
    QVERIFY(loaded.planner.plannerId == source.planner.plannerId);
    QCOMPARE(loaded.planner.executionSafety.executionMarginM, 0.25);
    QCOMPARE(json.value("planner").toObject().value("executionSafety").toObject().value("executionMarginM").toDouble(),
             0.25);
    QCOMPARE(json.value("planner").toObject().value("plannerId").toString(), QStringLiteral("marine.coverage.mock"));
    QCOMPARE(saveTask(loaded), json);
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
    for (const int version : {1, 2}) {
        QJsonObject json = saveTask(createTask());
        json.insert("version", version);
        MarineTask loaded = createTask();
        const std::string oldId = loaded.id;
        QString errorString;
        QVERIFY(!MarineTaskJsonCodec::load(json, loaded, errorString));
        QVERIFY(errorString.contains(QStringLiteral("Unsupported development schema")));
        QVERIFY(errorString.contains(QString::number(version)));
        QCOMPARE(loaded.id, oldId);
    }
}

void MarineTaskJsonTest::_testMissingRequiredField_data()
{
    QTest::addColumn<QStringList>("path");
    const QList<QStringList> paths = {
        {"version"},
        {"id"},
        {"type"},
        {"name"},
        {"vehicleId"},
        {"region"},
        {"coverage"},
        {"safety"},
        {"planner"},
        {"sensors"},
        {"region", "coverageBoundary"},
        {"region", "navigationBoundary"},
        {"region", "noGoRegions"},
        {"coverage", "swathWidthM"},
        {"coverage", "coverageRequirement"},
        {"coverage", "sweepAngleMode"},
        {"coverage", "sweepAngleDeg"},
        {"safety", "hardSafetyMarginM"},
        {"safety", "preferredSafetyMarginM"},
        {"planner", "plannerId"},
        {"planner", "executionSafety"},
        {"planner", "executionSafety", "executionMarginM"},
        {"sensors", "cameraEnabled"},
        {"sensors", "cameraRecord"},
        {"sensors", "sonarEnabled"},
        {"sensors", "sonarRecord"},
    };
    for (const QStringList& path : paths) {
        QTest::newRow(qPrintable(path.join('.'))) << path;
    }
}

void MarineTaskJsonTest::_testMissingRequiredField()
{
    QFETCH(QStringList, path);
    QJsonObject json = saveTask(createTask());
    if (path.size() == 1) {
        json.remove(path[0]);
    } else if (path.size() == 2) {
        QJsonObject nested = json.value(path[0]).toObject();
        nested.remove(path[1]);
        json.insert(path[0], nested);
    } else {
        QJsonObject planner = json.value(path[0]).toObject();
        QJsonObject nested = planner.value(path[1]).toObject();
        nested.remove(path[2]);
        planner.insert(path[1], nested);
        json.insert(path[0], planner);
    }
    MarineTask loaded = createTask();
    const std::string oldId = loaded.id;
    QString errorString;
    QVERIFY(!MarineTaskJsonCodec::load(json, loaded, errorString));
    QVERIFY(!errorString.isEmpty());
    QCOMPARE(loaded.id, oldId);
}

void MarineTaskJsonTest::_testInvalidField_data()
{
    QTest::addColumn<QStringList>("path");
    QTest::addColumn<QJsonValue>("value");
    const QList<std::pair<QStringList, QJsonValue>> cases = {
        {{"version"}, 3.5},
        {{"id"}, 7},
        {{"type"}, "unknown"},
        {{"region", "coverageBoundary"}, "polygon"},
        {{"region", "navigationBoundary"}, QJsonValue()},
        {{"region", "noGoRegions"}, QJsonObject{}},
        {{"coverage", "swathWidthM"}, 0.0},
        {{"coverage", "swathWidthM"}, -1.0},
        {{"coverage", "swathWidthM"}, "10"},
        {{"coverage", "coverageRequirement"}, "unknown"},
        {{"coverage", "sweepAngleMode"}, "unknown"},
        {{"coverage", "sweepAngleDeg"}, "90"},
        {{"safety", "hardSafetyMarginM"}, -1.0},
        {{"safety", "preferredSafetyMarginM"}, 1.0},
        {{"planner", "plannerId"}, QJsonObject{}},
        {{"planner", "plannerId"}, ""},
        {{"planner", "executionSafety"}, QJsonValue()},
        {{"planner", "executionSafety", "executionMarginM"}, -0.1},
        {{"planner", "executionSafety", "executionMarginM"}, "0.25"},
        {{"sensors", "cameraEnabled"}, 1},
        {{"sensors", "cameraRecord"}, "false"},
        {{"sensors", "sonarEnabled"}, 0},
        {{"sensors", "sonarRecord"}, QJsonValue()},
    };
    for (qsizetype index = 0; index < cases.size(); ++index) {
        QTest::newRow(qPrintable(QString::number(index))) << cases[index].first << cases[index].second;
    }
}

void MarineTaskJsonTest::_testInvalidField()
{
    QFETCH(QStringList, path);
    QFETCH(QJsonValue, value);
    QJsonObject json = saveTask(createTask());
    if (path.size() == 1) {
        json.insert(path[0], value);
    } else if (path.size() == 2) {
        QJsonObject nested = json.value(path[0]).toObject();
        nested.insert(path[1], value);
        json.insert(path[0], nested);
    } else {
        QJsonObject planner = json.value(path[0]).toObject();
        QJsonObject nested = planner.value(path[1]).toObject();
        nested.insert(path[2], value);
        planner.insert(path[1], nested);
        json.insert(path[0], planner);
    }
    MarineTask loaded = createTask();
    const std::string oldId = loaded.id;
    QString errorString;
    QVERIFY(!MarineTaskJsonCodec::load(json, loaded, errorString));
    QVERIFY(!errorString.isEmpty());
    QCOMPARE(loaded.id, oldId);
}

void MarineTaskJsonTest::_testInvalidSave()
{
    const auto checkRejected = [](const MarineTask& invalid) {
        QJsonObject saved{{"unchanged", true}};
        QString errorString;
        const bool result = MarineTaskJsonCodec::save(invalid, saved, errorString);
        return !result && !errorString.isEmpty() && saved.value("unchanged").toBool() && saved.size() == 1;
    };
    MarineTask invalid = createTask();
    invalid.coverage.swathWidthM = 0.0;
    QVERIFY(checkRejected(invalid));
    invalid = createTask();
    invalid.coverage.swathWidthM = std::numeric_limits<double>::infinity();
    QVERIFY(checkRejected(invalid));
    invalid = createTask();
    invalid.safety.hardSafetyMarginM = -0.1;
    QVERIFY(checkRejected(invalid));
    invalid = createTask();
    invalid.safety.preferredSafetyMarginM = 1.0;
    QVERIFY(checkRejected(invalid));
    invalid = createTask();
    invalid.coverage.sweepAngleDeg = std::numeric_limits<double>::quiet_NaN();
    QVERIFY(checkRejected(invalid));
    invalid = createTask();
    invalid.planner.executionSafety.executionMarginM = std::numeric_limits<double>::infinity();
    QVERIFY(checkRejected(invalid));
    invalid = createTask();
    invalid.region.navigationBoundary.vertices[0].latitudeDeg = 91.0;
    QVERIFY(checkRejected(invalid));
    invalid = createTask();
    invalid.coverage.coverageRequirement = static_cast<CoverageRequirement>(99);
    QVERIFY(checkRejected(invalid));
}

void MarineTaskJsonTest::_testManualAngleNormalization()
{
    QJsonObject json = saveTask(createTask());
    QJsonObject coverage = json.value("coverage").toObject();
    coverage.insert("sweepAngleDeg", -10.0);
    json.insert("coverage", coverage);
    const MarineTask loaded = loadTask(json);
    QCOMPARE(loaded.coverage.sweepAngleDeg, 170.0);
    QCOMPARE(saveTask(loaded).value("coverage").toObject().value("sweepAngleDeg").toDouble(), 170.0);
}

void MarineTaskJsonTest::_testInvalidCoordinates()
{
    QJsonObject json = saveTask(createTask());
    QJsonObject region = json.value("region").toObject();
    QJsonArray polygon = region.value("navigationBoundary").toArray();
    QJsonObject point = polygon[0].toObject();
    point.insert("lat", 91.0);
    polygon[0] = point;
    region.insert("navigationBoundary", polygon);
    json.insert("region", region);

    MarineTask loaded = createTask();
    const std::string oldId = loaded.id;
    QString errorString;
    QVERIFY(!MarineTaskJsonCodec::load(json, loaded, errorString));
    QVERIFY(errorString.contains(QStringLiteral("coordinate")));
    QCOMPARE(loaded.id, oldId);
}

void MarineTaskJsonTest::_testNoTopologyValidation()
{
    MarineTask source = createTask();
    source.region.navigationBoundary.vertices = {
        {39.0, 122.0, 0.0},
        {39.0, 122.1, 0.0},
        {39.1, 122.1, 0.0},
    };
    const MarineTask loaded = loadTask(saveTask(source));
    QCOMPARE(loaded.region.coverageBoundary.vertices[0].latitudeDeg, 38.1);
    QCOMPARE(loaded.region.navigationBoundary.vertices[0].latitudeDeg, 39.0);
}

UT_REGISTER_TEST_LIGHTWEIGHT(MarineTaskJsonTest, TestLabel::Unit)
