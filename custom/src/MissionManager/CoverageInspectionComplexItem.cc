#include "CoverageInspectionComplexItem.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>

#include <algorithm>
#include <cmath>
#include <utility>

#include "ArduPilotMissionAdapter.h"
#include "GeoJsonHelper.h"
#include "JsonParsing.h"
#include "MarinePlanContext.h"

using namespace Marine;

namespace {

bool pathsEqual(const std::vector<GeoPoint>& first, const std::vector<GeoPoint>& second)
{
    return std::ranges::equal(first, second, [](const GeoPoint& firstPoint, const GeoPoint& secondPoint) {
        return (firstPoint.latitudeDeg == secondPoint.latitudeDeg) &&
               (firstPoint.longitudeDeg == secondPoint.longitudeDeg) && (firstPoint.altitudeM == secondPoint.altitudeM);
    });
}

QString planningStatusToString(PlanningStatus status)
{
    switch (status) {
        case PlanningStatus::Success:
            return QStringLiteral("success");
        case PlanningStatus::InvalidInput:
            return QStringLiteral("invalidInput");
        case PlanningStatus::Failed:
            return QStringLiteral("failed");
    }
    return {};
}

bool planningStatusFromString(const QString& value, PlanningStatus& status)
{
    if (value == QStringLiteral("success")) {
        status = PlanningStatus::Success;
        return true;
    }
    if (value == QStringLiteral("invalidInput")) {
        status = PlanningStatus::InvalidInput;
        return true;
    }
    if (value == QStringLiteral("failed")) {
        status = PlanningStatus::Failed;
        return true;
    }
    return false;
}

}  // namespace

CoverageInspectionComplexItem::CoverageInspectionComplexItem(PlanMasterController* masterController, bool flyView,
                                                             MarinePlanContext* marineContext)
    : ComplexMissionItem(masterController, flyView), _marineContext(marineContext)
{
    if (_marineContext) {
        connect(_marineContext, &MarinePlanContext::taskChanged, this, [this](const QString& changedTaskId) {
            if (changedTaskId == taskId()) {
                emit taskDataChanged();
                invalidatePlan();
            }
        });
        connect(_marineContext, &MarinePlanContext::tasksCleared, this, [this]() {
            if (!_taskId.empty()) {
                emit taskDataChanged();
                invalidatePlan();
            }
        });
    }
    _editorQml = QStringLiteral("qrc:/qml/Marine/Plan/CoverageInspectionEditor.qml");
    setDirty(false);
}

QString CoverageInspectionComplexItem::taskId() const
{
    return QString::fromStdString(_taskId);
}

void CoverageInspectionComplexItem::setTaskId(const QString& taskId)
{
    const std::string newTaskId = taskId.toStdString();
    if (_taskId == newTaskId) {
        return;
    }

    _taskId = newTaskId;
    emit taskIdChanged();
    emit taskDataChanged();
    invalidatePlan();
}

QString CoverageInspectionComplexItem::taskName() const
{
    const MarineTask* marineTask = _task();
    return (marineTask != nullptr) ? QString::fromStdString(marineTask->name) : QString();
}

void CoverageInspectionComplexItem::setTaskName(const QString& taskName)
{
    const MarineTask* marineTask = _task();
    if ((marineTask == nullptr) || (marineTask->name == taskName.toStdString())) {
        return;
    }
    MarineTask updatedTask = *marineTask;
    updatedTask.name = taskName.toStdString();
    _replaceTask(updatedTask);
}

double CoverageInspectionComplexItem::swathWidthM() const
{
    const MarineTask* marineTask = _task();
    return (marineTask != nullptr) ? marineTask->coverage.swathWidthM : 0.0;
}

void CoverageInspectionComplexItem::setSwathWidthM(double swathWidthM)
{
    const MarineTask* marineTask = _task();
    if ((marineTask == nullptr) || (marineTask->coverage.swathWidthM == swathWidthM)) {
        return;
    }
    MarineTask updatedTask = *marineTask;
    updatedTask.coverage.swathWidthM = swathWidthM;
    _replaceTask(updatedTask);
}

