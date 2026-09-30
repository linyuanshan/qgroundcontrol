#pragma once

namespace Marine {

inline constexpr char CoverageQualityPolicySemanticVersion[] = "coverage-quality.v1";

struct StandardCoveragePolicy
{
    static constexpr double minimumCoverageRatio = 0.99;
    static constexpr double boundaryToleranceM = 0.50;
};

}  // namespace Marine
