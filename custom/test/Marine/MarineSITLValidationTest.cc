#include "MarineSITLValidationTest.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
#include <QtCore/QEventLoop>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QPointer>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>
#include <QtCore/QtEndian>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "CoverageInspectionComplexItem.h"
#include "CoverageInspectionPlanCreator.h"
#include "Fact.h"
#include "FirmwarePlugin.h"
#include "Geometry/GeoReference.h"
#include "Geometry/MarineGeometry.h"
#include "LinkManager.h"
#include "MAVLinkSigning.h"
#include "MarinePlanContext.h"
#include "MissionController.h"
#include "MissionItem.h"
#include "MissionManager.h"
#include "MultiVehicleManager.h"
#include "ParameterManager.h"
#include "PlanMasterController.h"
#include "Planning/BoustrophedonCoveragePlanner.h"
#include "Planning/CoverageFreeSpace.h"
#include "Planning/CoverageTaskAdapter.h"
#include "Planning/NominalCoverageValidator.h"
#include "QmlObjectListModel.h"
#include "TCPLink.h"
#include "Vehicle.h"

using namespace Marine;

namespace {

constexpr int ConnectionTimeoutMs = 120000;
constexpr int MissionTimeoutMs = 900000;
constexpr double CoordinateToleranceDeg = 1e-10;
constexpr double MetricToleranceM = 1e-3;

struct ScenarioDefinition
{
    QString id;
    Polygon2D outer;
    std::vector<Polygon2D> noGoRegions;
    double swathWidthM = 4.0;
    double safetyMarginM = 1.0;
    SweepAngleMode sweepAngleMode = SweepAngleMode::Manual;
    double sweepAngleDeg = 90.0;
    int determinismRuns = 1;
};

struct TrajectorySample
{
    qint64 timestampMs = 0;
    QGeoCoordinate coordinate;
    QString mode;
    int missionSequence = -1;
};

struct MissionRunResult
{
    bool readyToFly = false;
    bool positionEstimateReady = false;
    bool started = false;
    bool enteredAuto = false;
    bool missionComplete = false;
    bool timedOut = false;
    QStringList statusTexts;
    QList<int> reachedSequences;
    QList<int> currentSequences;
    std::vector<TrajectorySample> trajectory;
    QString prearmError;
};

Polygon2D polygon(std::initializer_list<Point2D> vertices)
{
    return {std::vector<Point2D>(vertices)};
}

Polygon2D rectangle(double minimumX, double minimumY, double maximumX, double maximumY)
{
    return polygon({{minimumX, minimumY}, {maximumX, minimumY}, {maximumX, maximumY}, {minimumX, maximumY}});
}

std::vector<ScenarioDefinition> scenarios()
{
    return {
        {QStringLiteral("S01"),
         polygon({{0.0, 0.0}, {20.0, 0.0}, {20.0, 8.0}, {8.0, 8.0}, {8.0, 20.0}, {0.0, 20.0}}),
         {},
         4.0,
         1.0,
         SweepAngleMode::Manual,
         90.0,
         1},
        {QStringLiteral("S02"),
         polygon(
             {{0.0, 0.0}, {20.0, 0.0}, {20.0, 6.0}, {6.0, 6.0}, {6.0, 14.0}, {20.0, 14.0}, {20.0, 20.0}, {0.0, 20.0}}),
         {},
         4.0,
         1.0,
         SweepAngleMode::Auto,
         0.0,
         1},
        {QStringLiteral("S03"),
         rectangle(0.0, 0.0, 20.0, 20.0),
         {rectangle(8.0, 8.0, 12.0, 12.0)},
         4.0,
         1.0,
         SweepAngleMode::Manual,
         90.0,
         3},
        {QStringLiteral("S04"),
         polygon({{0.0, 0.0}, {30.0, 0.0}, {30.0, 20.0}, {18.0, 20.0}, {18.0, 10.0}, {0.0, 10.0}}),
         {rectangle(22.0, 4.0, 26.0, 8.0)},
         4.0,
         1.0,
         SweepAngleMode::Auto,
         0.0,
         1},
    };
}

CoveragePlanningProblem problemFor(const ScenarioDefinition& definition)
{
    CoveragePlanningProblem problem;
    problem.region.outerBoundary = definition.outer;
    problem.region.noGoRegions = definition.noGoRegions;
    problem.swathWidthM = definition.swathWidthM;
    problem.safetyMarginM = definition.safetyMarginM;
    problem.sweepAngleMode = definition.sweepAngleMode;
    problem.requestedSweepAngleDeg = definition.sweepAngleDeg;
    return problem;
}

void translatePolygon(Polygon2D& polygon, const Point2D& offset)
{
    for (Point2D& point : polygon.vertices) {
        point.xM += offset.xM;
        point.yM += offset.yM;
    }
}

CoveragePlanningProblem anchorFirstPathPoint(CoveragePlanningProblem problem, QString& error)
{
    const BoustrophedonCoveragePlanner planner;
    const CoveragePlanningSolution initial = planner.plan(problem);
    if ((initial.status != PlanningStatus::Success) || initial.path.empty()) {
        error = QStringLiteral("Unable to anchor scenario: %1").arg(QString::fromStdString(initial.message));
        return {};
    }

    const Point2D offset{-initial.path.front().xM, -initial.path.front().yM};
    translatePolygon(problem.region.outerBoundary, offset);
    for (Polygon2D& noGo : problem.region.noGoRegions) {
        translatePolygon(noGo, offset);
    }

    const CoveragePlanningSolution anchored = planner.plan(problem);
    if ((anchored.status != PlanningStatus::Success) || anchored.path.empty() ||
        (std::hypot(anchored.path.front().xM, anchored.path.front().yM) > MetricToleranceM)) {
        error = QStringLiteral("Anchored scenario does not start at the vehicle position");
        return {};
    }
    return problem;
}

GeoPolygon toGeoPolygon(const Polygon2D& polygon, const GeoReference& reference)
{
    GeoPolygon result;
    result.vertices.reserve(polygon.vertices.size());
    for (const Point2D& point : polygon.vertices) {
        const std::optional<GeoPoint> geoPoint = reference.toGeo(point);
        if (!geoPoint) {
            return {};
        }
        result.vertices.push_back(*geoPoint);
    }
    return result;
}

CoverageInspectionPlanCreator* coverageCreator(PlanMasterController& controller)
{
    QmlObjectListModel* creators = controller.planCreators();
    if (creators == nullptr) {
        return nullptr;
    }
    for (int index = 0; index < creators->count(); ++index) {
        if (auto* creator = creators->value<CoverageInspectionPlanCreator*>(index)) {
            return creator;
        }
    }
    return nullptr;
}

CoverageInspectionComplexItem* coverageItem(PlanMasterController& controller)
{
    QmlObjectListModel* visualItems = controller.missionController()->visualItems();
    if (visualItems == nullptr) {
        return nullptr;
    }
    for (int index = 0; index < visualItems->count(); ++index) {
        if (auto* item = visualItems->value<CoverageInspectionComplexItem*>(index)) {
            return item;
        }
    }
    return nullptr;
}

bool samePlanningResult(const PlanningResult& first, const PlanningResult& second)
{
    if ((first.status != second.status) || (first.legRoles != second.legRoles) ||
        (first.path.size() != second.path.size()) || (first.coverageLengthM != second.coverageLengthM) ||
        (first.transitLengthM != second.transitLengthM) || (first.pathLengthM != second.pathLengthM) ||
        (first.selectedSweepAngleDeg != second.selectedSweepAngleDeg) || (first.cellCount != second.cellCount) ||
        (first.turnCount != second.turnCount)) {
        return false;
    }
    for (std::size_t index = 0; index < first.path.size(); ++index) {
        if ((first.path[index].latitudeDeg != second.path[index].latitudeDeg) ||
            (first.path[index].longitudeDeg != second.path[index].longitudeDeg) ||
            (first.path[index].altitudeM != second.path[index].altitudeM)) {
            return false;
        }
    }
    return true;
}

QByteArray sha256(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return {};
    }
    return hash.result().toHex();
}

