#include "MarineTaskJsonCodec.h"

#include <QtCore/QJsonArray>

#include <utility>

#include "JsonParsing.h"

namespace {

constexpr int currentVersion = 1;
constexpr const char* versionKey = "version";
constexpr const char* idKey = "id";
constexpr const char* typeKey = "type";
constexpr const char* nameKey = "name";
constexpr const char* vehicleIdKey = "vehicleId";
constexpr const char* regionKey = "region";
constexpr const char* outerBoundaryKey = "outerBoundary";
constexpr const char* noGoRegionsKey = "noGoRegions";
constexpr const char* coverageKey = "coverage";
constexpr const char* swathWidthMKey = "swathWidthM";
constexpr const char* safetyMarginMKey = "safetyMarginM";
constexpr const char* plannerKey = "planner";
constexpr const char* plannerIdKey = "id";
constexpr const char* sensorsKey = "sensors";
constexpr const char* cameraEnabledKey = "cameraEnabled";
constexpr const char* cameraRecordKey = "cameraRecord";
constexpr const char* sonarEnabledKey = "sonarEnabled";
constexpr const char* sonarRecordKey = "sonarRecord";
constexpr const char* latitudeKey = "lat";
constexpr const char* longitudeKey = "lon";
constexpr const char* coverageInspectionType = "coverageInspection";

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
        loadedPolygon.vertices.push_back(
            {pointObject.value(latitudeKey).toDouble(), pointObject.value(longitudeKey).toDouble(), 0.0});
    }

    polygon = std::move(loadedPolygon);
    return true;
}

}  // namespace

namespace Marine {

bool MarineTaskJsonCodec::save(const MarineTask& task, QJsonObject& json, QString& errorString)
{
    errorString.clear();
    if (!task.isValid()) {
        errorString = QStringLiteral("Cannot save invalid marine task");
        return false;
    }
    if (task.type != MarineTaskType::CoverageInspection) {
        errorString = QStringLiteral("Unsupported marine task type");
        return false;
    }

    QJsonArray noGoRegions;
    for (const GeoPolygon& polygon : task.region.noGoRegions) {
        noGoRegions.append(savePolygon(polygon));
    }

    const QJsonObject regionObject{{outerBoundaryKey, savePolygon(task.region.outerBoundary)},
                                   {noGoRegionsKey, noGoRegions}};
    const QJsonObject coverageObject{{swathWidthMKey, task.coverage.swathWidthM},
                                     {safetyMarginMKey, task.coverage.safetyMarginM}};
    const QJsonObject plannerObject{{plannerIdKey, QString::fromStdString(task.planner.plannerId)}};
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
    if (json.value(versionKey).toInt() != currentVersion) {
        errorString = QStringLiteral("Unsupported marine task version");
        return false;
    }

    const QList<JsonParsing::KeyValidateInfo> taskKeys = {
        {versionKey, QJsonValue::Double, true},   {idKey, QJsonValue::String, true},
        {typeKey, QJsonValue::String, true},      {nameKey, QJsonValue::String, true},
        {vehicleIdKey, QJsonValue::String, true}, {regionKey, QJsonValue::Object, true},
        {coverageKey, QJsonValue::Object, true},  {plannerKey, QJsonValue::Object, true},
        {sensorsKey, QJsonValue::Object, true},
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
        {outerBoundaryKey, QJsonValue::Array, true},
        {noGoRegionsKey, QJsonValue::Array, true},
    };
    if (!JsonParsing::validateKeys(regionObject, regionKeys, errorString)) {
        return false;
    }

    const QJsonObject coverageObject = json.value(coverageKey).toObject();
    const QList<JsonParsing::KeyValidateInfo> coverageKeys = {
        {swathWidthMKey, QJsonValue::Double, true},
        {safetyMarginMKey, QJsonValue::Double, true},
    };
    if (!JsonParsing::validateKeys(coverageObject, coverageKeys, errorString)) {
        return false;
    }

    const QJsonObject plannerObject = json.value(plannerKey).toObject();
    const QList<JsonParsing::KeyValidateInfo> plannerKeys = {
        {plannerIdKey, QJsonValue::String, true},
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
    if (!loadPolygon(regionObject.value(outerBoundaryKey).toArray(), loadedTask.region.outerBoundary, errorString)) {
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
    loadedTask.coverage.safetyMarginM = coverageObject.value(safetyMarginMKey).toDouble();
    loadedTask.planner.plannerId = plannerObject.value(plannerIdKey).toString().toStdString();
    loadedTask.sensors.cameraEnabled = sensorsObject.value(cameraEnabledKey).toBool();
    loadedTask.sensors.cameraRecord = sensorsObject.value(cameraRecordKey).toBool();
    loadedTask.sensors.sonarEnabled = sensorsObject.value(sonarEnabledKey).toBool();
    loadedTask.sensors.sonarRecord = sensorsObject.value(sonarRecordKey).toBool();

    if (!loadedTask.isValid()) {
        errorString = QStringLiteral("Invalid marine task");
        return false;
    }

    task = std::move(loadedTask);
    return true;
}

}  // namespace Marine
