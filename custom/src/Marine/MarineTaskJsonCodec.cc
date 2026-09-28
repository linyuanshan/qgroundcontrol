#include "MarineTaskJsonCodec.h"

#include <QtCore/QJsonArray>

#include <cmath>
#include <utility>

#include "JsonParsing.h"

namespace {

constexpr int currentVersion = 3;
constexpr const char* versionKey = "version";
constexpr const char* idKey = "id";
constexpr const char* typeKey = "type";
constexpr const char* nameKey = "name";
constexpr const char* vehicleIdKey = "vehicleId";
constexpr const char* regionKey = "region";
constexpr const char* coverageBoundaryKey = "coverageBoundary";
constexpr const char* navigationBoundaryKey = "navigationBoundary";
constexpr const char* noGoRegionsKey = "noGoRegions";
constexpr const char* coverageKey = "coverage";
constexpr const char* swathWidthMKey = "swathWidthM";
constexpr const char* coverageRequirementKey = "coverageRequirement";
constexpr const char* sweepAngleModeKey = "sweepAngleMode";
constexpr const char* sweepAngleDegKey = "sweepAngleDeg";
constexpr const char* plannerKey = "planner";
constexpr const char* safetyKey = "safety";
constexpr const char* hardSafetyMarginMKey = "hardSafetyMarginM";
constexpr const char* preferredSafetyMarginMKey = "preferredSafetyMarginM";
constexpr const char* plannerIdKey = "plannerId";
constexpr const char* executionSafetyKey = "executionSafety";
constexpr const char* executionMarginMKey = "executionMarginM";
constexpr const char* sensorsKey = "sensors";
constexpr const char* cameraEnabledKey = "cameraEnabled";
constexpr const char* cameraRecordKey = "cameraRecord";
constexpr const char* sonarEnabledKey = "sonarEnabled";
constexpr const char* sonarRecordKey = "sonarRecord";
constexpr const char* latitudeKey = "lat";
constexpr const char* longitudeKey = "lon";
constexpr const char* coverageInspectionType = "coverageInspection";

QString sweepAngleModeToString(Marine::SweepAngleMode mode)
{
    return mode == Marine::SweepAngleMode::Manual ? QStringLiteral("manual") : QStringLiteral("auto");
}

QString coverageRequirementToString(Marine::CoverageRequirement requirement)
{
    return requirement == Marine::CoverageRequirement::Strict ? QStringLiteral("strict") : QStringLiteral("standard");
}

bool coverageRequirementFromString(const QString& value, Marine::CoverageRequirement& requirement)
{
    if (value == QStringLiteral("standard")) {
        requirement = Marine::CoverageRequirement::Standard;
        return true;
    }
    if (value == QStringLiteral("strict")) {
        requirement = Marine::CoverageRequirement::Strict;
        return true;
    }
    return false;
}

double normalizedManualAngle(double angle)
{
    const double normalized = std::fmod(angle, 180.0);
    return normalized < 0.0 ? normalized + 180.0 : normalized;
}

bool sweepAngleModeFromString(const QString& value, Marine::SweepAngleMode& mode)
{
    if (value == QStringLiteral("auto")) {
        mode = Marine::SweepAngleMode::Auto;
        return true;
    }
    if (value == QStringLiteral("manual")) {
        mode = Marine::SweepAngleMode::Manual;
        return true;
    }
    return false;
}

QJsonArray savePolygon(const Marine::GeoPolygon& polygon)
{
    QJsonArray json;
    for (const Marine::GeoPoint& point : polygon.vertices) {
        json.append(QJsonObject{{latitudeKey, point.latitudeDeg}, {longitudeKey, point.longitudeDeg}});
    }
    return json;
}

bool loadPolygon(const QJsonArray& json, Marine::GeoPolygon& polygon, QString& errorString)
{
    Marine::GeoPolygon loadedPolygon;
    loadedPolygon.vertices.reserve(static_cast<std::size_t>(json.size()));

    const QList<JsonParsing::KeyValidateInfo> pointKeys = {
        {latitudeKey, QJsonValue::Double, true},
        {longitudeKey, QJsonValue::Double, true},
    };
    for (const QJsonValue& value : json) {
        if (!value.isObject()) {
            errorString = QStringLiteral("Marine task polygon point must be an object");
            return false;
        }

        const QJsonObject pointObject = value.toObject();
        if (!JsonParsing::validateKeys(pointObject, pointKeys, errorString)) {
            return false;
        }
        const double latitude = pointObject.value(latitudeKey).toDouble();
        const double longitude = pointObject.value(longitudeKey).toDouble();
        if (!std::isfinite(latitude) || !std::isfinite(longitude) || (latitude < -90.0) || (latitude > 90.0) ||
            (longitude < -180.0) || (longitude > 180.0)) {
            errorString = QStringLiteral("Marine task coordinate must be finite and within latitude/longitude range");
            return false;
        }
        loadedPolygon.vertices.push_back({latitude, longitude, 0.0});
    }

    polygon = std::move(loadedPolygon);
    return true;
}

}  // namespace

