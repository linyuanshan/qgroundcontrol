#pragma once

#include <QtCore/QPointer>
#include <QtCore/QVariantList>

#include <limits>
#include <string>

#include "ComplexMissionItem.h"
#include "ICoveragePlanner.h"
#include "MarinePlanContext.h"

class CoverageInspectionComplexItem final : public ComplexMissionItem
{
    Q_OBJECT

public:
    enum PlanningState
    {
        Unplanned,
        Planned,
    };
    Q_ENUM(PlanningState)

    explicit CoverageInspectionComplexItem(PlanMasterController* masterController, bool flyView,
                                           Marine::MarinePlanContext* marineContext);

    Q_PROPERTY(QString taskId READ taskId NOTIFY taskIdChanged)
    Q_PROPERTY(QString taskName READ taskName WRITE setTaskName NOTIFY taskDataChanged)
    Q_PROPERTY(double swathWidthM READ swathWidthM WRITE setSwathWidthM NOTIFY taskDataChanged)
    Q_PROPERTY(double safetyMarginM READ safetyMarginM WRITE setSafetyMarginM NOTIFY taskDataChanged)
    Q_PROPERTY(bool cameraEnabled READ cameraEnabled WRITE setCameraEnabled NOTIFY taskDataChanged)
    Q_PROPERTY(bool cameraRecord READ cameraRecord WRITE setCameraRecord NOTIFY taskDataChanged)
    Q_PROPERTY(bool sonarEnabled READ sonarEnabled WRITE setSonarEnabled NOTIFY taskDataChanged)
    Q_PROPERTY(bool sonarRecord READ sonarRecord WRITE setSonarRecord NOTIFY taskDataChanged)
    Q_PROPERTY(QVariantList outerBoundary READ outerBoundary NOTIFY taskDataChanged)
    Q_PROPERTY(QVariantList noGoRegions READ noGoRegions NOTIFY taskDataChanged)
    Q_PROPERTY(PlanningState planningState READ planningState NOTIFY planningStateChanged)
    Q_PROPERTY(QVariantList generatedPath READ generatedPath NOTIFY generatedPathChanged)

    static constexpr const char* canonicalName = "Coverage Inspection";
    static constexpr const char* jsonComplexItemTypeValue = "coverageInspection";

    QString taskId() const;
    void setTaskId(const QString& taskId);

    QString taskName() const;
    void setTaskName(const QString& taskName);
    double swathWidthM() const;
    void setSwathWidthM(double swathWidthM);
    double safetyMarginM() const;
    void setSafetyMarginM(double safetyMarginM);
    bool cameraEnabled() const;
    void setCameraEnabled(bool enabled);
    bool cameraRecord() const;
    void setCameraRecord(bool record);
    bool sonarEnabled() const;
    void setSonarEnabled(bool enabled);
    bool sonarRecord() const;
    void setSonarRecord(bool record);
    QVariantList outerBoundary() const;
    QVariantList noGoRegions() const;

    PlanningState planningState() const { return _planningState; }

    QVariantList generatedPath() const;

    const Marine::PlanningResult& planningResult() const { return _planningResult; }

    Q_INVOKABLE bool plan();
    Q_INVOKABLE void invalidatePlan();

    QString patternName() const final { return tr(canonicalName); }

    double complexDistance() const final { return _planningResult.pathLengthM; }

    double minAMSLAltitude() const final;
    double maxAMSLAltitude() const final;

    int lastSequenceNumber() const final;

    bool load(const QJsonObject& complexObject, int sequenceNumber, QString& errorString) final;
    double greatestDistanceTo(const QGeoCoordinate& other) const final;

    QString mapVisualQML() const final
    {
        return QStringLiteral("qrc:/qml/Marine/Plan/CoverageInspectionMapVisual.qml");
    }

    bool dirty() const final { return _dirty; }

    bool isSimpleItem() const final { return false; }

    bool isStandaloneCoordinate() const final { return false; }

    bool specifiesCoordinate() const final { return !_planningResult.path.empty(); }

    bool specifiesAltitudeOnly() const final { return false; }

    QString commandDescription() const final { return tr(canonicalName); }

    QString commandName() const final { return tr(canonicalName); }

    QString abbreviation() const final { return QStringLiteral("CI"); }

    QGeoCoordinate coordinate() const final;

    QGeoCoordinate entryCoordinate() const final { return coordinate(); }

    QGeoCoordinate exitCoordinate() const final;
    bool exitCoordinateSameAsEntry() const final;

    double editableAlt() const final { return std::numeric_limits<double>::quiet_NaN(); }

    double amslEntryAlt() const final { return coordinate().altitude(); }

    double amslExitAlt() const final { return exitCoordinate().altitude(); }

    int sequenceNumber() const final { return _sequenceNumber; }

    double specifiedFlightSpeed() final { return std::numeric_limits<double>::quiet_NaN(); }

    double specifiedGimbalYaw() final { return std::numeric_limits<double>::quiet_NaN(); }

    double specifiedGimbalPitch() final { return std::numeric_limits<double>::quiet_NaN(); }

    void appendMissionItems(QList<MissionItem*>& items, QObject* missionItemParent) final;
    void setMissionFlightStatus(MissionFlightStatus_t& missionFlightStatus) final;
    void applyNewAltitude(double newAltitude) final;

    double additionalTimeDelay() const final { return 0.0; }

    ReadyForSaveState readyForSaveState() const final;
    void setDirty(bool dirty) final;
    void setCoordinate(const QGeoCoordinate& coordinate) final;
    void setSequenceNumber(int sequenceNumber) final;
    void save(QJsonArray& missionItems) final;

signals:
    void taskIdChanged();
    void taskDataChanged();
    void planningStateChanged();
    void generatedPathChanged();

private:
    void _applyPlanningResult(Marine::PlanningResult result);
    const Marine::MarineTask* _task() const;
    void _replaceTask(const Marine::MarineTask& task);
    static QVariantList _toQGeoCoordinates(const Marine::GeoPolygon& polygon);
    static QGeoCoordinate _toQGeoCoordinate(const Marine::GeoPoint& point);

    std::string _taskId;
    Marine::PlanningResult _planningResult;
    QPointer<Marine::MarinePlanContext> _marineContext;
    PlanningState _planningState = Unplanned;
    int _sequenceNumber = 0;

    static constexpr const char* _jsonTaskIdKey = "taskId";
    static constexpr const char* _jsonPlanningStatusKey = "planningStatus";
    static constexpr const char* _jsonGeneratedPathKey = "generatedPath";
    static constexpr const char* _jsonPathLengthKey = "pathLengthM";
    static constexpr const char* _jsonPlanningMessageKey = "planningMessage";
};
