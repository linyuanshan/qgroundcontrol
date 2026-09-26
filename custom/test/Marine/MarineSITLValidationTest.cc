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
#include <QtCore/QSet>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>
#include <QtCore/QtEndian>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <algorithm>
#include <array>
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
#include "Geometry/PolygonRegion.h"
#include "LinkManager.h"
#include "MAVLinkSigning.h"
#include "MarinePlanContext.h"
#include "MissionController.h"
#include "MissionItem.h"
#include "MissionManager.h"
#include "MissionSettingsItem.h"
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
#include "VisualMissionItem.h"

using namespace Marine;

namespace {

constexpr int ConnectionTimeoutMs = 120000;
constexpr int MissionTimeoutMs = 900000;
constexpr double CoordinateToleranceDeg = 1e-10;
constexpr double MetricToleranceM = 1e-3;
constexpr double TargetedStopBoundaryDistanceM = 0.05;
constexpr double TargetedStopMinimumTurnDeg = 5.0;

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

struct MissionStateSample
{
    qint64 timestampMs = 0;
    int sequence = -1;
    int state = MISSION_STATE_UNKNOWN;
    int mode = 0;
};

struct MissionRunResult
{
    bool initialArmed = false;
    QString initialFlightMode;
    bool initialFlying = false;
    bool readyToFly = false;
    bool positionEstimateReady = false;
    bool started = false;
    bool enteredAuto = false;
    bool missionComplete = false;
    bool timedOut = false;
    bool missionStartAckSeen = false;
    int missionStartAckResult = -1;
    bool missionActiveObserved = false;
    QStringList statusTexts;
    QList<int> reachedSequences;
    QList<int> currentSequences;
    QList<MissionStateSample> missionStateHistory;
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
    result.initialArmed = vehicle->armed();
    result.initialFlightMode = vehicle->flightMode();
    result.initialFlying = vehicle->flying();
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
            } else if (message.msgid == MAVLINK_MSG_ID_COMMAND_ACK) {
                mavlink_command_ack_t acknowledgement{};
                mavlink_msg_command_ack_decode(&message, &acknowledgement);
                if (acknowledgement.command == MAV_CMD_MISSION_START) {
                    result.missionStartAckSeen = true;
                    result.missionStartAckResult = static_cast<int>(acknowledgement.result);
                }
            } else if (message.msgid == MAVLINK_MSG_ID_MISSION_CURRENT && message.len >= 5) {
                mavlink_mission_current_t current{};
                mavlink_msg_mission_current_decode(&message, &current);
                result.missionStateHistory.append({QDateTime::currentMSecsSinceEpoch(),
                                                   static_cast<int>(current.seq),
                                                   static_cast<int>(current.mission_state),
                                                   static_cast<int>(current.mission_mode)});
                result.missionActiveObserved |= current.mission_state == MISSION_STATE_ACTIVE;
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

    vehicle->startMission();
    result.started = true;

    QElapsedTimer modeWait;
    modeWait.start();
    const bool rover = vehicle->vehicleType() == MAV_TYPE_GROUND_ROVER;
    while (modeWait.elapsed() < ConnectionTimeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        QTest::qWait(50);
        const bool modeReady = vehicle->armed() && (vehicle->flightMode() == vehicle->missionFlightMode());
        const bool startEvidenceReady = !rover || (result.missionStartAckSeen &&
                                                   (result.missionStartAckResult == MAV_RESULT_ACCEPTED) &&
                                                   result.missionActiveObserved);
        if (modeReady && startEvidenceReady) {
            break;
        }
        if (rover && result.missionStartAckSeen && (result.missionStartAckResult != MAV_RESULT_ACCEPTED)) {
            break;
        }
    }
    result.enteredAuto = vehicle->armed() && (vehicle->flightMode() == vehicle->missionFlightMode()) &&
                         (!rover || (result.missionStartAckSeen &&
                                     (result.missionStartAckResult == MAV_RESULT_ACCEPTED) &&
                                     result.missionActiveObserved));
    if (result.enteredAuto) {
        timeout.start(MissionTimeoutMs);
        loop.exec();
    }

    timeout.stop();
    QObject::disconnect(messageConnection);
    QObject::disconnect(currentConnection);
    QObject::disconnect(coordinateConnection);
    (void) tlog.flush();
    return result;
}

enum class DiagnosticStopPolicy
{
    FlyThrough,
    AllStop,
    TargetedStop,
};

struct DiagnosticRunDefinition
{
    QString id;
    double waypointRadiusM = 3.0;
    double speedFactor = 1.0;
    DiagnosticStopPolicy stopPolicy = DiagnosticStopPolicy::FlyThrough;
};

struct NoGoExecutionResult
{
    int insideSampleCount = 0;
    double maximumPenetrationM = 0.0;
    double minimumSignedClearanceM = std::numeric_limits<double>::infinity();
    QSet<int> incidentSequences;
    std::vector<Point2D> actualLocal;
};

std::optional<DiagnosticRunDefinition> diagnosticRunDefinition(const QString& requestedRun)
{
    const QString run = requestedRun.trimmed().toUpper();
    if ((run == QStringLiteral("D00")) || (run == QStringLiteral("S04-DIAG-00"))) {
        return DiagnosticRunDefinition{QStringLiteral("S04-DIAG-00"), 3.0, 1.0, DiagnosticStopPolicy::FlyThrough};
    }
    if ((run == QStringLiteral("D01")) || (run == QStringLiteral("S04-DIAG-01"))) {
        return DiagnosticRunDefinition{QStringLiteral("S04-DIAG-01"), 1.0, 1.0, DiagnosticStopPolicy::FlyThrough};
    }
    if ((run == QStringLiteral("D02")) || (run == QStringLiteral("S04-DIAG-02"))) {
        return DiagnosticRunDefinition{QStringLiteral("S04-DIAG-02"), 0.5, 1.0, DiagnosticStopPolicy::FlyThrough};
    }
    if ((run == QStringLiteral("D03")) || (run == QStringLiteral("S04-DIAG-03"))) {
        return DiagnosticRunDefinition{QStringLiteral("S04-DIAG-03"), 3.0, 0.5, DiagnosticStopPolicy::FlyThrough};
    }
    if ((run == QStringLiteral("D04")) || (run == QStringLiteral("S04-DIAG-04-ALL-STOP"))) {
        return DiagnosticRunDefinition{QStringLiteral("S04-DIAG-04-ALL-STOP"), 3.0, 1.0, DiagnosticStopPolicy::AllStop};
    }
    if ((run == QStringLiteral("D05")) || (run == QStringLiteral("S04-DIAG-05-TARGETED-STOP"))) {
        return DiagnosticRunDefinition{QStringLiteral("S04-DIAG-05-TARGETED-STOP"), 3.0, 1.0,
                                       DiagnosticStopPolicy::TargetedStop};
    }
    return std::nullopt;
}

QString diagnosticStopPolicyName(DiagnosticStopPolicy policy)
{
    switch (policy) {
        case DiagnosticStopPolicy::FlyThrough:
            return QStringLiteral("Fly-through");
        case DiagnosticStopPolicy::AllStop:
            return QStringLiteral("ALL-STOP");
        case DiagnosticStopPolicy::TargetedStop:
            return QStringLiteral("targeted-stop");
    }
    return QStringLiteral("Unknown");
}

QJsonObject parameterSnapshot(ParameterManager* manager, const QStringList& names)
{
    QJsonObject snapshot;
    for (const QString& name : names) {
        QJsonObject parameter{{QStringLiteral("present"), false}};
        if (manager->parameterExists(ParameterManager::defaultComponentId, name)) {
            Fact* fact = manager->getParameter(ParameterManager::defaultComponentId, name);
            parameter.insert(QStringLiteral("present"), true);
            parameter.insert(QStringLiteral("value"), fact->rawValue().toDouble());
        } else {
            parameter.insert(QStringLiteral("value"), QStringLiteral("not present"));
        }
        snapshot.insert(name, parameter);
    }
    return snapshot;
}

bool setVehicleParameter(ParameterManager* manager, const QString& name, double value)
{
    if (!manager->parameterExists(ParameterManager::defaultComponentId, name)) {
        return false;
    }
    Fact* fact = manager->getParameter(ParameterManager::defaultComponentId, name);
    fact->setRawValue(value);
    QElapsedTimer wait;
    wait.start();
    while ((wait.elapsed() < 5000) && (std::abs(fact->rawValue().toDouble() - value) > 1e-6)) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        QTest::qWait(50);
    }
    QTest::qWait(500);
    manager->refreshParameter(ParameterManager::defaultComponentId, name);
    QTest::qWait(500);
    return std::abs(fact->rawValue().toDouble() - value) <= 1e-6;
}

