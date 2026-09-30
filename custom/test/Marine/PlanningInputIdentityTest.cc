#include "PlanningInputIdentityTest.h"

#include <algorithm>
#include <functional>
#include <limits>

#include "PlanningInputIdentity.h"

using namespace Marine;

namespace {

MarineTask input()
{
    MarineTask task;
    task.planner.executionSafety.executionMarginM = 0.0;
    task.region.coverageBoundary.vertices = {{1, 2, 0}, {1, 3, 0}, {2, 3, 0}, {2, 2, 0}};
    task.region.navigationBoundary = task.region.coverageBoundary;
    task.region.noGoRegions = {{{{1.1, 2.1, 0}, {1.1, 2.2, 0}, {1.2, 2.2, 0}}},
                               {{{1.3, 2.3, 0}, {1.3, 2.4, 0}, {1.4, 2.4, 0}}}};
    task.coverage.swathWidthM = 5;
    task.coverage.sweepAngleMode = SweepAngleMode::Manual;
    task.coverage.sweepAngleDeg = 30;
    task.safety.hardSafetyMarginM = 1;
    task.safety.preferredSafetyMarginM = 2;
    return task;
}

}  // namespace

void PlanningInputIdentityTest::_testPlanningFields()
{
    const auto task = input();
    const auto identity = PlanningInputIdentity::fromTask(task);
    QVERIFY(identity.has_value());
    QCOMPARE(identity->fingerprint, QStringLiteral("af1f2fef58fc5e2ad31cfc2c8d926752fa57631adc7cb56a13e31ce1b5e0941e"));
    QCOMPARE(PlanningInputIdentity::fromTask(task)->fingerprint, identity->fingerprint);
    const std::vector<std::function<void(MarineTask&)>> changes = {
        [](auto& t) { t.region.coverageBoundary.vertices[0].latitudeDeg += 0.01; },
        [](auto& t) { t.region.navigationBoundary.vertices[0].longitudeDeg += 0.01; },
        [](auto& t) { t.region.noGoRegions[0].vertices[0].latitudeDeg += 0.01; },
        [](auto& t) { t.region.noGoRegions[1].vertices[0].longitudeDeg += 0.01; },
        [](auto& t) { t.region.noGoRegions.pop_back(); },
        [](auto& t) { t.coverage.swathWidthM += 1; },
        [](auto& t) { t.safety.hardSafetyMarginM += 0.1; },
        [](auto& t) { t.safety.preferredSafetyMarginM += 1; },
        [](auto& t) { t.planner.executionSafety.executionMarginM += 1; },
        [](auto& t) { t.coverage.coverageRequirement = CoverageRequirement::Strict; },
        [](auto& t) { t.coverage.sweepAngleMode = SweepAngleMode::Auto; },
        [](auto& t) { t.coverage.sweepAngleDeg += 1; },
        [](auto& t) { t.planner.plannerId = "another.planner"; },
    };
    for (const auto& change : changes) {
        auto changed = task;
        change(changed);
        const auto other = PlanningInputIdentity::fromTask(changed);
        QVERIFY(other.has_value());
        QVERIFY(other->fingerprint != identity->fingerprint);
        QVERIFY(!identity->matches(changed));
    }
}

void PlanningInputIdentityTest::_testCanonicalGeometry()
{
    auto task = input();
    const auto identity = PlanningInputIdentity::fromTask(task);
    const auto permute = [](GeoPolygon& p) {
        std::ranges::reverse(p.vertices);
        std::rotate(p.vertices.begin(), p.vertices.begin() + 1, p.vertices.end());
        p.vertices.push_back(p.vertices.front());
    };
    permute(task.region.coverageBoundary);
    permute(task.region.navigationBoundary);
    for (auto& polygon : task.region.noGoRegions) {
        permute(polygon);
    }
    std::ranges::reverse(task.region.noGoRegions);
    QVERIFY(identity->matches(task));
    task.coverage.sweepAngleDeg += 360;
    QVERIFY(identity->matches(task));
    task.planner.executionSafety.executionMarginM = -0.0;
    QVERIFY(identity->matches(task));
}

void PlanningInputIdentityTest::_testNonPlanningFields()
{
    auto task = input();
    const auto identity = PlanningInputIdentity::fromTask(task);
    task.name = "renamed";
    task.sensors = {false, false, false, false};
    QVERIFY(identity->matches(task));
    task.coverage.sweepAngleMode = SweepAngleMode::Auto;
    const auto automatic = PlanningInputIdentity::fromTask(task);
    task.coverage.sweepAngleDeg = -145;
    QVERIFY(automatic->matches(task));
}

void PlanningInputIdentityTest::_testSemanticIdentity()
{
    const auto task = input();
    const auto baseline = PlanningInputIdentity::fromTask(task);
    QCOMPARE(baseline->semantics.policyVersion, QStringLiteral("coverage-quality.v1"));
    const std::vector<std::function<void(PlanningSemantics&)>> changes = {
        [](auto& s) { s.planningVersion = "p2.v0.5.final"; },
        [](auto& s) { s.policyVersion = "approved-calibration"; },
        [](auto& s) { s.resolvedStrategy = "SimpleMonotone"; },
        [](auto& s) { s.strategyVersion = "1"; },
    };
    for (const auto& change : changes) {
        PlanningSemantics semantics;
        change(semantics);
        const auto identity = PlanningInputIdentity::fromTask(task, semantics);
        QVERIFY(identity.has_value());
        QVERIFY(identity->fingerprint != baseline->fingerprint);
        QVERIFY(!identity->matches(task));
        QVERIFY(identity->matches(task, semantics));
        PlanningInputIdentity loaded;
        QString error;
        QVERIFY(PlanningInputIdentity::fromJson(identity->toJson(), loaded, error));
        QVERIFY(loaded == *identity);
    }
}

void PlanningInputIdentityTest::_testInvalidIdentity()
{
    const auto valid = PlanningInputIdentity::fromTask(input());
    for (const QString& key : valid->toJson().keys()) {
        auto json = valid->toJson();
        json.remove(key);
        PlanningInputIdentity loaded = *valid;
        QString error;
        QVERIFY(!PlanningInputIdentity::fromJson(json, loaded, error));
        QVERIFY(loaded == *valid);
    }
    auto json = valid->toJson();
    json.insert("encodingVersion", 2);
    PlanningInputIdentity loaded;
    QString error;
    QVERIFY(PlanningInputIdentity::fromJson(json, loaded, error));
    QVERIFY(!loaded.matches(input()));
    json.insert("fingerprint", QString(64, QLatin1Char('z')));
    QVERIFY(!PlanningInputIdentity::fromJson(json, loaded, error));
    auto task = input();
    task.region.navigationBoundary.vertices[0].latitudeDeg = std::numeric_limits<double>::quiet_NaN();
    QVERIFY(!PlanningInputIdentity::fromTask(task));
}

UT_REGISTER_TEST_LIGHTWEIGHT(PlanningInputIdentityTest, TestLabel::Unit)
