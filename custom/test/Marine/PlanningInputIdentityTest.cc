#include "PlanningInputIdentityTest.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <utility>

#include "Planning/CoverageStrategySemantics.h"
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
    QCOMPARE(identity->fingerprint, QStringLiteral("1ca08ed4daef1c9b516266caf053ed7369f8909e12a4338675115bfd201ed4a8"));
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
        auto changed = input();
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

void PlanningInputIdentityTest::_testPlanningAndResolvedSemanticFingerprints()
{
    const MarineTask task = input();
    const auto unresolved = PlanningInputIdentity::fromTask(task);
    QVERIFY(unresolved.has_value());
    QCOMPARE(unresolved->semantics.planningVersion,
             QString::fromLatin1(CoverageStrategySemantics::PlanningSemanticsVersion));
    QCOMPARE(unresolved->semantics.resolvedStrategy, QStringLiteral("unresolved"));
    QCOMPARE(unresolved->semantics.strategyVersion, QStringLiteral("unresolved"));

    PlanningSemantics semantics;
    semantics.resolvedStrategy = QString::fromLatin1(CoverageStrategySemantics::SimpleMonotoneId);
    semantics.strategyVersion = QString::fromLatin1(CoverageStrategySemantics::SimpleMonotoneVersion);
    const auto resolved = PlanningInputIdentity::fromTask(task, semantics);
    QVERIFY(resolved.has_value());
    QCOMPARE(resolved->fingerprint, QStringLiteral("ccaeefbca3d7fac593dfa4a686c7f0a1f10a00bec1927941290e3ce0f884f53b"));

    semantics.resolvedStrategy = QString::fromLatin1(CoverageStrategySemantics::BoustrophedonId);
    semantics.strategyVersion = QString::fromLatin1(CoverageStrategySemantics::BoustrophedonVersion);
    const auto bcdResolved = PlanningInputIdentity::fromTask(task, semantics);
    QVERIFY(bcdResolved.has_value());
    QCOMPARE(bcdResolved->fingerprint,
             QStringLiteral("548e77c9c6d0f59cea9eac2905c9f8669c4b735eccd6bc336b606a837e2b5235"));
}

void PlanningInputIdentityTest::_testSupportedSemanticMatching()
{
    const MarineTask task = input();
    for (const auto [strategyId, version] :
         {std::pair{QString::fromLatin1(CoverageStrategySemantics::SimpleMonotoneId),
                    QString::fromLatin1(CoverageStrategySemantics::SimpleMonotoneVersion)},
          std::pair{QString::fromLatin1(CoverageStrategySemantics::BoustrophedonId),
                    QString::fromLatin1(CoverageStrategySemantics::BoustrophedonVersion)},
          std::pair{QString::fromLatin1(CoverageStrategySemantics::LawnmowerId),
                    QString::fromLatin1(CoverageStrategySemantics::LawnMowerVersion)},
          std::pair{QString::fromLatin1(CoverageStrategySemantics::MockPlannerId),
                    QString::fromLatin1(CoverageStrategySemantics::MockVersion)}}) {
        PlanningSemantics semantics;
        semantics.resolvedStrategy = strategyId;
        semantics.strategyVersion = version;
        const auto identity = PlanningInputIdentity::fromTask(task, semantics);
        QVERIFY(identity.has_value());
        QVERIFY2(identity->matchesSupported(task),
                 qPrintable(strategyId + QLatin1Char('@') + version));
    }

    for (const QString& unsupportedVersion :
         {QString::fromLatin1(CoverageStrategySemantics::LegacyBoustrophedonVersion),
          QString::fromLatin1(CoverageStrategySemantics::BoustrophedonPendingVersion)}) {
        PlanningSemantics unsupportedBcd;
        unsupportedBcd.resolvedStrategy = QString::fromLatin1(CoverageStrategySemantics::BoustrophedonId);
        unsupportedBcd.strategyVersion = unsupportedVersion;
        const auto identity = PlanningInputIdentity::fromTask(task, unsupportedBcd);
        QVERIFY(identity.has_value());
        QVERIFY(!identity->matchesSupported(task));
    }

    PlanningSemantics unsupported;
    unsupported.resolvedStrategy = QString::fromLatin1(CoverageStrategySemantics::SimpleMonotoneId);
    unsupported.strategyVersion = QStringLiteral("simple-monotone.future");
    auto identity = PlanningInputIdentity::fromTask(task, unsupported);
    QVERIFY(identity.has_value());
    QVERIFY(!identity->matchesSupported(task));

    PlanningSemantics oldPlanning;
    oldPlanning.planningVersion = QStringLiteral("p2.v0.5.infrastructure.1");
    oldPlanning.resolvedStrategy = QString::fromLatin1(CoverageStrategySemantics::SimpleMonotoneId);
    oldPlanning.strategyVersion = QString::fromLatin1(CoverageStrategySemantics::SimpleMonotoneVersion);
    identity = PlanningInputIdentity::fromTask(task, oldPlanning);
    QVERIFY(identity.has_value());
    QVERIFY(!identity->matchesSupported(task));
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