QList<MissionItem*> missionItemsFromController(PlanMasterController& controller, Vehicle* vehicle)
{
    QList<MissionItem*> missionItems;
    QmlObjectListModel* visualItems = controller.missionController()->visualItems();
    if ((visualItems == nullptr) || (visualItems->count() == 0)) {
        return missionItems;
    }

    int lastSequence = 0;
    for (int index = 0; index < visualItems->count(); ++index) {
        VisualMissionItem* visualItem = visualItems->value<VisualMissionItem*>(index);
        if (visualItem == nullptr) {
            qDeleteAll(missionItems);
            return {};
        }
        lastSequence = visualItem->lastSequenceNumber();
        visualItem->appendMissionItems(missionItems, vehicle);
    }
    if (MissionSettingsItem* settings = visualItems->value<MissionSettingsItem*>(0)) {
        (void) settings->addMissionEndAction(missionItems, lastSequence + 1, vehicle);
    }
    return missionItems;
}

bool uploadMissionItems(Vehicle* vehicle, const QList<MissionItem*>& missionItems)
{
    QSignalSpy sendSpy(vehicle->missionManager(), &MissionManager::sendComplete);
    if (!sendSpy.isValid()) {
        qDeleteAll(missionItems);
        return false;
    }
    vehicle->missionManager()->writeMissionItems(missionItems);
    if (!sendSpy.wait(ConnectionTimeoutMs) || (sendSpy.count() != 1)) {
        return false;
    }
    return !sendSpy.takeFirst().at(0).toBool();
}

bool resetMissionExecutionState(Vehicle* vehicle)
{
    vehicle->setFlightMode(vehicle->pauseFlightMode());
    QElapsedTimer modeWait;
    modeWait.start();
    while ((vehicle->flightMode() != vehicle->pauseFlightMode()) && (modeWait.elapsed() < ConnectionTimeoutMs)) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        QTest::qWait(50);
    }
    vehicle->setArmed(false, true);
    QElapsedTimer disarmWait;
    disarmWait.start();
    while (vehicle->armed() && (disarmWait.elapsed() < ConnectionTimeoutMs)) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        QTest::qWait(50);
    }
    return (vehicle->flightMode() == vehicle->pauseFlightMode()) && !vehicle->armed();
}

QJsonObject missionExecutionState(Vehicle* vehicle, const QString& prefix)
{
    return {{prefix + QStringLiteral("Armed"), vehicle->armed()},
            {prefix + QStringLiteral("FlightMode"), vehicle->flightMode()},
            {prefix + QStringLiteral("Flying"), vehicle->flying()},
            {prefix + QStringLiteral("MissionCurrent"), vehicle->missionManager()->currentIndex()}};
}

bool seedStaleMissionExecutionState(Vehicle* vehicle)
{
    vehicle->setFlightMode(vehicle->missionFlightMode());
    QElapsedTimer modeWait;
    modeWait.start();
    while ((vehicle->flightMode() != vehicle->missionFlightMode()) && (modeWait.elapsed() < ConnectionTimeoutMs)) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        QTest::qWait(50);
    }
    if (vehicle->flightMode() != vehicle->missionFlightMode()) {
        return false;
    }
    vehicle->setArmed(true, true);
    QElapsedTimer armWait;
    armWait.start();
    while (!vehicle->armed() && (armWait.elapsed() < ConnectionTimeoutMs)) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        QTest::qWait(50);
    }
    return vehicle->armed() && (vehicle->flightMode() == vehicle->missionFlightMode());
}

bool prepositionAtFirstWaypoint(Vehicle* vehicle, const QList<MissionItem*>& fullMission, int missionItemListPathOffset,
                                int missionSequenceOffset, const QString& tlogPath, MissionRunResult& run)
{
    if ((missionItemListPathOffset != 1) || (fullMission.size() < 2)) {
        return false;
    }
    QList<MissionItem*> prepositionMission;
    prepositionMission.append(new MissionItem(*fullMission.at(0), vehicle));
    prepositionMission.append(new MissionItem(*fullMission.at(missionItemListPathOffset), vehicle));
    prepositionMission.back()->setParam1(1.0);
    if (!uploadMissionItems(vehicle, prepositionMission)) {
        return false;
    }
    vehicle->setCurrentMissionSequence(missionSequenceOffset);
    QElapsedTimer sequenceWait;
    sequenceWait.start();
    while ((vehicle->missionManager()->currentIndex() != missionSequenceOffset) &&
           (sequenceWait.elapsed() < ConnectionTimeoutMs)) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        QTest::qWait(50);
    }
    if (vehicle->missionManager()->currentIndex() != missionSequenceOffset) {
        return false;
    }

    QFile tlog(tlogPath);
    if (!tlog.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    run = executeMission(vehicle, missionSequenceOffset, tlog);
    tlog.close();
    const bool reset = resetMissionExecutionState(vehicle);
    return run.readyToFly && run.enteredAuto && !run.timedOut && run.missionComplete && reset;
}