double CoverageInspectionComplexItem::safetyMarginM() const
{
    const MarineTask* marineTask = _task();
    return (marineTask != nullptr) ? marineTask->coverage.safetyMarginM : 0.0;
}

void CoverageInspectionComplexItem::setSafetyMarginM(double safetyMarginM)
{
    const MarineTask* marineTask = _task();
    if ((marineTask == nullptr) || (marineTask->coverage.safetyMarginM == safetyMarginM)) {
        return;
    }
    MarineTask updatedTask = *marineTask;
    updatedTask.coverage.safetyMarginM = safetyMarginM;
    _replaceTask(updatedTask);
}

bool CoverageInspectionComplexItem::cameraEnabled() const
{
    const MarineTask* marineTask = _task();
    return (marineTask != nullptr) && marineTask->sensors.cameraEnabled;
}

void CoverageInspectionComplexItem::setCameraEnabled(bool enabled)
{
    const MarineTask* marineTask = _task();
    if ((marineTask == nullptr) || (marineTask->sensors.cameraEnabled == enabled)) {
        return;
    }
    MarineTask updatedTask = *marineTask;
    updatedTask.sensors.cameraEnabled = enabled;
    _replaceTask(updatedTask);
}

bool CoverageInspectionComplexItem::cameraRecord() const
{
    const MarineTask* marineTask = _task();
    return (marineTask != nullptr) && marineTask->sensors.cameraRecord;
}

void CoverageInspectionComplexItem::setCameraRecord(bool record)
{
    const MarineTask* marineTask = _task();
    if ((marineTask == nullptr) || (marineTask->sensors.cameraRecord == record)) {
        return;
    }
    MarineTask updatedTask = *marineTask;
    updatedTask.sensors.cameraRecord = record;
    _replaceTask(updatedTask);
}

bool CoverageInspectionComplexItem::sonarEnabled() const
{
    const MarineTask* marineTask = _task();
    return (marineTask != nullptr) && marineTask->sensors.sonarEnabled;
}

void CoverageInspectionComplexItem::setSonarEnabled(bool enabled)
{
    const MarineTask* marineTask = _task();
    if ((marineTask == nullptr) || (marineTask->sensors.sonarEnabled == enabled)) {
        return;
    }
    MarineTask updatedTask = *marineTask;
    updatedTask.sensors.sonarEnabled = enabled;
    _replaceTask(updatedTask);
}

bool CoverageInspectionComplexItem::sonarRecord() const
{
    const MarineTask* marineTask = _task();
    return (marineTask != nullptr) && marineTask->sensors.sonarRecord;
}

void CoverageInspectionComplexItem::setSonarRecord(bool record)
{
    const MarineTask* marineTask = _task();
    if ((marineTask == nullptr) || (marineTask->sensors.sonarRecord == record)) {
        return;
    }
    MarineTask updatedTask = *marineTask;
    updatedTask.sensors.sonarRecord = record;
    _replaceTask(updatedTask);
}

QVariantList CoverageInspectionComplexItem::outerBoundary() const
{
    const MarineTask* marineTask = _task();
    return (marineTask != nullptr) ? _toQGeoCoordinates(marineTask->region.outerBoundary) : QVariantList();
}

QVariantList CoverageInspectionComplexItem::noGoRegions() const
{
    QVariantList regions;
    const MarineTask* marineTask = _task();
    if (marineTask == nullptr) {
        return regions;
    }
    regions.reserve(static_cast<qsizetype>(marineTask->region.noGoRegions.size()));
    for (const GeoPolygon& polygon : marineTask->region.noGoRegions) {
        regions.append(QVariant::fromValue(_toQGeoCoordinates(polygon)));
    }
    return regions;
}