bool writeTrajectoryCsv(const QString& path, const std::vector<TrajectorySample>& samples)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }
    QTextStream stream(&file);
    stream << "timestamp_ms,latitude_deg,longitude_deg,mode,mission_sequence\n";
    for (const TrajectorySample& sample : samples) {
        stream << sample.timestampMs << ',' << QString::number(sample.coordinate.latitude(), 'f', 10) << ','
               << QString::number(sample.coordinate.longitude(), 'f', 10) << ',' << sample.mode << ','
               << sample.missionSequence << '\n';
    }
    return stream.status() == QTextStream::Ok;
}

double pointToSegmentDistance(const Point2D& point, const Point2D& first, const Point2D& second)
{
    const double dx = second.xM - first.xM;
    const double dy = second.yM - first.yM;
    const double lengthSquared = (dx * dx) + (dy * dy);
    if (lengthSquared == 0.0) {
        return std::hypot(point.xM - first.xM, point.yM - first.yM);
    }
    const double projection =
        std::clamp(((point.xM - first.xM) * dx + (point.yM - first.yM) * dy) / lengthSquared, 0.0, 1.0);
    return std::hypot(point.xM - (first.xM + projection * dx), point.yM - (first.yM + projection * dy));
}

double pointToPolygonBoundaryDistance(const Point2D& point, const Polygon2D& polygon)
{
    double distance = std::numeric_limits<double>::infinity();
    Point2D previous = polygon.vertices.back();
    for (const Point2D& current : polygon.vertices) {
        distance = std::min(distance, pointToSegmentDistance(point, previous, current));
        previous = current;
    }
    return distance;
}