double turnAngleDeg(const Point2D& before, const Point2D& point, const Point2D& after)
{
    constexpr double RadiansToDegrees = 180.0 / 3.14159265358979323846;
    const double incoming = std::atan2(point.yM - before.yM, point.xM - before.xM);
    const double outgoing = std::atan2(after.yM - point.yM, after.xM - point.xM);
    return std::abs(std::remainder((outgoing - incoming) * RadiansToDegrees, 360.0));
}

QSet<int> targetedStopPathIndices(const std::vector<Point2D>& path, const PolygonRegionSet2D& trackFeasibleRegion)
{
    QSet<int> indices;
    if ((path.size() < 3) || trackFeasibleRegion.empty()) {
        return indices;
    }
    for (std::size_t index = 1; index + 1 < path.size(); ++index) {
        double neighbourhoodDistance = std::numeric_limits<double>::infinity();
        for (const PolygonRegion2D& region : trackFeasibleRegion) {
            for (const Polygon2D& hole : region.holes) {
                neighbourhoodDistance =
                    std::min({neighbourhoodDistance, pointToPolygonBoundaryDistance(path[index - 1], hole),
                              pointToPolygonBoundaryDistance(path[index], hole),
                              pointToPolygonBoundaryDistance(path[index + 1], hole)});
            }
        }
        if ((neighbourhoodDistance <= TargetedStopBoundaryDistanceM) &&
            (turnAngleDeg(path[index - 1], path[index], path[index + 1]) >= TargetedStopMinimumTurnDeg)) {
            indices.insert(static_cast<int>(index));
        }
    }
    return indices;
}

NoGoExecutionResult analyzeNoGoExecution(const MissionRunResult& missionRun, const CoveragePlanningProblem& problem,
                                         const GeoReference& reference)
{
    NoGoExecutionResult result;
    result.actualLocal.reserve(missionRun.trajectory.size());
    for (const TrajectorySample& sample : missionRun.trajectory) {
        const std::optional<Point2D> local =
            reference.toLocal({sample.coordinate.latitude(), sample.coordinate.longitude(), 0.0});
        if (!local) {
            continue;
        }
        result.actualLocal.push_back(*local);
        for (const Polygon2D& noGo : problem.region.noGoRegions) {
            const double boundaryDistance = pointToPolygonBoundaryDistance(*local, noGo);
            const bool inside = pointStrictlyInside(noGo, *local);
            const double signedClearance = inside ? -boundaryDistance : boundaryDistance;
            result.minimumSignedClearanceM = std::min(result.minimumSignedClearanceM, signedClearance);
            if (inside) {
                ++result.insideSampleCount;
                result.maximumPenetrationM = std::max(result.maximumPenetrationM, boundaryDistance);
                result.incidentSequences.insert(sample.missionSequence);
            }
        }
    }
    return result;
}

double polygonPerimeter(const Polygon2D& polygon)
{
    double perimeterM = 0.0;
    Point2D previous = polygon.vertices.back();
    for (const Point2D& current : polygon.vertices) {
        perimeterM += std::hypot(current.xM - previous.xM, current.yM - previous.yM);
        previous = current;
    }
    return perimeterM;
}

double minimumLegLength(const CoveragePlanningSolution& solution)
{
    double minimumM = std::numeric_limits<double>::infinity();
    for (std::size_t index = 1; index < solution.path.size(); ++index) {
        minimumM = std::min(minimumM, std::hypot(solution.path[index].xM - solution.path[index - 1].xM,
                                                 solution.path[index].yM - solution.path[index - 1].yM));
    }
    return minimumM;
}

bool sameSolution(const CoveragePlanningSolution& first, const CoveragePlanningSolution& second)
{
    if ((first.status != second.status) || (first.error != second.error) || (first.legRoles != second.legRoles) ||
        (first.path.size() != second.path.size()) || (first.coverageLengthM != second.coverageLengthM) ||
        (first.transitLengthM != second.transitLengthM) || (first.pathLengthM != second.pathLengthM) ||
        (first.selectedSweepAngleDeg != second.selectedSweepAngleDeg) || (first.cellCount != second.cellCount) ||
        (first.turnCount != second.turnCount)) {
        return false;
    }
    for (std::size_t index = 0; index < first.path.size(); ++index) {
        if ((first.path[index].xM != second.path[index].xM) || (first.path[index].yM != second.path[index].yM)) {
            return false;
        }
    }
    return true;
}

std::optional<Polygon2D> miterInflatedRectangle(const Polygon2D& polygon, double marginM)
{
    if (polygon.vertices.size() != 4) {
        return std::nullopt;
    }
    double minimumX = std::numeric_limits<double>::infinity();
    double minimumY = std::numeric_limits<double>::infinity();
    double maximumX = -std::numeric_limits<double>::infinity();
    double maximumY = -std::numeric_limits<double>::infinity();
    for (const Point2D& point : polygon.vertices) {
        minimumX = std::min(minimumX, point.xM);
        minimumY = std::min(minimumY, point.yM);
        maximumX = std::max(maximumX, point.xM);
        maximumY = std::max(maximumY, point.yM);
    }
    for (const Point2D& point : polygon.vertices) {
        const bool onX = (point.xM == minimumX) || (point.xM == maximumX);
        const bool onY = (point.yM == minimumY) || (point.yM == maximumY);
        if (!onX || !onY) {
            return std::nullopt;
        }
    }
    return rectangle(minimumX - marginM, minimumY - marginM, maximumX + marginM, maximumY + marginM);
}

