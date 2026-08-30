#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QString>

#include "MarineTask.h"

namespace Marine {

class MarineTaskJsonCodec
{
public:
    static bool save(const MarineTask& task, QJsonObject& json, QString& errorString);
    static bool load(const QJsonObject& json, MarineTask& task, QString& errorString);
};

}  // namespace Marine