bool pointStrictlyInside(const Polygon2D& polygon, const Point2D& point)
{
    if (!Geometry::containsPoint(polygon, point) ||
        (pointToPolygonBoundaryDistance(point, polygon) <= Geometry::LengthEpsilonM)) {
        return false;
    }
    return true;
}

int roleRunCount(const std::vector<PathLegRole>& roles, PathLegRole requestedRole)
{
    int count = 0;
    std::optional<PathLegRole> previous;
    for (const PathLegRole role : roles) {
        if ((!previous || (*previous != role)) && (role == requestedRole)) {
            ++count;
        }
        previous = role;
    }
    return count;
}

QPointF plotPoint(const Point2D& point, const QRectF& bounds, const QRectF& canvas)
{
    const double x = canvas.left() + ((point.xM - bounds.left()) / bounds.width()) * canvas.width();
    const double y = canvas.bottom() - ((point.yM - bounds.top()) / bounds.height()) * canvas.height();
    return {x, y};
}

QRectF plotBounds(const CoveragePlanningProblem& problem, const std::vector<Point2D>& actual)
{
    double minimumX = std::numeric_limits<double>::infinity();
    double minimumY = std::numeric_limits<double>::infinity();
    double maximumX = -std::numeric_limits<double>::infinity();
    double maximumY = -std::numeric_limits<double>::infinity();
    const auto include = [&](const Point2D& point) {
        minimumX = std::min(minimumX, point.xM);
        minimumY = std::min(minimumY, point.yM);
        maximumX = std::max(maximumX, point.xM);
        maximumY = std::max(maximumY, point.yM);
    };
    for (const Point2D& point : problem.region.outerBoundary.vertices) {
        include(point);
    }
    for (const Point2D& point : actual) {
        include(point);
    }
    const double padding = std::max(2.0, 0.08 * std::max(maximumX - minimumX, maximumY - minimumY));
    return {minimumX - padding, minimumY - padding, maximumX - minimumX + 2.0 * padding,
            maximumY - minimumY + 2.0 * padding};
}

void drawPolygon(QPainter& painter, const Polygon2D& polygon, const QRectF& bounds, const QRectF& canvas,
                 const QPen& pen, const QBrush& brush = Qt::NoBrush)
{
    QPolygonF points;
    for (const Point2D& point : polygon.vertices) {
        points << plotPoint(point, bounds, canvas);
    }
    painter.setPen(pen);
    painter.setBrush(brush);
    painter.drawPolygon(points);
}

bool renderEvidence(const QString& path, const QString& title, const CoveragePlanningProblem& problem,
                    const CoveragePlanningSolution& solution, const std::vector<Point2D>& actual)
{
    QImage image(1400, 1000, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::black);
    QFont titleFont = painter.font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.drawText(QRectF(30.0, 20.0, 1340.0, 45.0), Qt::AlignCenter, title);

    const QRectF canvas(70.0, 90.0, 1260.0, 830.0);
    const QRectF bounds = plotBounds(problem, actual);
    drawPolygon(painter, problem.region.outerBoundary, bounds, canvas, QPen(Qt::black, 3.0));
    for (const Polygon2D& noGo : problem.region.noGoRegions) {
        drawPolygon(painter, noGo, bounds, canvas, QPen(QColor(190, 0, 0), 3.0), QBrush(QColor(255, 210, 210)));
    }
    for (std::size_t index = 1; index < solution.path.size(); ++index) {
        const QColor color =
            solution.legRoles.at(index - 1) == PathLegRole::Coverage ? QColor(0, 145, 80) : QColor(115, 115, 115);
        painter.setPen(QPen(color, 3.0));
        painter.drawLine(plotPoint(solution.path[index - 1], bounds, canvas),
                         plotPoint(solution.path[index], bounds, canvas));
    }
    if (actual.size() >= 2) {
        painter.setPen(QPen(QColor(0, 90, 210), 3.0));
        for (std::size_t index = 1; index < actual.size(); ++index) {
            painter.drawLine(plotPoint(actual[index - 1], bounds, canvas), plotPoint(actual[index], bounds, canvas));
        }
    }
    painter.end();
    return image.save(path, "PNG");
}

