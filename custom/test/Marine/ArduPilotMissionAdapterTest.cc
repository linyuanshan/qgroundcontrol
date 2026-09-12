#include "ArduPilotMissionAdapterTest.h"

#include <QtCore/QList>
#include <QtCore/QObject>

#include <cmath>

#include "ArduPilotMissionAdapter.h"
#include "MissionItem.h"

using namespace Marine;

namespace {

PlanningResult validResult()
{
    PlanningResult result;
    result.status = PlanningStatus::Success;
    result.path = {
        {.latitudeDeg = 38.1, .longitudeDeg = 121.1, .altitudeM = 0.0},
        {.latitudeDeg = 38.2, .longitudeDeg = 121.2, .altitudeM = 1.5},
        {.latitudeDeg = 38.3, .longitudeDeg = 121.3, .altitudeM = 3.0},
    };
    return result;
}

}  // namespace

void ArduPilotMissionAdapterTest::_testAppendWaypoints()
{
    QObject parent;
    QList<MissionItem*> items;
    int sequenceNumber = 7;
    QString errorString = QStringLiteral("stale error");
    const PlanningResult result = validResult();

    QVERIFY2(ArduPilotMissionAdapter::appendWaypoints(result, items, &parent, sequenceNumber, errorString),
             qPrintable(errorString));

    QCOMPARE(errorString, QString());
    QCOMPARE(items.size(), 3);
    QCOMPARE(sequenceNumber, 10);
    for (int index = 0; index < items.size(); ++index) {
        const MissionItem* item = items.at(index);
        QCOMPARE(item->sequenceNumber(), 7 + index);
        QCOMPARE(item->command(), MAV_CMD_NAV_WAYPOINT);
        QCOMPARE(item->frame(), MAV_FRAME_GLOBAL_RELATIVE_ALT);
        QCOMPARE(item->param1(), 0.0);
        QCOMPARE(item->param2(), 0.0);
        QCOMPARE(item->param3(), 0.0);
        QVERIFY(std::isnan(item->param4()));
        QCOMPARE(item->param5(), result.path.at(static_cast<std::size_t>(index)).latitudeDeg);
        QCOMPARE(item->param6(), result.path.at(static_cast<std::size_t>(index)).longitudeDeg);
        QCOMPARE(item->param7(), 0.0);
        QVERIFY(item->autoContinue());
        QVERIFY(!item->isCurrentItem());
        QCOMPARE(item->parent(), &parent);
    }
}

void ArduPilotMissionAdapterTest::_testRejectsUnsuccessfulResult()
{
    QObject parent;
    QList<MissionItem*> items;
    items.append(new MissionItem(&parent));
    PlanningResult result = validResult();
    result.status = PlanningStatus::Failed;
    int sequenceNumber = 4;
    QString errorString;

    QVERIFY(!ArduPilotMissionAdapter::appendWaypoints(result, items, &parent, sequenceNumber, errorString));
    QCOMPARE(items.size(), 1);
    QCOMPARE(sequenceNumber, 4);
    QVERIFY(!errorString.isEmpty());
}

void ArduPilotMissionAdapterTest::_testRejectsInvalidPath()
{
    QObject parent;
    QList<MissionItem*> items;
    int sequenceNumber = 4;
    QString errorString;
    PlanningResult result = validResult();
    result.path.clear();

    QVERIFY(!ArduPilotMissionAdapter::appendWaypoints(result, items, &parent, sequenceNumber, errorString));
    QVERIFY(items.isEmpty());
    QCOMPARE(sequenceNumber, 4);
    QVERIFY(!errorString.isEmpty());

    result = validResult();
    result.path.at(1).latitudeDeg = 91.0;
    errorString.clear();
    QVERIFY(!ArduPilotMissionAdapter::appendWaypoints(result, items, &parent, sequenceNumber, errorString));
    QVERIFY(items.isEmpty());
    QCOMPARE(sequenceNumber, 4);
    QVERIFY(!errorString.isEmpty());
}

UT_REGISTER_TEST(ArduPilotMissionAdapterTest, TestLabel::Unit, TestLabel::MissionManager)
