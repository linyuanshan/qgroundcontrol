#include "PlanningInputIdentity.h"

#include <QtCore/QCryptographicHash>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "JsonParsing.h"

namespace {

void appendInteger(std::string& bytes, std::uint64_t value)
{
    for (int shift = 56; shift >= 0; shift -= 8) {
        bytes.push_back(static_cast<char>((value >> shift) & 0xff));
    }
}

void appendNumber(std::string& bytes, double value)
{
    static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
    appendInteger(bytes, std::bit_cast<std::uint64_t>(value == 0.0 ? 0.0 : value));
}

void appendString(std::string& bytes, const std::string& value)
{
    appendInteger(bytes, value.size());
    bytes.append(value);
}

std::string canonicalPolygon(const Marine::GeoPolygon& polygon)
{
    std::vector<std::string> points;
    for (std::size_t index = 0; index < Marine::openRingVertexCount(polygon); ++index) {
        const auto& point = polygon.vertices[index];
        std::string bytes;
        appendNumber(bytes, point.latitudeDeg);
        appendNumber(bytes, point.longitudeDeg);
        points.push_back(std::move(bytes));
    }
    std::string best;
    for (std::size_t start = 0; start < points.size(); ++start) {
        for (const bool reverse : {false, true}) {
            std::string candidate;
            appendInteger(candidate, points.size());
            for (std::size_t offset = 0; offset < points.size(); ++offset) {
                const auto index =
                    reverse ? (start + points.size() - offset) % points.size() : (start + offset) % points.size();
                candidate.append(points[index]);
            }
            if (best.empty() || candidate < best) {
                best = std::move(candidate);
            }
        }
    }
    return best;
}

}  // namespace

namespace Marine {

std::optional<PlanningInputIdentity> PlanningInputIdentity::fromTask(const MarineTask& task,
                                                                     const PlanningSemantics& semantics)
{
    if (!task.schemaValid() || semantics.planningVersion.isEmpty() || semantics.policyVersion.isEmpty() ||
        semantics.resolvedStrategy.isEmpty() || semantics.strategyVersion.isEmpty()) {
        return std::nullopt;
    }
    // Encoding v1: length-prefixed UTF-8, big-endian uint64 / IEEE binary64, no rounding.
    // Geometry is 2D, with ring rotation/winding and obstacle order canonicalized.
    std::string bytes;
    appendString(bytes, "marine.planning-input");
    appendInteger(bytes, 1);
    appendString(bytes, semantics.planningVersion.toStdString());
    appendString(bytes, semantics.policyVersion.toStdString());
    appendString(bytes, semantics.resolvedStrategy.toStdString());
    appendString(bytes, semantics.strategyVersion.toStdString());
    appendString(bytes, canonicalPolygon(task.region.coverageBoundary));
    appendString(bytes, canonicalPolygon(task.region.navigationBoundary));
    std::vector<std::string> obstacles;
    for (const auto& polygon : task.region.noGoRegions) {
        obstacles.push_back(canonicalPolygon(polygon));
    }
    std::ranges::sort(obstacles);
    appendInteger(bytes, obstacles.size());
    for (const auto& polygon : obstacles) {
        appendString(bytes, polygon);
    }
    appendNumber(bytes, task.coverage.swathWidthM);
    appendNumber(bytes, task.safety.hardSafetyMarginM);
    appendNumber(bytes, task.safety.preferredSafetyMarginM);
    appendNumber(bytes, task.planner.executionSafety.executionMarginM);
    appendString(bytes, task.coverage.coverageRequirement == CoverageRequirement::Standard ? "Standard" : "Strict");
    const bool manual = task.coverage.sweepAngleMode == SweepAngleMode::Manual;
    appendString(bytes, manual ? "Manual" : "Auto");
    if (manual) {
        double angle = std::fmod(task.coverage.sweepAngleDeg, 180.0);
        appendNumber(bytes, angle < 0.0 ? angle + 180.0 : angle);
    }
    appendString(bytes, task.planner.plannerId);
    PlanningInputIdentity identity;
    identity.semantics = semantics;
    identity.fingerprint =
        QString::fromLatin1(QCryptographicHash::hash(QByteArrayView(bytes.data(), static_cast<qsizetype>(bytes.size())),
                                                     QCryptographicHash::Sha256)
                                .toHex());
    return identity;
}

bool PlanningInputIdentity::matches(const MarineTask& task, const PlanningSemantics& supported) const
{
    if (encodingVersion != 1 || semantics != supported) {
        return false;
    }
    const auto current = fromTask(task, supported);
    return current.has_value() && *this == *current;
}

QJsonObject PlanningInputIdentity::toJson() const
{
    return {{"encodingVersion", encodingVersion},           {"planningVersion", semantics.planningVersion},
            {"policyVersion", semantics.policyVersion},     {"resolvedStrategy", semantics.resolvedStrategy},
            {"strategyVersion", semantics.strategyVersion}, {"fingerprint", fingerprint}};
}

bool PlanningInputIdentity::fromJson(const QJsonObject& json, PlanningInputIdentity& identity, QString& error)
{
    const QList<JsonParsing::KeyValidateInfo> keys = {
        {"encodingVersion", QJsonValue::Double, true}, {"planningVersion", QJsonValue::String, true},
        {"policyVersion", QJsonValue::String, true},   {"resolvedStrategy", QJsonValue::String, true},
        {"strategyVersion", QJsonValue::String, true}, {"fingerprint", QJsonValue::String, true},
    };
    if (!JsonParsing::validateKeys(json, keys, error)) {
        return false;
    }
    const double version = json.value("encodingVersion").toDouble();
    PlanningInputIdentity loaded;
    loaded.semantics = {json.value("planningVersion").toString(), json.value("policyVersion").toString(),
                        json.value("resolvedStrategy").toString(), json.value("strategyVersion").toString()};
    loaded.fingerprint = json.value("fingerprint").toString();
    if (!std::isfinite(version) || version < 1 || version > std::numeric_limits<int>::max() ||
        std::floor(version) != version || loaded.semantics.planningVersion.isEmpty() ||
        loaded.semantics.policyVersion.isEmpty() || loaded.semantics.resolvedStrategy.isEmpty() ||
        loaded.semantics.strategyVersion.isEmpty() || loaded.fingerprint.size() != 64 ||
        !std::ranges::all_of(loaded.fingerprint, [](QChar ch) {
            return (ch >= QLatin1Char('0') && ch <= QLatin1Char('9')) ||
                   (ch >= QLatin1Char('a') && ch <= QLatin1Char('f'));
        })) {
        error = QStringLiteral("Invalid Marine planning input identity");
        return false;
    }
    loaded.encodingVersion = static_cast<int>(version);
    identity = std::move(loaded);
    error.clear();
    return true;
}

}  // namespace Marine
