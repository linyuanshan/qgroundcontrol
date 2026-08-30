#pragma once

#include "QGCCorePlugin.h"

class CustomPlugin final : public QGCCorePlugin
{
    Q_OBJECT

public:
    explicit CustomPlugin(QObject* parent = nullptr);

    static QGCCorePlugin* instance();
};
