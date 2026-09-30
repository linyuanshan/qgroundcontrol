#include "MarineTaskModelTest.h"

#include <QtCore/QRegularExpression>

#include <cmath>
#include <limits>
#include <tuple>

#include "MarineTask.h"
#include "MarineTaskJsonCodec.h"

using namespace Marine;

void MarineTaskModelTest::_testDefaults()
{
    const MarineTask task;

    QVERIFY(std::isnan(task.planner.executionSafety.executionMarginM));

    QVERIFY(task.type == MarineTaskType::CoverageInspection);
    QVERIFY(task.name.empty());
    QVERIFY(task.vehicleId.empty());
    QCOMPARE(task.planner.plannerId, std::string("marine.coverage.auto"));
    QCOMPARE(task.coverage.swathWidthM, 0.0);
    QVERIFY(task.coverage.coverageRequirement == CoverageRequirement::Standard);
    QVERIFY(std::isnan(task.safety.hardSafetyMarginM));
    QVERIFY(std::isnan(task.safety.preferredSafetyMarginM));
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
    task.region.coverageBoundary.vertices.push_back({47.0, 8.0, 0.0});

    QVERIFY(task.id == originalId);
}

void MarineTaskModelTest::_testWorkRegion()
{
    MarineTask task;
    task.region.coverageBoundary.vertices = {
        {47.0, 8.0, 0.0},
        {47.0, 8.1, 0.0},
        {47.1, 8.1, 0.0},
    };
    task.region.navigationBoundary = task.region.coverageBoundary;
    task.region.noGoRegions.push_back({{
        {47.02, 8.02, 0.0},
        {47.02, 8.03, 0.0},
        {47.03, 8.03, 0.0},
    }});

    QCOMPARE(task.region.coverageBoundary.vertices.size(), std::size_t{3});
    QCOMPARE(task.region.navigationBoundary.vertices.size(), std::size_t{3});
    QCOMPARE(task.region.noGoRegions.size(), std::size_t{1});
    QCOMPARE(task.region.noGoRegions.front().vertices.size(), std::size_t{3});
    QCOMPARE(task.region.noGoRegions.front().vertices.front().latitudeDeg, 47.02);
}

void MarineTaskModelTest::_testConfiguration()
{
    MarineTask task;
    task.vehicleId = "usv-1";
    task.coverage.swathWidthM = 4.5;
    task.safety.hardSafetyMarginM = 1.25;
    task.safety.preferredSafetyMarginM = 2.0;
    task.coverage.coverageRequirement = CoverageRequirement::Strict;
    task.coverage.sweepAngleMode = SweepAngleMode::Manual;
    task.coverage.sweepAngleDeg = 75.0;
    task.sensors.cameraEnabled = false;
    task.sensors.cameraRecord = false;
    task.sensors.sonarEnabled = true;
    task.sensors.sonarRecord = false;

    QVERIFY(task.vehicleId == "usv-1");
    QCOMPARE(task.coverage.swathWidthM, 4.5);
    QCOMPARE(task.safety.hardSafetyMarginM, 1.25);
    QCOMPARE(task.safety.preferredSafetyMarginM, 2.0);
    QVERIFY(task.coverage.coverageRequirement == CoverageRequirement::Strict);
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
    task.region.coverageBoundary.vertices.push_back({47.0, 8.0, 0.0});
    task.region.coverageBoundary.vertices.push_back({47.0, 8.1, 0.0});
    QVERIFY(!task.isValid());
    task.region.coverageBoundary.vertices.push_back({47.1, 8.1, 0.0});
    QVERIFY(!task.isValid());
    task.safety.hardSafetyMarginM = 0.0;
    task.safety.preferredSafetyMarginM = 0.0;
    task.planner.executionSafety.executionMarginM = 0.0;
    QVERIFY(task.isValid());
    QVERIFY(!task.schemaValid());
    task.region.navigationBoundary = task.region.coverageBoundary;
    task.coverage.swathWidthM = 5.0;
    QVERIFY(task.schemaValid());

    task.id.clear();
    QVERIFY(!task.isValid());
}

void MarineTaskModelTest::_testExecutionMarginMustBeConfigured()
{
    MarineTask task;
    task.region.coverageBoundary.vertices = {{47, 8, 0}, {47, 8.1, 0}, {47.1, 8.1, 0}};
    task.region.navigationBoundary = task.region.coverageBoundary;
    task.coverage.swathWidthM = 5;
    QVERIFY(std::isnan(task.safety.hardSafetyMarginM));
    QVERIFY(std::isnan(task.safety.preferredSafetyMarginM));
    QVERIFY(std::isnan(task.planner.executionSafety.executionMarginM));
    QVERIFY(!task.isValid());
    QVERIFY(!task.schemaValid());
    QJsonObject object;
    QString error;
    QVERIFY(!MarineTaskJsonCodec::save(task, object, error));
    task.planner.executionSafety.executionMarginM = 0.0;
    QVERIFY(!task.schemaValid());
    QVERIFY(!MarineTaskJsonCodec::save(task, object, error));
    for (const auto [explicitH, explicitP, explicitE] : {
             std::tuple{0.0, 0.0, 0.0},
             std::tuple{1.0, 1.0, 0.0},
             std::tuple{1.0, 2.0, 0.25},
         }) {
        task.safety.hardSafetyMarginM = explicitH;
        task.safety.preferredSafetyMarginM = explicitP;
        task.planner.executionSafety.executionMarginM = explicitE;
        QVERIFY(task.isValid());
        QVERIFY(task.schemaValid());
        QVERIFY2(MarineTaskJsonCodec::save(task, object, error), qPrintable(error));
        QCOMPARE(
            object.value("planner").toObject().value("executionSafety").toObject().value("executionMarginM").toDouble(),
            explicitE);
        MarineTask restored;
        QVERIFY(std::isnan(restored.safety.hardSafetyMarginM));
        QVERIFY(std::isnan(restored.safety.preferredSafetyMarginM));
        QVERIFY(std::isnan(restored.planner.executionSafety.executionMarginM));
        QVERIFY2(MarineTaskJsonCodec::load(object, restored, error), qPrintable(error));
        QCOMPARE(restored.safety.hardSafetyMarginM, explicitH);
        QCOMPARE(restored.safety.preferredSafetyMarginM, explicitP);
        QCOMPARE(restored.planner.executionSafety.executionMarginM, explicitE);
        QVERIFY(restored.schemaValid());
    }

    const auto rejected = [&task](double h, double p, double e) {
        task.safety.hardSafetyMarginM = h;
        task.safety.preferredSafetyMarginM = p;
        task.planner.executionSafety.executionMarginM = e;
        return !task.isValid() && !task.schemaValid();
    };
    QVERIFY(rejected(2.0, 1.0, 0.0));
    QVERIFY(rejected(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0));
    QVERIFY(rejected(0.0, std::numeric_limits<double>::quiet_NaN(), 0.0));
    QVERIFY(rejected(0.0, 0.0, std::numeric_limits<double>::quiet_NaN()));
    QVERIFY(rejected(std::numeric_limits<double>::infinity(), 0.0, 0.0));
    QVERIFY(rejected(0.0, std::numeric_limits<double>::infinity(), 0.0));
    QVERIFY(rejected(0.0, 0.0, std::numeric_limits<double>::infinity()));
    QVERIFY(rejected(-1.0, 0.0, 0.0));
    QVERIFY(rejected(0.0, -1.0, 0.0));
    QVERIFY(rejected(0.0, 0.0, -1.0));
}

UT_REGISTER_TEST_LIGHTWEIGHT(MarineTaskModelTest, TestLabel::Unit)
