#pragma once

#include <QtCore/QVariantMap>

#include "PlanningResult.h"

namespace Marine::QGC {

/// Converts already published geographic facts for display; never evaluates or certifies them.
QVariantMap planningPresentation(const PlanningResult& result, bool current, bool stale);

}  // namespace Marine::QGC
