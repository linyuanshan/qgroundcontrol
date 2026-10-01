#pragma once

#include <string_view>

namespace Marine::CoverageStrategySemantics {

inline constexpr char AutoPlannerId[] = "marine.coverage.auto";
inline constexpr char AutoPlannerVersion[] = "auto.v1";
inline constexpr char SimpleMonotoneId[] = "marine.coverage.simple-monotone";
inline constexpr char SimpleMonotoneVersion[] = "simple-monotone.v1";
inline constexpr char BoustrophedonId[] = "marine.coverage.bcd";
inline constexpr char LawnmowerId[] = "marine.coverage.lawnmower";
inline constexpr char MockPlannerId[] = "marine.coverage.mock";
inline constexpr char BoustrophedonPendingVersion[] = "bcd.v0.5.pending-v05-06";
inline constexpr char LegacyBoustrophedonVersion[] = "bcd.pre-v05-06.v1";
inline constexpr char LawnMowerVersion[] = "lawnmower.p1.frozen.v1";
inline constexpr char MockVersion[] = "mock.test.v1";
inline constexpr char GlobalSweepVersion[] = "global-sweep.v1";
inline constexpr char PlanningSemanticsVersion[] = "p2.v0.5.planning.1";

[[nodiscard]] constexpr bool isSupportedStrategySemantic(std::string_view strategyId, std::string_view semanticVersion)
{
    return (strategyId == SimpleMonotoneId && semanticVersion == SimpleMonotoneVersion) ||
           (strategyId == BoustrophedonId &&
            (semanticVersion == LegacyBoustrophedonVersion || semanticVersion == BoustrophedonPendingVersion)) ||
           (strategyId == LawnmowerId && semanticVersion == LawnMowerVersion) ||
           (strategyId == MockPlannerId && semanticVersion == MockVersion);
}

}  // namespace Marine::CoverageStrategySemantics