namespace Marine {

bool MarineTaskJsonCodec::save(const MarineTask& task, QJsonObject& json, QString& errorString)
{
    errorString.clear();
    if (!task.schemaValid()) {
        errorString = QStringLiteral("Cannot save invalid marine task");
        return false;
    }
    QJsonArray noGoRegions;
    for (const GeoPolygon& polygon : task.region.noGoRegions) {
        noGoRegions.append(savePolygon(polygon));
    }

    const QJsonObject regionObject{{coverageBoundaryKey, savePolygon(task.region.coverageBoundary)},
                                   {navigationBoundaryKey, savePolygon(task.region.navigationBoundary)},
                                   {noGoRegionsKey, noGoRegions}};
    const QJsonObject coverageObject{
        {swathWidthMKey, task.coverage.swathWidthM},
        {coverageRequirementKey, coverageRequirementToString(task.coverage.coverageRequirement)},
        {sweepAngleModeKey, sweepAngleModeToString(task.coverage.sweepAngleMode)},
        {sweepAngleDegKey, task.coverage.sweepAngleMode == SweepAngleMode::Manual
                               ? normalizedManualAngle(task.coverage.sweepAngleDeg)
                               : task.coverage.sweepAngleDeg}};
    const QJsonObject safetyObject{{hardSafetyMarginMKey, task.safety.hardSafetyMarginM},
                                   {preferredSafetyMarginMKey, task.safety.preferredSafetyMarginM}};
    const QJsonObject plannerObject{
        {plannerIdKey, QString::fromStdString(task.planner.plannerId)},
        {executionSafetyKey, QJsonObject{{executionMarginMKey, task.planner.executionSafety.executionMarginM}}}};
    const QJsonObject sensorsObject{{cameraEnabledKey, task.sensors.cameraEnabled},
                                    {cameraRecordKey, task.sensors.cameraRecord},
                                    {sonarEnabledKey, task.sensors.sonarEnabled},
                                    {sonarRecordKey, task.sensors.sonarRecord}};

    json = QJsonObject{{versionKey, currentVersion},
                       {idKey, QString::fromStdString(task.id)},
                       {typeKey, coverageInspectionType},
                       {nameKey, QString::fromStdString(task.name)},
                       {vehicleIdKey, QString::fromStdString(task.vehicleId)},
                       {regionKey, regionObject},
                       {coverageKey, coverageObject},
                       {safetyKey, safetyObject},
                       {plannerKey, plannerObject},
                       {sensorsKey, sensorsObject}};
    return true;
}

bool MarineTaskJsonCodec::load(const QJsonObject& json, MarineTask& task, QString& errorString)
{
    errorString.clear();
    const QList<JsonParsing::KeyValidateInfo> versionKeys = {
        {versionKey, QJsonValue::Double, true},
    };
    if (!JsonParsing::validateKeys(json, versionKeys, errorString)) {
        return false;
    }
    const double versionValue = json.value(versionKey).toDouble();
    if (!std::isfinite(versionValue) || (versionValue != currentVersion)) {
        errorString = (versionValue == 1.0 || versionValue == 2.0)
                          ? QStringLiteral("Unsupported development schema: MarineTask version %1").arg(versionValue)
                          : QStringLiteral("Unsupported MarineTask version");
        return false;
    }

    const QList<JsonParsing::KeyValidateInfo> taskKeys = {
        {versionKey, QJsonValue::Double, true},   {idKey, QJsonValue::String, true},
        {typeKey, QJsonValue::String, true},      {nameKey, QJsonValue::String, true},
        {vehicleIdKey, QJsonValue::String, true}, {regionKey, QJsonValue::Object, true},
        {coverageKey, QJsonValue::Object, true},  {safetyKey, QJsonValue::Object, true},
        {plannerKey, QJsonValue::Object, true},   {sensorsKey, QJsonValue::Object, true},
    };
    if (!JsonParsing::validateKeys(json, taskKeys, errorString)) {
        return false;
    }
    if (json.value(typeKey).toString() != QLatin1String(coverageInspectionType)) {
        errorString = QStringLiteral("Unsupported marine task type");
        return false;
    }

    const QJsonObject regionObject = json.value(regionKey).toObject();
    const QList<JsonParsing::KeyValidateInfo> regionKeys = {
        {coverageBoundaryKey, QJsonValue::Array, true},
        {navigationBoundaryKey, QJsonValue::Array, true},
        {noGoRegionsKey, QJsonValue::Array, true},
    };
    if (!JsonParsing::validateKeys(regionObject, regionKeys, errorString)) {
        return false;
    }

    const QJsonObject coverageObject = json.value(coverageKey).toObject();
    const QList<JsonParsing::KeyValidateInfo> coverageKeys = {
        {swathWidthMKey, QJsonValue::Double, true},
        {coverageRequirementKey, QJsonValue::String, true},
        {sweepAngleModeKey, QJsonValue::String, true},
        {sweepAngleDegKey, QJsonValue::Double, true},
    };
    if (!JsonParsing::validateKeys(coverageObject, coverageKeys, errorString)) {
        return false;
    }

    const QJsonObject safetyObject = json.value(safetyKey).toObject();
    const QList<JsonParsing::KeyValidateInfo> safetyKeys = {
        {hardSafetyMarginMKey, QJsonValue::Double, true},
        {preferredSafetyMarginMKey, QJsonValue::Double, true},
    };
    if (!JsonParsing::validateKeys(safetyObject, safetyKeys, errorString)) {
        return false;
    }

    const QJsonObject plannerObject = json.value(plannerKey).toObject();
    const QList<JsonParsing::KeyValidateInfo> plannerKeys = {
        {plannerIdKey, QJsonValue::String, true},
        {executionSafetyKey, QJsonValue::Object, true},
    };
    if (!JsonParsing::validateKeys(plannerObject, plannerKeys, errorString)) {
        return false;
    }

    const QJsonObject sensorsObject = json.value(sensorsKey).toObject();
    const QList<JsonParsing::KeyValidateInfo> sensorKeys = {
        {cameraEnabledKey, QJsonValue::Bool, true},
        {cameraRecordKey, QJsonValue::Bool, true},
        {sonarEnabledKey, QJsonValue::Bool, true},
        {sonarRecordKey, QJsonValue::Bool, true},
    };
    if (!JsonParsing::validateKeys(sensorsObject, sensorKeys, errorString)) {
        return false;
    }

    MarineTask loadedTask;
    loadedTask.id = json.value(idKey).toString().toStdString();
    loadedTask.name = json.value(nameKey).toString().toStdString();
    loadedTask.type = MarineTaskType::CoverageInspection;
    loadedTask.vehicleId = json.value(vehicleIdKey).toString().toStdString();
    if (!loadPolygon(regionObject.value(coverageBoundaryKey).toArray(), loadedTask.region.coverageBoundary,
                     errorString) ||
        !loadPolygon(regionObject.value(navigationBoundaryKey).toArray(), loadedTask.region.navigationBoundary,
                     errorString)) {
        return false;
    }

    const QJsonArray noGoRegionArray = regionObject.value(noGoRegionsKey).toArray();
    for (const QJsonValue& value : noGoRegionArray) {
        if (!value.isArray()) {
            errorString = QStringLiteral("Marine task no-go region must be an array");
            return false;
        }
        GeoPolygon polygon;
        if (!loadPolygon(value.toArray(), polygon, errorString)) {
            return false;
        }
        loadedTask.region.noGoRegions.push_back(std::move(polygon));
    }

    loadedTask.coverage.swathWidthM = coverageObject.value(swathWidthMKey).toDouble();
    if (!coverageRequirementFromString(coverageObject.value(coverageRequirementKey).toString(),
                                       loadedTask.coverage.coverageRequirement)) {
        errorString = QStringLiteral("Invalid coverage requirement");
        return false;
    }
    if (!sweepAngleModeFromString(coverageObject.value(sweepAngleModeKey).toString(),
                                  loadedTask.coverage.sweepAngleMode)) {
        errorString = QStringLiteral("Invalid sweep angle mode");
        return false;
    }
    loadedTask.coverage.sweepAngleDeg = coverageObject.value(sweepAngleDegKey).toDouble();
    if (std::isfinite(loadedTask.coverage.sweepAngleDeg) &&
        loadedTask.coverage.sweepAngleMode == SweepAngleMode::Manual) {
        loadedTask.coverage.sweepAngleDeg = normalizedManualAngle(loadedTask.coverage.sweepAngleDeg);
    }
    loadedTask.safety.hardSafetyMarginM = safetyObject.value(hardSafetyMarginMKey).toDouble();
    loadedTask.safety.preferredSafetyMarginM = safetyObject.value(preferredSafetyMarginMKey).toDouble();
    loadedTask.planner.plannerId = plannerObject.value(plannerIdKey).toString().toStdString();
    {
        const QJsonObject executionSafetyObject = plannerObject.value(executionSafetyKey).toObject();
        const QList<JsonParsing::KeyValidateInfo> executionSafetyKeys = {
            {executionMarginMKey, QJsonValue::Double, true},
        };
        if (!JsonParsing::validateKeys(executionSafetyObject, executionSafetyKeys, errorString)) {
            return false;
        }
        const double executionMarginM = executionSafetyObject.value(executionMarginMKey).toDouble();
        if (!std::isfinite(executionMarginM) || (executionMarginM < 0.0)) {
            errorString = QStringLiteral("Execution margin must be finite and non-negative");
            return false;
        }
        loadedTask.planner.executionSafety.executionMarginM = executionMarginM;
    }
    loadedTask.sensors.cameraEnabled = sensorsObject.value(cameraEnabledKey).toBool();
    loadedTask.sensors.cameraRecord = sensorsObject.value(cameraRecordKey).toBool();
    loadedTask.sensors.sonarEnabled = sensorsObject.value(sonarEnabledKey).toBool();
    loadedTask.sensors.sonarRecord = sensorsObject.value(sonarRecordKey).toBool();

    if (!loadedTask.schemaValid()) {
        errorString = QStringLiteral("Invalid marine task");
        return false;
    }

    task = std::move(loadedTask);
    return true;
}

}  // namespace Marine