bool writeJson(const QString& path, const QJsonObject& object)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(QJsonDocument(object).toJson(QJsonDocument::Indented)) > 0;
}

MissionRunResult executeMission(Vehicle* vehicle, int finalSequence, QFile& tlog)
{
    MissionRunResult result;
    int currentSequence = vehicle->missionManager()->currentIndex();
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);

    const QMetaObject::Connection messageConnection =
        QObject::connect(vehicle, &Vehicle::mavlinkMessageReceived, &loop, [&](const mavlink_message_t& message) {
            const QByteArray messageBytes = MAVLinkSigning::serializeUnsignedCopy(message);
            const quint64 timestamp = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()) * 1000U;
            QByteArray record(static_cast<qsizetype>(sizeof(timestamp)) + messageBytes.size(), Qt::Uninitialized);
            qToBigEndian(timestamp, reinterpret_cast<uchar*>(record.data()));
            std::memcpy(record.data() + sizeof(timestamp), messageBytes.constData(),
                        static_cast<std::size_t>(messageBytes.size()));
            (void) tlog.write(record);

            if (message.msgid == MAVLINK_MSG_ID_MISSION_ITEM_REACHED) {
                mavlink_mission_item_reached_t reached{};
                mavlink_msg_mission_item_reached_decode(&message, &reached);
                result.reachedSequences.append(static_cast<int>(reached.seq));
            } else if (message.msgid == MAVLINK_MSG_ID_EKF_STATUS_REPORT) {
                mavlink_ekf_status_report_t status{};
                mavlink_msg_ekf_status_report_decode(&message, &status);
                result.positionEstimateReady = (status.flags & EKF_POS_HORIZ_ABS) != 0;
            } else if (message.msgid == MAVLINK_MSG_ID_STATUSTEXT) {
                mavlink_statustext_t status{};
                mavlink_msg_statustext_decode(&message, &status);
                const QString text = QString::fromLatin1(status.text, static_cast<int>(sizeof(status.text))).trimmed();
                if (!text.isEmpty()) {
                    result.statusTexts.append(text);
                    if (text.contains(QStringLiteral("Mission Complete"), Qt::CaseInsensitive)) {
                        result.missionComplete = true;
                    }
                }
            }
            if (result.missionComplete && result.reachedSequences.contains(finalSequence)) {
                loop.quit();
            }
        });
    const QMetaObject::Connection currentConnection =
        QObject::connect(vehicle->missionManager(), &MissionManager::currentIndexChanged, &loop, [&](int sequence) {
            currentSequence = sequence;
            result.currentSequences.append(sequence);
        });
    const QMetaObject::Connection coordinateConnection =
        QObject::connect(vehicle, &Vehicle::coordinateChanged, &loop, [&](const QGeoCoordinate& coordinate) {
            if (coordinate.isValid()) {
                result.trajectory.push_back(
                    {QDateTime::currentMSecsSinceEpoch(), coordinate, vehicle->flightMode(), currentSequence});
            }
        });
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        result.timedOut = true;
        loop.quit();
    });

    if (vehicle->coordinate().isValid()) {
        result.trajectory.push_back(
            {QDateTime::currentMSecsSinceEpoch(), vehicle->coordinate(), vehicle->flightMode(), currentSequence});
    }
    QElapsedTimer readinessWait;
    readinessWait.start();
    while ((readinessWait.elapsed() < ConnectionTimeoutMs) &&
           (!vehicle->readyToFlyAvailable() || !vehicle->readyToFly() || !result.positionEstimateReady)) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        QTest::qWait(50);
    }
    result.readyToFly = vehicle->readyToFlyAvailable() && vehicle->readyToFly() && result.positionEstimateReady;
    result.prearmError = vehicle->prearmError();
    if (!result.readyToFly) {
        QObject::disconnect(messageConnection);
        QObject::disconnect(currentConnection);
        QObject::disconnect(coordinateConnection);
        (void) tlog.flush();
        return result;
    }

    timeout.start(MissionTimeoutMs);
    vehicle->startMission();
    result.started = true;

    QElapsedTimer modeWait;
    modeWait.start();
    while ((modeWait.elapsed() < ConnectionTimeoutMs) &&
           (!vehicle->armed() || (vehicle->flightMode() != vehicle->missionFlightMode()))) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        QTest::qWait(50);
    }
    result.enteredAuto = vehicle->armed() && (vehicle->flightMode() == vehicle->missionFlightMode());
    if (result.enteredAuto) {
        loop.exec();
    }

    timeout.stop();
    QObject::disconnect(messageConnection);
    QObject::disconnect(currentConnection);
    QObject::disconnect(coordinateConnection);
    (void) tlog.flush();
    return result;
}

}  // namespace

