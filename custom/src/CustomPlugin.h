#pragma once

#include "QGCCorePlugin.h"

class CustomPlugin final : public QGCCorePlugin
{
    Q_OBJECT

public:
    explicit CustomPlugin(QObject* parent = nullptr);

    static QGCCorePlugin* instance();

    QVariantList complexMissionItemNames(Vehicle* vehicle) final;
    ComplexMissionItem* createComplexMissionItem(const QString& complexItemType, PlanMasterController* masterController,
                                                 bool flyView, const QString& kmlOrShpFile = QString()) final;
    QList<PlanCreator*> planCreators(PlanMasterController* planMasterController) final;
    void postSaveToJson(PlanMasterController* planMasterController, QJsonObject& json) final;
    bool preLoadFromJson(PlanMasterController* planMasterController, QJsonObject& json, QString& errorString) final;
};