QVariantList CoverageInspectionComplexItem::generatedPath() const
{
    QVariantList path;
    path.reserve(static_cast<qsizetype>(_planningResult.path.size()));
    for (const GeoPoint& point : _planningResult.path) {
        path.append(QVariant::fromValue(_toQGeoCoordinate(point)));
    }
    return path;
}

bool CoverageInspectionComplexItem::plan()
{
    PlanningResult result;
    if (!_marineContext) {
        result.message = "Marine plan context is unavailable";
        _applyPlanningResult(std::move(result));
        return false;
    }

    const MarineTask* task = _marineContext->task(_taskId);
    if (task == nullptr) {
        result.status = PlanningStatus::InvalidInput;
        result.message = "Marine task was not found";
        _applyPlanningResult(std::move(result));
        return false;
    }

    const std::shared_ptr<ICoveragePlanner> planner =
        _marineContext->plannerRegistry().planner(task->planner.plannerId);
    if (!planner) {
        result.message = "Coverage planner was not found";
        _applyPlanningResult(std::move(result));
        return false;
    }

    result = planner->plan(*task);
    if ((result.status == PlanningStatus::Success) &&
        (result.path.empty() || !std::isfinite(result.pathLengthM) || (result.pathLengthM < 0.0))) {
        result.status = PlanningStatus::Failed;
        result.message = "Coverage planner returned an invalid result";
    }
    if (result.status != PlanningStatus::Success) {
        result.path.clear();
        result.pathLengthM = 0.0;
    }
    const bool success = result.status == PlanningStatus::Success;
    _applyPlanningResult(std::move(result));
    return success;
}

void CoverageInspectionComplexItem::invalidatePlan()
{
    _applyPlanningResult({});
}