QJsonObject analyzeExecutionCandidate(const ScenarioDefinition& definition, double executionMarginM, bool miterNoGo)
{
    const double totalMarginM = definition.safetyMarginM + executionMarginM;
    CoveragePlanningProblem planningProblem = problemFor(definition);
    PolygonRegionSet2D executionRegion;
    Geometry::PolygonRegionOperationStatus geometryStatus = Geometry::PolygonRegionOperationStatus::GeometryFailure;

    if (!miterNoGo) {
        planningProblem.safetyMarginM = totalMarginM;
        const Geometry::PolygonRegionOperationResult trackFeasible =
            Geometry::buildTrackFeasibleRegion(definition.outer, definition.noGoRegions, totalMarginM);
        geometryStatus = trackFeasible.status;
        executionRegion = trackFeasible.regions;
    } else {
        const Geometry::PolygonRegionOperationResult insetOuter =
            Geometry::buildTrackFeasibleRegion(definition.outer, {}, totalMarginM);
        if ((insetOuter.status == Geometry::PolygonRegionOperationStatus::Success) &&
            (insetOuter.regions.size() == 1) && insetOuter.regions.front().holes.empty()) {
            std::vector<Polygon2D> inflatedNoGo;
            inflatedNoGo.reserve(definition.noGoRegions.size());
            for (const Polygon2D& noGo : definition.noGoRegions) {
                const std::optional<Polygon2D> inflated = miterInflatedRectangle(noGo, totalMarginM);
                if (!inflated) {
                    inflatedNoGo.clear();
                    break;
                }
                inflatedNoGo.push_back(*inflated);
            }
            if (inflatedNoGo.size() == definition.noGoRegions.size()) {
                const Geometry::PolygonRegionOperationResult candidate =
                    Geometry::buildCoverageTarget(insetOuter.regions.front().outerBoundary, inflatedNoGo);
                geometryStatus = candidate.status;
                executionRegion = candidate.regions;
                if (candidate.status == Geometry::PolygonRegionOperationStatus::Success) {
                    planningProblem.region.outerBoundary = insetOuter.regions.front().outerBoundary;
                    planningProblem.region.noGoRegions = std::move(inflatedNoGo);
                    planningProblem.safetyMarginM = 0.0;
                }
            }
        }
    }

    const bool geometryFeasible =
        (geometryStatus == Geometry::PolygonRegionOperationStatus::Success) && !executionRegion.empty();
    const bool connected = geometryFeasible && (executionRegion.size() == 1);
    QJsonObject result{{QStringLiteral("scenario"), definition.id},
                       {QStringLiteral("candidate"), miterNoGo ? QStringLiteral("Miter") : QStringLiteral("Round")},
                       {QStringLiteral("executionMarginM"), executionMarginM},
                       {QStringLiteral("totalCenterlineMarginM"), totalMarginM},
                       {QStringLiteral("geometryFeasible"), geometryFeasible},
                       {QStringLiteral("connected"), connected},
                       {QStringLiteral("coverageComplete"), false},
                       {QStringLiteral("deterministic"), false},
                       {QStringLiteral("pathPoints"), 0},
                       {QStringLiteral("legCount"), 0},
                       {QStringLiteral("turnCount"), 0},
                       {QStringLiteral("executionBoundaryVertexCount"), 0},
                       {QStringLiteral("executionBoundaryPerimeterM"), 0.0},
                       {QStringLiteral("freeSpaceAreaM2"), 0.0}};

    if (geometryFeasible) {
        const Geometry::PolygonRegionAreaResult area = Geometry::polygonRegionArea(executionRegion);
        if (area.status == Geometry::PolygonRegionOperationStatus::Success) {
            result.insert(QStringLiteral("freeSpaceAreaM2"), area.areaM2);
        }
        int holeVertexCount = 0;
        double holePerimeterM = 0.0;
        for (const PolygonRegion2D& region : executionRegion) {
            for (const Polygon2D& hole : region.holes) {
                holeVertexCount += static_cast<int>(hole.vertices.size());
                holePerimeterM += polygonPerimeter(hole);
            }
        }
        result.insert(QStringLiteral("executionBoundaryVertexCount"), holeVertexCount);
        result.insert(QStringLiteral("executionBoundaryPerimeterM"), holePerimeterM);
    }
    if (!connected) {
        result.insert(QStringLiteral("message"), QStringLiteral("Execution region is empty or disconnected"));
        return result;
    }

    const BoustrophedonCoveragePlanner planner;
    const CoveragePlanningSolution solution = planner.plan(planningProblem);
    const CoveragePlanningSolution repeated = planner.plan(planningProblem);
    result.insert(QStringLiteral("planningStatus"), static_cast<int>(solution.status));
    result.insert(QStringLiteral("planningError"), static_cast<int>(solution.error));
    result.insert(QStringLiteral("message"), QString::fromStdString(solution.message));
    result.insert(QStringLiteral("deterministic"), sameSolution(solution, repeated));
    if (solution.status != PlanningStatus::Success) {
        return result;
    }

    bool geometryGate = true;
    for (std::size_t index = 1; index < solution.path.size(); ++index) {
        if (!Geometry::segmentInsidePolygonRegionForValidatedGeometry(executionRegion, solution.path[index - 1],
                                                                      solution.path[index])) {
            geometryGate = false;
            break;
        }
    }
    const Geometry::PolygonRegionOperationResult coverageTarget =
        Geometry::buildCoverageTarget(definition.outer, definition.noGoRegions);
    CoverageCompletenessResult completeness;
    if ((coverageTarget.status == Geometry::PolygonRegionOperationStatus::Success) &&
        (coverageTarget.regions.size() == 1)) {
        completeness =
            validateNominalCoverage(coverageTarget.regions, solution.path, solution.legRoles, definition.swathWidthM);
    }

    result.insert(QStringLiteral("geometryGate"), geometryGate);
    result.insert(QStringLiteral("coverageComplete"), completeness.status == PlanningStatus::Success);
    result.insert(QStringLiteral("coverageError"), static_cast<int>(completeness.error));
    result.insert(QStringLiteral("coverageTargetAreaM2"), completeness.coverageTargetAreaM2);
    result.insert(QStringLiteral("uncoveredAreaM2"), completeness.uncoveredAreaM2);
    result.insert(QStringLiteral("coverageToleranceM2"), completeness.toleranceM2);
    result.insert(QStringLiteral("pathPoints"), static_cast<qint64>(solution.path.size()));
    result.insert(QStringLiteral("legCount"), static_cast<qint64>(solution.legRoles.size()));
    result.insert(QStringLiteral("turnCount"), solution.turnCount);
    result.insert(QStringLiteral("minimumLegLengthM"), minimumLegLength(solution));
    result.insert(QStringLiteral("pathLengthM"), solution.pathLengthM);
    result.insert(QStringLiteral("selectedSweepAngleDeg"), solution.selectedSweepAngleDeg);
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

void MarineSITLValidationTest::_analyzeExecutionSafety()
{
    const QString outputPath = qEnvironmentVariable("QGC_P2_EXECUTION_SAFETY_ANALYSIS");
    if (outputPath.isEmpty()) {
        QSKIP("Set QGC_P2_EXECUTION_SAFETY_ANALYSIS to run the offline P2-13E analysis");
    }

    constexpr std::array<double, 5> ExecutionMarginsM{0.0, 0.25, 0.5, 0.75, 1.0};
    QJsonArray results;
    for (const ScenarioDefinition& definition : scenarios()) {
        if ((definition.id != QStringLiteral("S03")) && (definition.id != QStringLiteral("S04"))) {
            continue;
        }
        ScenarioDefinition analysisDefinition = definition;
        analysisDefinition.sweepAngleMode = SweepAngleMode::Manual;
        analysisDefinition.sweepAngleDeg = definition.id == QStringLiteral("S04") ? 90.00014626 : 90.0;
        for (const bool miterNoGo : {false, true}) {
            for (const double executionMarginM : ExecutionMarginsM) {
                results.append(analyzeExecutionCandidate(analysisDefinition, executionMarginM, miterNoGo));
            }
        }
    }

    const QJsonObject report{{QStringLiteral("safetyMarginM"), 1.0},
                             {QStringLiteral("swathWidthM"), 4.0},
                             {QStringLiteral("sweepAnglesFrozenFromEvidence"), true},
                             {QStringLiteral("results"), results}};
    QVERIFY2(writeJson(outputPath, report), qPrintable(QStringLiteral("Cannot write %1").arg(outputPath)));
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
    const QList<QPair<QString, double>> expectedParameters{
        {QStringLiteral("WP_RADIUS"), 3.0},
        {QStringLiteral("WP_SPEED"), 5.0},
        {QStringLiteral("TURN_RADIUS"), 0.9},
        {QStringLiteral("ATC_TURN_MAX_G"), 0.6},
        {QStringLiteral("WP_ACCEL"), 0.0},
        {QStringLiteral("WP_JERK"), 0.0},
    };
    QStringList parameterNames;
    for (const auto& expectedParameter : expectedParameters) {
        parameterNames.append(expectedParameter.first);
    }
    const QJsonObject parameterValues = parameterSnapshot(vehicle->parameterManager(), parameterNames);
    for (const auto& expectedParameter : expectedParameters) {
        const QJsonObject parameter = parameterValues.value(expectedParameter.first).toObject();
        QVERIFY2(parameter.value(QStringLiteral("present")).toBool(),
                 qPrintable(QStringLiteral("Missing %1").arg(expectedParameter.first)));
        QVERIFY2(std::abs(parameter.value(QStringLiteral("value")).toDouble() - expectedParameter.second) <= 1e-3,
                 qPrintable(QStringLiteral("Unexpected %1 value").arg(expectedParameter.first)));
    }
    const double waypointRadiusM = vehicle->parameterManager()
                                       ->getParameter(ParameterManager::defaultComponentId, QStringLiteral("WP_RADIUS"))
                                       ->rawValue()
                                       .toDouble();

    QJsonObject baseline{{QStringLiteral("firmwareVersion"), QStringLiteral("%1.%2.%3")
                                                                 .arg(vehicle->firmwareMajorVersion())
                                                                 .arg(vehicle->firmwareMinorVersion())
                                                                 .arg(vehicle->firmwarePatchVersion())},
                         {QStringLiteral("wpRadiusM"), waypointRadiusM},
                         {QStringLiteral("parameters"), parameterValues},
                         {QStringLiteral("tcpEndpoint"), QStringLiteral("127.0.0.1:5760")},
                         {QStringLiteral("recordedAtUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
    QVERIFY(writeJson(QDir(evidenceRoot).filePath(QStringLiteral("baseline.json")), baseline));

    const QString requestedScenario = qEnvironmentVariable("QGC_P2_SITL_SCENARIO");
    if (!requestedScenario.isEmpty()) {
        const auto definitions = scenarios();
        QVERIFY(std::any_of(definitions.cbegin(), definitions.cend(), [&](const ScenarioDefinition& definition) {
            return definition.id == requestedScenario;
        }));
    }
    for (const ScenarioDefinition& definition : scenarios()) {
        if (!requestedScenario.isEmpty() && (definition.id != requestedScenario)) {
            continue;
        }
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
                    freeSpace.freeSpace.executionTrackFeasibleRegion, localSolution.path[index - 1],
                    localSolution.path[index]));
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

        const bool seedStaleState = (definition.id == QStringLiteral("S01")) &&
                                    (qEnvironmentVariableIntValue("QGC_P2_SITL_SEED_STALE_EXECUTION_STATE") == 1);
        const bool staleStateSeeded = !seedStaleState || seedStaleMissionExecutionState(vehicle);
        const QJsonObject preResetState = missionExecutionState(vehicle, QStringLiteral("preReset"));
        const bool resetSucceeded = resetMissionExecutionState(vehicle);
        const QJsonObject postResetState = missionExecutionState(vehicle, QStringLiteral("postReset"));
        QJsonObject executionState = preResetState;
        for (const QString& key : postResetState.keys()) {
            executionState.insert(key, postResetState.value(key));
        }
        QVERIFY2(writeJson(QDir(scenarioDirectory).filePath(QStringLiteral("execution-state.json")), executionState),
                 "Cannot write execution-state.json");
        QVERIFY2(staleStateSeeded, "Unable to create stale armed AUTO test precondition");
        QVERIFY2(resetSucceeded, "Unable to reset Rover mission execution state");

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
        QJsonArray initialMissionStateHistory;
        for (const MissionStateSample& sample : missionRun.missionStateHistory) {
            initialMissionStateHistory.append(QJsonObject{{QStringLiteral("timestampMs"), sample.timestampMs},
                                                          {QStringLiteral("sequence"), sample.sequence},
                                                          {QStringLiteral("missionState"), sample.state},
                                                          {QStringLiteral("missionMode"), sample.mode}});
        }
        QVERIFY(writeJson(QDir(scenarioDirectory).filePath(QStringLiteral("result.json")),
                          {{QStringLiteral("scenario"), definition.id},
                           {QStringLiteral("initialArmed"), missionRun.initialArmed},
                           {QStringLiteral("initialFlightMode"), missionRun.initialFlightMode},
                           {QStringLiteral("initialFlying"), missionRun.initialFlying},
                           {QStringLiteral("missionStartAckSeen"), missionRun.missionStartAckSeen},
                           {QStringLiteral("missionStartAckResult"), missionRun.missionStartAckResult},
                           {QStringLiteral("missionActiveObserved"), missionRun.missionActiveObserved},
                           {QStringLiteral("missionStateHistory"), initialMissionStateHistory},
                           {QStringLiteral("missionComplete"), missionRun.missionComplete},
                           {QStringLiteral("timedOut"), missionRun.timedOut}}));
        QVERIFY2(missionRun.readyToFly,
                 qPrintable(QStringLiteral("Rover did not become ready to fly: %1").arg(missionRun.prearmError)));
        QVERIFY(missionRun.started);
        QVERIFY2(missionRun.enteredAuto, "Rover did not arm and enter AUTO");
        QVERIFY2(missionRun.missionStartAckSeen && (missionRun.missionStartAckResult == MAV_RESULT_ACCEPTED),
                 "Rover did not acknowledge MAV_CMD_MISSION_START with ACCEPTED");
        QVERIFY2(missionRun.missionActiveObserved, "Rover did not report MISSION_STATE_ACTIVE");
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
            {QStringLiteral("initialArmed"), missionRun.initialArmed},
            {QStringLiteral("initialFlightMode"), missionRun.initialFlightMode},
            {QStringLiteral("initialFlying"), missionRun.initialFlying},
            {QStringLiteral("missionStartAckSeen"), missionRun.missionStartAckSeen},
            {QStringLiteral("missionStartAckResult"), missionRun.missionStartAckResult},
            {QStringLiteral("missionActiveObserved"), missionRun.missionActiveObserved},
            {QStringLiteral("missionStateHistory"), initialMissionStateHistory},
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
        QVERIFY2(resetMissionExecutionState(vehicle), "Unable to reset Rover after completed scenario");
    }
}

void MarineSITLValidationTest::_diagnoseS04Execution()
{
    const QString evidenceRoot = qEnvironmentVariable("QGC_P2_SITL_DIAGNOSTIC_ROOT");
    const QString planPath = qEnvironmentVariable("QGC_P2_SITL_DIAGNOSTIC_PLAN");
    const QString requestedRun = qEnvironmentVariable("QGC_P2_SITL_DIAGNOSTIC_RUN");
    if (evidenceRoot.isEmpty() || planPath.isEmpty() || requestedRun.isEmpty()) {
        QSKIP(
            "Set QGC_P2_SITL_DIAGNOSTIC_ROOT, QGC_P2_SITL_DIAGNOSTIC_PLAN, and "
            "QGC_P2_SITL_DIAGNOSTIC_RUN to enable an S04 diagnostic run");
    }
    const std::optional<DiagnosticRunDefinition> definition = diagnosticRunDefinition(requestedRun);
    QVERIFY2(definition.has_value(), qPrintable(QStringLiteral("Unknown diagnostic run: %1").arg(requestedRun)));
    QVERIFY(QFile::exists(planPath));
    const QByteArray planHash = sha256(planPath);
    QCOMPARE(planHash, QByteArrayLiteral("a9cad2b593d827266751efe055534e4eecf0ea4be416ca7adbd4f4039e7b1b60"));

    const QString runDirectory = QDir(evidenceRoot).filePath(definition->id);
    QVERIFY2(QDir().mkpath(runDirectory), qPrintable(QStringLiteral("Cannot create %1").arg(runDirectory)));
    QCOMPARE(LinkManager::instance()->links().count(), 0);

    auto* tcpConfiguration = new TCPConfiguration(QStringLiteral("P2-13D ArduRover SITL"));
    tcpConfiguration->setHost(QStringLiteral("127.0.0.1"));
    tcpConfiguration->setPort(5760);
    tcpConfiguration->setDynamic(true);
    SharedLinkConfigurationPtr sharedConfiguration(tcpConfiguration);
    QVERIFY2(LinkManager::instance()->createConnectedLink(sharedConfiguration), "Failed to connect TCP SITL link");

    QTRY_VERIFY_WITH_TIMEOUT(MultiVehicleManager::instance()->activeVehicle() != nullptr, ConnectionTimeoutMs);
    Vehicle* vehicle = MultiVehicleManager::instance()->activeVehicle();
    QVERIFY(vehicle != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(vehicle->isInitialConnectComplete(), ConnectionTimeoutMs);
    ParameterManager* parameterManager = vehicle->parameterManager();
    QVERIFY(parameterManager != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(parameterManager->parametersReady(), ConnectionTimeoutMs);
    QCOMPARE(vehicle->firmwareType(), MAV_AUTOPILOT_ARDUPILOTMEGA);
    QCOMPARE(vehicle->vehicleType(), MAV_TYPE_GROUND_ROVER);
    QTRY_VERIFY_WITH_TIMEOUT(vehicle->coordinate().isValid(), ConnectionTimeoutMs);
    QVERIFY2(resetMissionExecutionState(vehicle), "Unable to reset Rover to disarmed HOLD before diagnostic");

    const QStringList diagnosticParameterNames{QStringLiteral("WP_RADIUS"),   QStringLiteral("WP_SPEED"),
                                               QStringLiteral("TURN_RADIUS"), QStringLiteral("ATC_TURN_MAX_G"),
                                               QStringLiteral("WP_ACCEL"),    QStringLiteral("WP_JERK")};
    const QJsonObject baselineParameters = parameterSnapshot(parameterManager, diagnosticParameterNames);
    QVERIFY(parameterManager->parameterExists(ParameterManager::defaultComponentId, QStringLiteral("WP_RADIUS")));
    QVERIFY(parameterManager->parameterExists(ParameterManager::defaultComponentId, QStringLiteral("WP_SPEED")));
    const double baselineWaypointRadiusM =
        parameterManager->getParameter(ParameterManager::defaultComponentId, QStringLiteral("WP_RADIUS"))
            ->rawValue()
            .toDouble();
    const double baselineWaypointSpeedMps =
        parameterManager->getParameter(ParameterManager::defaultComponentId, QStringLiteral("WP_SPEED"))
            ->rawValue()
            .toDouble();
    QCOMPARE(baselineWaypointRadiusM, 3.0);
    QVERIFY(baselineWaypointSpeedMps > 0.0);

    PlanMasterController controller;
    controller.setFlyView(false);
    controller.start();
    controller.loadFromFile(planPath);
    CoverageInspectionComplexItem* item = coverageItem(controller);
    QVERIFY(item != nullptr);
    QCOMPARE(item->planningState(), CoverageInspectionComplexItem::Planned);
    const PlanningResult& planningResult = item->planningResult();
    QCOMPARE(planningResult.status, PlanningStatus::Success);
    QVERIFY(planningResult.path.size() >= 2);
    QCOMPARE(planningResult.legRoles.size(), planningResult.path.size() - 1);

    MarinePlanContext* context = controller.findChild<MarinePlanContext*>(QString(), Qt::FindDirectChildrenOnly);
    QVERIFY(context != nullptr);
    const MarineTask* task = context->task(item->taskId().toStdString());
    QVERIFY(task != nullptr);
    CoveragePlanningProblem problem;
    std::optional<GeoReference> reference;
    CoveragePlanningError adapterError = CoveragePlanningError::None;
    QVERIFY(CoverageTaskAdapter::buildProblem(*task, problem, reference, adapterError));
    QVERIFY(reference.has_value());
    QCOMPARE(problem.region.noGoRegions.size(), std::size_t{1});

    std::vector<Point2D> canonicalPath;
    canonicalPath.reserve(planningResult.path.size());
    for (const GeoPoint& point : planningResult.path) {
        const std::optional<Point2D> local = reference->toLocal(point);
        QVERIFY(local.has_value());
        canonicalPath.push_back(*local);
    }
    const CoverageFreeSpaceResult freeSpace = buildCoverageFreeSpace(problem);
    QCOMPARE(freeSpace.status, PlanningStatus::Success);
    const QSet<int> targetedPathIndices =
        targetedStopPathIndices(canonicalPath, freeSpace.freeSpace.executionTrackFeasibleRegion);
    if (definition->stopPolicy == DiagnosticStopPolicy::TargetedStop) {
        QVERIFY(!targetedPathIndices.isEmpty());
        QVERIFY(targetedPathIndices.size() < static_cast<qsizetype>(canonicalPath.size()));
    }

    QList<MissionItem*> missionItems = missionItemsFromController(controller, vehicle);
    const int missionItemListPathOffset = missionItems.size() - static_cast<int>(planningResult.path.size());
    QCOMPARE(missionItemListPathOffset, 1);
    const int missionSequenceOffset = vehicle->firmwarePlugin()->sendHomePositionToVehicle() ? 1 : 0;
    QCOMPARE(missionItems.size(), static_cast<qsizetype>(planningResult.path.size()) + missionItemListPathOffset);
    int stopWaypointCount = 0;
    QJsonArray stopPathIndicesJson;
    for (int pathIndex = 0; pathIndex < static_cast<int>(planningResult.path.size()); ++pathIndex) {
        MissionItem* missionItem = missionItems.at(pathIndex + missionItemListPathOffset);
        QCOMPARE(missionItem->command(), MAV_CMD_NAV_WAYPOINT);
        const bool stop =
            (definition->stopPolicy == DiagnosticStopPolicy::AllStop) ||
            ((definition->stopPolicy == DiagnosticStopPolicy::TargetedStop) && targetedPathIndices.contains(pathIndex));
        if (stop) {
            missionItem->setParam1(1.0);
            ++stopWaypointCount;
            stopPathIndicesJson.append(pathIndex);
        }
    }

    const double requestedWaypointSpeedMps = baselineWaypointSpeedMps * definition->speedFactor;
    const QString stagePath = QDir(runDirectory).filePath(QStringLiteral("stage.json"));
    (void) writeJson(stagePath, {{QStringLiteral("stage"), QStringLiteral("mission-items-ready")}});
    const bool radiusSet =
        setVehicleParameter(parameterManager, QStringLiteral("WP_RADIUS"), definition->waypointRadiusM);
    const bool speedSet = setVehicleParameter(parameterManager, QStringLiteral("WP_SPEED"), requestedWaypointSpeedMps);
    const QJsonObject appliedParameters = parameterSnapshot(parameterManager, diagnosticParameterNames);
    (void) writeJson(stagePath, {{QStringLiteral("stage"), QStringLiteral("parameters-applied")},
                                 {QStringLiteral("parameters"), appliedParameters}});

    const QString prepositionTlogPath = QDir(runDirectory).filePath(QStringLiteral("preposition.tlog"));
    MissionRunResult prepositionRun;
    const bool prepositioned = radiusSet && speedSet &&
                               prepositionAtFirstWaypoint(vehicle, missionItems, missionItemListPathOffset,
                                                          missionSequenceOffset, prepositionTlogPath, prepositionRun);
    (void) writeJson(stagePath,
                     {{QStringLiteral("stage"), QStringLiteral("preposition-finished")},
                      {QStringLiteral("success"), prepositioned},
                      {QStringLiteral("readyToFly"), prepositionRun.readyToFly},
                      {QStringLiteral("enteredAuto"), prepositionRun.enteredAuto},
                      {QStringLiteral("missionComplete"), prepositionRun.missionComplete},
                      {QStringLiteral("timedOut"), prepositionRun.timedOut},
                      {QStringLiteral("trajectorySamples"), static_cast<qint64>(prepositionRun.trajectory.size())}});
    bool uploaded = false;
    bool sequenceSelected = false;
    MissionRunResult missionRun;
    const QString tlogPath = QDir(runDirectory).filePath(definition->id + QStringLiteral(".tlog"));
    if (prepositioned) {
        uploaded = uploadMissionItems(vehicle, missionItems);
        (void) writeJson(stagePath, {{QStringLiteral("stage"), QStringLiteral("diagnostic-upload-finished")},
                                     {QStringLiteral("success"), uploaded}});
        if (uploaded) {
            vehicle->setCurrentMissionSequence(missionSequenceOffset);
            QElapsedTimer sequenceWait;
            sequenceWait.start();
            while ((vehicle->missionManager()->currentIndex() != missionSequenceOffset) &&
                   (sequenceWait.elapsed() < ConnectionTimeoutMs)) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
                QTest::qWait(50);
            }
            sequenceSelected = vehicle->missionManager()->currentIndex() == missionSequenceOffset;
        }
        if (sequenceSelected) {
            QFile tlog(tlogPath);
            if (tlog.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                const int finalSequence = static_cast<int>(planningResult.path.size()) - 1 + missionSequenceOffset;
                missionRun = executeMission(vehicle, finalSequence, tlog);
                tlog.close();
                (void) writeJson(stagePath, {{QStringLiteral("stage"), QStringLiteral("diagnostic-run-finished")},
                                             {QStringLiteral("missionComplete"), missionRun.missionComplete},
                                             {QStringLiteral("timedOut"), missionRun.timedOut},
                                             {QStringLiteral("trajectorySamples"),
                                              static_cast<qint64>(missionRun.trajectory.size())}});
            }
        }
    } else {
        qDeleteAll(missionItems);
    }

    const bool executionReset = resetMissionExecutionState(vehicle);
    const bool radiusRestored =
        setVehicleParameter(parameterManager, QStringLiteral("WP_RADIUS"), baselineWaypointRadiusM);
    const bool speedRestored =
        setVehicleParameter(parameterManager, QStringLiteral("WP_SPEED"), baselineWaypointSpeedMps);
    const QJsonObject restoredParameters = parameterSnapshot(parameterManager, diagnosticParameterNames);

    const QString csvPath = QDir(runDirectory).filePath(definition->id + QStringLiteral("-trajectory.csv"));
    const bool csvWritten = writeTrajectoryCsv(csvPath, missionRun.trajectory);
    const NoGoExecutionResult noGoExecution = analyzeNoGoExecution(missionRun, problem, *reference);
    CoveragePlanningSolution plotSolution;
    plotSolution.status = PlanningStatus::Success;
    plotSolution.path = canonicalPath;
    plotSolution.legRoles = planningResult.legRoles;
    const QString overlayPath = QDir(runDirectory).filePath(definition->id + QStringLiteral("-overlay.png"));
    const bool overlayWritten =
        renderEvidence(overlayPath, definition->id, problem, plotSolution, noGoExecution.actualLocal);

    QJsonArray incidentSequencesJson;
    QList<int> incidentSequences = noGoExecution.incidentSequences.values();
    std::sort(incidentSequences.begin(), incidentSequences.end());
    for (const int sequence : incidentSequences) {
        incidentSequencesJson.append(sequence);
    }
    QJsonArray reachedSequencesJson;
    for (const int sequence : missionRun.reachedSequences) {
        reachedSequencesJson.append(sequence);
    }
    QJsonArray statusTextsJson;
    for (const QString& statusText : missionRun.statusTexts) {
        statusTextsJson.append(statusText);
    }

    const QString parameterPath = QDir(runDirectory).filePath(QStringLiteral("parameter-snapshot.json"));
    const QJsonObject parameterEvidence{
        {QStringLiteral("firmwareVersion"), QStringLiteral("%1.%2.%3")
                                                .arg(vehicle->firmwareMajorVersion())
                                                .arg(vehicle->firmwareMinorVersion())
                                                .arg(vehicle->firmwarePatchVersion())},
        {QStringLiteral("run"), definition->id},
        {QStringLiteral("baseline"), baselineParameters},
        {QStringLiteral("applied"), appliedParameters},
        {QStringLiteral("restored"), restoredParameters},
        {QStringLiteral("recordedAtUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
    };
    const bool parametersWritten = writeJson(parameterPath, parameterEvidence);

    const QJsonObject resultJson{
        {QStringLiteral("run"), definition->id},
        {QStringLiteral("sourcePlan"), planPath},
        {QStringLiteral("planSha256"), QString::fromLatin1(planHash)},
        {QStringLiteral("canonicalPathPoints"), static_cast<qint64>(canonicalPath.size())},
        {QStringLiteral("geometry"), QStringLiteral("frozen S04")},
        {QStringLiteral("wpRadiusM"), definition->waypointRadiusM},
        {QStringLiteral("wpSpeedMps"), requestedWaypointSpeedMps},
        {QStringLiteral("stopPolicy"), diagnosticStopPolicyName(definition->stopPolicy)},
        {QStringLiteral("stopWaypointCount"), stopWaypointCount},
        {QStringLiteral("stopPathIndices"), stopPathIndicesJson},
        {QStringLiteral("targetedStopBoundaryDistanceM"), TargetedStopBoundaryDistanceM},
        {QStringLiteral("targetedStopMinimumTurnDeg"), TargetedStopMinimumTurnDeg},
        {QStringLiteral("prepositioned"), prepositioned},
        {QStringLiteral("missionUploaded"), uploaded},
        {QStringLiteral("missionSequenceSelected"), sequenceSelected},
        {QStringLiteral("missionComplete"), missionRun.missionComplete},
        {QStringLiteral("timedOut"), missionRun.timedOut},
        {QStringLiteral("reachedSequences"), reachedSequencesJson},
        {QStringLiteral("statusTexts"), statusTextsJson},
        {QStringLiteral("trajectorySampleCount"), static_cast<qint64>(missionRun.trajectory.size())},
        {QStringLiteral("insideOriginalNoGoSampleCount"), noGoExecution.insideSampleCount},
        {QStringLiteral("enteredOriginalNoGo"), noGoExecution.insideSampleCount > 0},
        {QStringLiteral("maximumPenetrationM"), noGoExecution.maximumPenetrationM},
        {QStringLiteral("minimumSignedClearanceM"), std::isfinite(noGoExecution.minimumSignedClearanceM)
                                                        ? QJsonValue(noGoExecution.minimumSignedClearanceM)
                                                        : QJsonValue()},
        {QStringLiteral("incidentMissionSequences"), incidentSequencesJson},
        {QStringLiteral("tlogSha256"), QString::fromLatin1(sha256(tlogPath))},
        {QStringLiteral("trajectoryCsvSha256"), QString::fromLatin1(sha256(csvPath))},
        {QStringLiteral("overlaySha256"), QString::fromLatin1(sha256(overlayPath))},
        {QStringLiteral("parameterSnapshotSha256"), QString::fromLatin1(sha256(parameterPath))},
        {QStringLiteral("parametersRestored"), radiusRestored && speedRestored},
        {QStringLiteral("executionStateReset"), executionReset},
    };
    const bool resultWritten = writeJson(QDir(runDirectory).filePath(QStringLiteral("result.json")), resultJson);

    QVERIFY(radiusSet);
    QVERIFY(speedSet);
    QVERIFY(prepositioned);
    QVERIFY(uploaded);
    QVERIFY(sequenceSelected);
    QVERIFY2(missionRun.readyToFly,
             qPrintable(QStringLiteral("Rover did not become ready: %1").arg(missionRun.prearmError)));
    QVERIFY(missionRun.started);
    QVERIFY(missionRun.enteredAuto);
    QVERIFY(!missionRun.timedOut);
    QVERIFY(missionRun.missionComplete);
    QVERIFY(executionReset);
    QVERIFY(radiusRestored);
    QVERIFY(speedRestored);
    QVERIFY(csvWritten);
    QVERIFY(overlayWritten);
    QVERIFY(parametersWritten);
    QVERIFY(resultWritten);
}

UT_REGISTER_TEST_STANDALONE(MarineSITLValidationTest, TestLabel::Integration, TestLabel::Vehicle,
                            TestLabel::MissionManager, TestLabel::Network, TestLabel::Serial, TestLabel::Slow)