void MarineSITLValidationTest::init()
{
    UnitTest::init();
    MultiVehicleManager::instance()->init();
    LinkManager::instance()->setConnectionsAllowed();
}

void MarineSITLValidationTest::cleanup()
{
    LinkManager::instance()->disconnectAll();
    QElapsedTimer wait;
    wait.start();
    while ((MultiVehicleManager::instance()->activeVehicle() != nullptr) && (wait.elapsed() < ConnectionTimeoutMs)) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        QTest::qWait(50);
    }
    UnitTest::cleanup();
}

void MarineSITLValidationTest::_validateP2Scenarios()
{
    const QString evidenceRoot = qEnvironmentVariable("QGC_P2_SITL_EVIDENCE_DIR");
    if (evidenceRoot.isEmpty()) {
        QSKIP("Set QGC_P2_SITL_EVIDENCE_DIR to explicitly enable the live P2 SITL validation");
    }
    QVERIFY2(QDir().mkpath(evidenceRoot), qPrintable(QStringLiteral("Cannot create %1").arg(evidenceRoot)));
    QCOMPARE(LinkManager::instance()->links().count(), 0);

    auto* tcpConfiguration = new TCPConfiguration(QStringLiteral("P2-13 ArduRover SITL"));
    tcpConfiguration->setHost(QStringLiteral("127.0.0.1"));
    tcpConfiguration->setPort(5760);
    tcpConfiguration->setDynamic(true);
    SharedLinkConfigurationPtr sharedConfiguration(tcpConfiguration);
    QVERIFY2(LinkManager::instance()->createConnectedLink(sharedConfiguration), "Failed to connect TCP SITL link");

    QTRY_VERIFY_WITH_TIMEOUT(MultiVehicleManager::instance()->activeVehicle() != nullptr, ConnectionTimeoutMs);
    Vehicle* vehicle = MultiVehicleManager::instance()->activeVehicle();
    QVERIFY(vehicle != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(vehicle->isInitialConnectComplete(), ConnectionTimeoutMs);
    QVERIFY(vehicle->parameterManager() != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(vehicle->parameterManager()->parametersReady(), ConnectionTimeoutMs);
    QCOMPARE(vehicle->firmwareType(), MAV_AUTOPILOT_ARDUPILOTMEGA);
    QCOMPARE(vehicle->vehicleType(), MAV_TYPE_GROUND_ROVER);
    QTRY_VERIFY_WITH_TIMEOUT(vehicle->coordinate().isValid(), ConnectionTimeoutMs);

    QVERIFY(vehicle->parameterManager()->parameterExists(ParameterManager::defaultComponentId,
                                                         QStringLiteral("WP_RADIUS")));
    const double waypointRadiusM = vehicle->parameterManager()
                                       ->getParameter(ParameterManager::defaultComponentId, QStringLiteral("WP_RADIUS"))
                                       ->rawValue()
                                       .toDouble();

    QJsonObject baseline{{QStringLiteral("firmwareVersion"), QStringLiteral("%1.%2.%3")
                                                                 .arg(vehicle->firmwareMajorVersion())
                                                                 .arg(vehicle->firmwareMinorVersion())
                                                                 .arg(vehicle->firmwarePatchVersion())},
                         {QStringLiteral("wpRadiusM"), waypointRadiusM},
                         {QStringLiteral("tcpEndpoint"), QStringLiteral("127.0.0.1:5760")},
                         {QStringLiteral("recordedAtUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
    QVERIFY(writeJson(QDir(evidenceRoot).filePath(QStringLiteral("baseline.json")), baseline));

    for (const ScenarioDefinition& definition : scenarios()) {
        TEST_CONTEXT(QStringLiteral("P2-13 %1").arg(definition.id));
        const QString scenarioDirectory = QDir(evidenceRoot).filePath(definition.id);
        QVERIFY(QDir().mkpath(scenarioDirectory));

        QGeoCoordinate anchor = vehicle->coordinate();
        QVERIFY(anchor.isValid());
        const std::optional<GeoReference> anchorReference =
            GeoReference::create({anchor.latitude(), anchor.longitude(), 0.0});
        QVERIFY(anchorReference.has_value());

        QString anchorError;
        const CoveragePlanningProblem anchoredProblem = anchorFirstPathPoint(problemFor(definition), anchorError);
        QVERIFY2(anchorError.isEmpty(), qPrintable(anchorError));

        const QString planPath = QDir(scenarioDirectory).filePath(definition.id + QStringLiteral(".plan"));
        PlanningResult expectedResult;
        CoverageCompletenessResult completeness;
        CoveragePlanningSolution localSolution;
        QString taskId;

        {
            PlanMasterController controller(MAV_AUTOPILOT_ARDUPILOTMEGA, MAV_TYPE_GROUND_ROVER);
            controller.setFlyView(false);
            controller.start();
            CoverageInspectionPlanCreator* creator = coverageCreator(controller);
            QVERIFY(creator != nullptr);
            creator->createPlan(anchor);
            CoverageInspectionComplexItem* item = coverageItem(controller);
            QVERIFY(item != nullptr);
            MarinePlanContext* context =
                controller.findChild<MarinePlanContext*>(QString(), Qt::FindDirectChildrenOnly);
            QVERIFY(context != nullptr);
            taskId = item->taskId();
            const MarineTask* createdTask = context->task(taskId.toStdString());
            QVERIFY(createdTask != nullptr);
            MarineTask task = *createdTask;
            task.name = definition.id.toStdString() + " P2 SITL validation";
            task.planner.plannerId = "marine.coverage.bcd";
            task.region.outerBoundary = toGeoPolygon(anchoredProblem.region.outerBoundary, *anchorReference);
            task.region.noGoRegions.clear();
            for (const Polygon2D& noGo : anchoredProblem.region.noGoRegions) {
                task.region.noGoRegions.push_back(toGeoPolygon(noGo, *anchorReference));
            }
            task.coverage.swathWidthM = anchoredProblem.swathWidthM;
            task.coverage.safetyMarginM = anchoredProblem.safetyMarginM;
            task.coverage.sweepAngleMode = anchoredProblem.sweepAngleMode;
            task.coverage.sweepAngleDeg = anchoredProblem.requestedSweepAngleDeg;
            context->addTask(task);
            QCoreApplication::processEvents();

            PlanningResult deterministicResult;
            for (int run = 0; run < definition.determinismRuns; ++run) {
                QVERIFY2(item->plan(), qPrintable(item->planningMessage()));
                if (run == 0) {
                    deterministicResult = item->planningResult();
                } else {
                    QVERIFY2(samePlanningResult(item->planningResult(), deterministicResult),
                             "Repeated planning result changed");
                }
            }
            expectedResult = item->planningResult();
            QCOMPARE(expectedResult.status, PlanningStatus::Success);
            QVERIFY(expectedResult.path.size() >= 2);
            QCOMPARE(expectedResult.legRoles.size(), expectedResult.path.size() - 1);
            QVERIFY(expectedResult.coverageLengthM > 0.0);
            QVERIFY(expectedResult.transitLengthM >= 0.0);
            QVERIFY(std::abs(expectedResult.coverageLengthM + expectedResult.transitLengthM -
                             expectedResult.pathLengthM) <= MetricToleranceM);

            CoveragePlanningProblem adapterProblem;
            std::optional<GeoReference> adapterReference;
            CoveragePlanningError adapterError = CoveragePlanningError::None;
            QVERIFY(CoverageTaskAdapter::buildProblem(task, adapterProblem, adapterReference, adapterError));
            const BoustrophedonCoveragePlanner planner;
            localSolution = planner.plan(adapterProblem);
            QCOMPARE(localSolution.status, PlanningStatus::Success);
            const CoverageFreeSpaceResult freeSpace = buildCoverageFreeSpace(adapterProblem);
            QCOMPARE(freeSpace.status, PlanningStatus::Success);
            for (std::size_t index = 1; index < localSolution.path.size(); ++index) {
                QVERIFY(Geometry::segmentInsidePolygonRegionForValidatedGeometry(
                    freeSpace.freeSpace.trackFeasibleRegion, localSolution.path[index - 1], localSolution.path[index]));
            }
            completeness =
                validateNominalCoverage(PolygonRegionSet2D{freeSpace.freeSpace.coverageTarget}, localSolution.path,
                                        localSolution.legRoles, adapterProblem.swathWidthM);
            QCOMPARE(completeness.status, PlanningStatus::Success);
            QVERIFY(controller.saveToFile(planPath));
        }

        QVERIFY(QFile::exists(planPath));
        const QByteArray planHash = sha256(planPath);
        QVERIFY(!planHash.isEmpty());

        PlanMasterController restoredController;
        restoredController.setFlyView(false);
        restoredController.start();
        restoredController.loadFromFile(planPath);
        CoverageInspectionComplexItem* restoredItem = coverageItem(restoredController);
        QVERIFY(restoredItem != nullptr);
        QCOMPARE(restoredItem->taskId(), taskId);
        QCOMPARE(restoredItem->planningState(), CoverageInspectionComplexItem::Planned);
        QVERIFY2(samePlanningResult(restoredItem->planningResult(), expectedResult),
                 "Reloaded v2 planning artifact changed");

        QList<MissionItem*> missionItems;
        restoredItem->appendMissionItems(missionItems, vehicle);
        QCOMPARE(missionItems.size(), static_cast<qsizetype>(expectedResult.path.size()));
        for (int index = 0; index < missionItems.size(); ++index) {
            QCOMPARE(missionItems.at(index)->sequenceNumber(), index + 1);
            QCOMPARE(missionItems.at(index)->command(), MAV_CMD_NAV_WAYPOINT);
        }
        qDeleteAll(missionItems);
        missionItems.clear();

        QSignalSpy sendSpy(vehicle->missionManager(), &MissionManager::sendComplete);
        QVERIFY(sendSpy.isValid());
        restoredController.sendToVehicle();
        QVERIFY2(sendSpy.wait(ConnectionTimeoutMs), "Mission upload did not complete");
        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy.takeFirst().at(0).toBool(), false);
        QTRY_VERIFY_WITH_TIMEOUT(!restoredController.dirtyForUpload(), ConnectionTimeoutMs);

        const int missionSequenceOffset = vehicle->firmwarePlugin()->sendHomePositionToVehicle() ? 1 : 0;
        QCOMPARE(vehicle->missionManager()->missionItems().size(),
                 static_cast<qsizetype>(expectedResult.path.size()) + missionSequenceOffset);
        vehicle->setCurrentMissionSequence(missionSequenceOffset);
        QTRY_COMPARE_WITH_TIMEOUT(vehicle->missionManager()->currentIndex(), missionSequenceOffset,
                                  ConnectionTimeoutMs);

        const QString tlogPath = QDir(scenarioDirectory).filePath(definition.id + QStringLiteral(".tlog"));
        QFile tlog(tlogPath);
        QVERIFY(tlog.open(QIODevice::WriteOnly | QIODevice::Truncate));
        const MissionRunResult missionRun =
            executeMission(vehicle, static_cast<int>(expectedResult.path.size()) - 1 + missionSequenceOffset, tlog);
        tlog.close();
        QVERIFY2(missionRun.readyToFly,
                 qPrintable(QStringLiteral("Rover did not become ready to fly: %1").arg(missionRun.prearmError)));
        QVERIFY(missionRun.started);
        QVERIFY2(missionRun.enteredAuto, "Rover did not arm and enter AUTO");
        QVERIFY2(!missionRun.timedOut, "Timed out waiting for Mission Complete");
        QVERIFY2(missionRun.missionComplete, "ArduRover did not report Mission Complete");
        QVERIFY2(missionRun.reachedSequences.contains(static_cast<int>(expectedResult.path.size()) - 1 +
                                                      missionSequenceOffset),
                 "Final waypoint was not reported reached");

        QList<int> uniqueReached;
        for (const int sequence : missionRun.reachedSequences) {
            if (uniqueReached.isEmpty() || (uniqueReached.back() != sequence)) {
                uniqueReached.append(sequence);
            }
        }
        QCOMPARE(uniqueReached.size(), static_cast<qsizetype>(expectedResult.path.size()));
        for (int index = 0; index < uniqueReached.size(); ++index) {
            QCOMPARE(uniqueReached.at(index), index + missionSequenceOffset);
        }

        const QString csvPath = QDir(scenarioDirectory).filePath(definition.id + QStringLiteral("-trajectory.csv"));
        QVERIFY(writeTrajectoryCsv(csvPath, missionRun.trajectory));
        QVERIFY(!sha256(tlogPath).isEmpty());

        std::vector<Point2D> actualLocal;
        actualLocal.reserve(missionRun.trajectory.size());
        double minimumNoGoClearanceM = std::numeric_limits<double>::infinity();
        bool enteredOriginalNoGo = false;
        for (const TrajectorySample& sample : missionRun.trajectory) {
            const std::optional<Point2D> local =
                anchorReference->toLocal({sample.coordinate.latitude(), sample.coordinate.longitude(), 0.0});
            if (!local) {
                continue;
            }
            actualLocal.push_back(*local);
            for (const Polygon2D& noGo : anchoredProblem.region.noGoRegions) {
                minimumNoGoClearanceM = std::min(minimumNoGoClearanceM, pointToPolygonBoundaryDistance(*local, noGo));
                enteredOriginalNoGo |= pointStrictlyInside(noGo, *local);
            }
        }
        QVERIFY(!missionRun.trajectory.empty());
        QVERIFY2(!enteredOriginalNoGo, "Actual SITL trajectory entered the original No-Go polygon");

        const QString plannedImagePath =
            QDir(scenarioDirectory).filePath(definition.id + QStringLiteral("-planned.png"));
        const QString actualImagePath = QDir(scenarioDirectory).filePath(definition.id + QStringLiteral("-actual.png"));
        const BoustrophedonCoveragePlanner plotPlanner;
        const CoveragePlanningSolution plotSolution = plotPlanner.plan(anchoredProblem);
        QCOMPARE(plotSolution.status, PlanningStatus::Success);
        QVERIFY(renderEvidence(plannedImagePath, definition.id + QStringLiteral(" planned roles"), anchoredProblem,
                               plotSolution, {}));
        QVERIFY(renderEvidence(actualImagePath, definition.id + QStringLiteral(" planned + actual"), anchoredProblem,
                               plotSolution, actualLocal));

        QJsonArray reachedJson;
        for (const int sequence : uniqueReached) {
            reachedJson.append(sequence);
        }
        QJsonArray statusTextJson;
        for (const QString& text : missionRun.statusTexts) {
            statusTextJson.append(text);
        }
        const QJsonObject resultJson{
            {QStringLiteral("scenario"), definition.id},
            {QStringLiteral("planner"), QStringLiteral("marine.coverage.bcd")},
            {QStringLiteral("selectedSweepAngleDeg"), expectedResult.selectedSweepAngleDeg},
            {QStringLiteral("pathPoints"), static_cast<qint64>(expectedResult.path.size())},
            {QStringLiteral("coverageRuns"), roleRunCount(expectedResult.legRoles, PathLegRole::Coverage)},
            {QStringLiteral("transitRuns"), roleRunCount(expectedResult.legRoles, PathLegRole::Transit)},
            {QStringLiteral("cellCount"), expectedResult.cellCount},
            {QStringLiteral("turnCount"), expectedResult.turnCount},
            {QStringLiteral("coverageLengthM"), expectedResult.coverageLengthM},
            {QStringLiteral("transitLengthM"), expectedResult.transitLengthM},
            {QStringLiteral("pathLengthM"), expectedResult.pathLengthM},
            {QStringLiteral("coverageTargetAreaM2"), completeness.coverageTargetAreaM2},
            {QStringLiteral("uncoveredAreaM2"), completeness.uncoveredAreaM2},
            {QStringLiteral("coverageToleranceM2"), completeness.toleranceM2},
            {QStringLiteral("planSha256"), QString::fromLatin1(planHash)},
            {QStringLiteral("tlogSha256"), QString::fromLatin1(sha256(tlogPath))},
            {QStringLiteral("trajectoryCsvSha256"), QString::fromLatin1(sha256(csvPath))},
            {QStringLiteral("plannedImageSha256"), QString::fromLatin1(sha256(plannedImagePath))},
            {QStringLiteral("actualImageSha256"), QString::fromLatin1(sha256(actualImagePath))},
            {QStringLiteral("missionComplete"), missionRun.missionComplete},
            {QStringLiteral("reachedSequences"), reachedJson},
            {QStringLiteral("statusTexts"), statusTextJson},
            {QStringLiteral("enteredOriginalNoGo"), enteredOriginalNoGo},
            {QStringLiteral("minimumNoGoClearanceM"),
             std::isfinite(minimumNoGoClearanceM) ? QJsonValue(minimumNoGoClearanceM) : QJsonValue()},
            {QStringLiteral("safetyBandIntrusion"),
             std::isfinite(minimumNoGoClearanceM) &&
                 (minimumNoGoClearanceM + Geometry::LengthEpsilonM < definition.safetyMarginM)},
        };
        QVERIFY(writeJson(QDir(scenarioDirectory).filePath(QStringLiteral("result.json")), resultJson));
        vehicle->setArmed(false, true);
        QTRY_VERIFY_WITH_TIMEOUT(!vehicle->armed(), ConnectionTimeoutMs);
    }
}

UT_REGISTER_TEST_STANDALONE(MarineSITLValidationTest, TestLabel::Integration, TestLabel::Vehicle,
                            TestLabel::MissionManager, TestLabel::Network, TestLabel::Serial, TestLabel::Slow)