double CoverageInspectionComplexItem::minAMSLAltitude() const
{
    if (_planningResult.path.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return std::ranges::min_element(_planningResult.path, {}, &GeoPoint::altitudeM)->altitudeM;
}

double CoverageInspectionComplexItem::maxAMSLAltitude() const
{
    if (_planningResult.path.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return std::ranges::max_element(_planningResult.path, {}, &GeoPoint::altitudeM)->altitudeM;
}

QGeoCoordinate CoverageInspectionComplexItem::coordinate() const
{
    return _planningResult.path.empty() ? QGeoCoordinate() : _toQGeoCoordinate(_planningResult.path.front());
}

QGeoCoordinate CoverageInspectionComplexItem::exitCoordinate() const
{
    return _planningResult.path.empty() ? QGeoCoordinate() : _toQGeoCoordinate(_planningResult.path.back());
}

bool CoverageInspectionComplexItem::exitCoordinateSameAsEntry() const
{
    return !_planningResult.path.empty() && (entryCoordinate() == exitCoordinate());
}

double CoverageInspectionComplexItem::greatestDistanceTo(const QGeoCoordinate& other) const
{
    double greatestDistance = 0.0;
    for (const GeoPoint& point : _planningResult.path) {
        greatestDistance = std::max(greatestDistance, _toQGeoCoordinate(point).distanceTo(other));
    }
    return greatestDistance;
}

int CoverageInspectionComplexItem::lastSequenceNumber() const
{
    return _planningResult.path.empty() ? _sequenceNumber
                                        : _sequenceNumber + static_cast<int>(_planningResult.path.size()) - 1;
}

void CoverageInspectionComplexItem::appendMissionItems(QList<MissionItem*>& items, QObject* missionItemParent)
{
    int nextSequenceNumber = _sequenceNumber;
    QString errorString;
    if (!ArduPilotMissionAdapter::appendWaypoints(_planningResult, items, missionItemParent, nextSequenceNumber,
                                                  errorString)) {
        return;
    }
}

void CoverageInspectionComplexItem::setMissionFlightStatus(MissionFlightStatus_t& missionFlightStatus)
{
    ComplexMissionItem::setMissionFlightStatus(missionFlightStatus);
}

void CoverageInspectionComplexItem::applyNewAltitude(double newAltitude)
{
    Q_UNUSED(newAltitude)
}

VisualMissionItem::ReadyForSaveState CoverageInspectionComplexItem::readyForSaveState() const
{
    return (_planningState == Planned) ? ReadyForSave : NotReadyForSaveData;
}

void CoverageInspectionComplexItem::setDirty(bool dirty)
{
    if (_dirty != dirty) {
        _dirty = dirty;
        emit dirtyChanged(dirty);
    }
}

void CoverageInspectionComplexItem::setCoordinate(const QGeoCoordinate& coordinate)
{
    Q_UNUSED(coordinate)
}

void CoverageInspectionComplexItem::setSequenceNumber(int sequenceNumber)
{
    if (_sequenceNumber != sequenceNumber) {
        _sequenceNumber = sequenceNumber;
        emit sequenceNumberChanged(sequenceNumber);
        emit lastSequenceNumberChanged(lastSequenceNumber());
    }
}

void CoverageInspectionComplexItem::save(QJsonArray& missionItems)
{
    QJsonObject object;
    object.insert(JsonParsing::jsonVersionKey, 1);
    object.insert(VisualMissionItem::jsonTypeKey, VisualMissionItem::jsonTypeComplexItemValue);
    object.insert(ComplexMissionItem::jsonComplexItemTypeKey, jsonComplexItemTypeValue);
    object.insert(_jsonTaskIdKey, taskId());
    object.insert(_jsonPlanningStatusKey, planningStatusToString(_planningResult.status));
    object.insert(_jsonPathLengthKey, _planningResult.pathLengthM);
    object.insert(_jsonPlanningMessageKey, QString::fromStdString(_planningResult.message));

    QJsonValue pathValue;
    GeoJsonHelper::saveGeoCoordinateArray(generatedPath(), true, pathValue);
    object.insert(_jsonGeneratedPathKey, pathValue);
    missionItems.append(object);
}

bool CoverageInspectionComplexItem::load(const QJsonObject& object, int sequenceNumber, QString& errorString)
{
    const QList<JsonParsing::KeyValidateInfo> keyInfoList = {
        {.key = JsonParsing::jsonVersionKey, .type = QJsonValue::Double, .required = true},
        {.key = VisualMissionItem::jsonTypeKey, .type = QJsonValue::String, .required = true},
        {.key = ComplexMissionItem::jsonComplexItemTypeKey, .type = QJsonValue::String, .required = true},
        {.key = _jsonTaskIdKey, .type = QJsonValue::String, .required = true},
        {.key = _jsonPlanningStatusKey, .type = QJsonValue::String, .required = true},
        {.key = _jsonGeneratedPathKey, .type = QJsonValue::Array, .required = true},
        {.key = _jsonPathLengthKey, .type = QJsonValue::Double, .required = true},
        {.key = _jsonPlanningMessageKey, .type = QJsonValue::String, .required = true},
    };
    if (!JsonParsing::validateKeys(object, keyInfoList, errorString)) {
        return false;
    }
    if ((object.value(VisualMissionItem::jsonTypeKey).toString() != VisualMissionItem::jsonTypeComplexItemValue) ||
        (object.value(ComplexMissionItem::jsonComplexItemTypeKey).toString() != jsonComplexItemTypeValue)) {
        errorString = tr("Unsupported coverage inspection complex item type");
        return false;
    }
    if (object.value(JsonParsing::jsonVersionKey).toInt() != 1) {
        errorString = tr("Coverage inspection version is not supported");
        return false;
    }

    const std::string loadedTaskId = object.value(_jsonTaskIdKey).toString().toStdString();
    if ((_marineContext == nullptr) || (_marineContext->task(loadedTaskId) == nullptr)) {
        errorString = tr("Marine coverage item references missing task '%1'").arg(QString::fromStdString(loadedTaskId));
        return false;
    }

    PlanningStatus status;
    if (!planningStatusFromString(object.value(_jsonPlanningStatusKey).toString(), status)) {
        errorString = tr("Coverage inspection planning status is invalid");
        return false;
    }

    QVariantList path;
    if (!GeoJsonHelper::loadGeoCoordinateArray(object.value(_jsonGeneratedPathKey), true, path, errorString)) {
        return false;
    }

    PlanningResult result;
    result.status = status;
    result.pathLengthM = object.value(_jsonPathLengthKey).toDouble();
    result.message = object.value(_jsonPlanningMessageKey).toString().toStdString();
    result.path.reserve(static_cast<std::size_t>(path.size()));
    for (const QVariant& value : std::as_const(path)) {
        const auto coordinate = value.value<QGeoCoordinate>();
        result.path.push_back({.latitudeDeg = coordinate.latitude(),
                               .longitudeDeg = coordinate.longitude(),
                               .altitudeM = coordinate.altitude()});
    }
    if (!std::isfinite(result.pathLengthM) || (result.pathLengthM < 0.0)) {
        errorString = tr("Coverage inspection path length is invalid");
        return false;
    }
    if ((result.status == PlanningStatus::Success) && result.path.empty()) {
        errorString = tr("A successful coverage inspection must contain a generated path");
        return false;
    }
    if ((result.status != PlanningStatus::Success) && !result.path.empty()) {
        errorString = tr("An unplanned coverage inspection cannot contain a generated path");
        return false;
    }

    _taskId = loadedTaskId;
    _sequenceNumber = sequenceNumber;
    _applyPlanningResult(std::move(result));
    setDirty(false);
    return true;
}

void CoverageInspectionComplexItem::_applyPlanningResult(PlanningResult result)
{
    const PlanningState newState = (result.status == PlanningStatus::Success) ? Planned : Unplanned;
    const bool stateChanged = _planningState != newState;
    const bool incompleteChanged = _isIncomplete != (newState != Planned);
    const bool pathChanged = !pathsEqual(_planningResult.path, result.path);
    const bool distanceChanged = _planningResult.pathLengthM != result.pathLengthM;

    _planningResult = std::move(result);
    _planningState = newState;
    _isIncomplete = newState != Planned;

    if (stateChanged) {
        emit planningStateChanged();
    }
    if (incompleteChanged) {
        emit isIncompleteChanged();
    }
    if (pathChanged) {
        emit generatedPathChanged();
        emit lastSequenceNumberChanged(lastSequenceNumber());
        emit coordinateChanged(coordinate());
        emit entryCoordinateChanged(entryCoordinate());
        emit exitCoordinateChanged(exitCoordinate());
        emit exitCoordinateSameAsEntryChanged(exitCoordinateSameAsEntry());
        emit specifiesCoordinateChanged();
        emit minAMSLAltitudeChanged();
        emit maxAMSLAltitudeChanged();
    }
    if (distanceChanged) {
        emit complexDistanceChanged();
    }
    if (stateChanged) {
        emit readyForSaveStateChanged();
    }
    setDirty(true);
}

const MarineTask* CoverageInspectionComplexItem::_task() const
{
    return _marineContext ? _marineContext->task(_taskId) : nullptr;
}

void CoverageInspectionComplexItem::_replaceTask(const MarineTask& task)
{
    if (_marineContext) {
        (void) _marineContext->updateTask(task);
    }
}

QVariantList CoverageInspectionComplexItem::_toQGeoCoordinates(const GeoPolygon& polygon)
{
    QVariantList coordinates;
    coordinates.reserve(static_cast<qsizetype>(polygon.vertices.size()));
    for (const GeoPoint& point : polygon.vertices) {
        coordinates.append(QVariant::fromValue(_toQGeoCoordinate(point)));
    }
    return coordinates;
}

QGeoCoordinate CoverageInspectionComplexItem::_toQGeoCoordinate(const GeoPoint& point)
{
    return {point.latitudeDeg, point.longitudeDeg, point.altitudeM};
}
