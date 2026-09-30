# V05-04 Implementation Report

## Approved policy

- CAL-01 `StandardCoveragePolicy.minimumCoverageRatio`: **0.99** (owner-approved).
- CAL-02 `StandardCoveragePolicy.boundaryToleranceM`: **0.50 m** (owner-approved).
- Policy semantic identity: `coverage-quality.v1`.

The only production definitions of the two CAL values are in `CoverageQualityPolicy.h`.

## Implementation

`CoverageQualityEvaluator` is a pure Marine-domain evaluator. It validates the target, path,
leg roles, swath width, requirement, and semantic version using the existing planning-path
metrics validator. Coverage comes only from round-cap swath buffers of Coverage legs; Transit
legs contribute no footprint. It clips the unioned footprint to the target, computes the
uncovered geometry and areas, and reports numerical or geometry failures as
`AssessmentError` rather than policy failure.

Standard and Strict share one policy-independent coverage and residual evaluation. For the
same target, path, leg roles, swath, and semantic identity, both produce identical target and
covered/uncovered metrics, critical core, residual geometries, critical uncovered area, and
degenerate-core/fallback metadata. Residual output includes the critical coverage core,
uncovered region, critical uncovered region, boundary shortfall, and target components
requiring Strict fallback. Only after that shared evaluation is complete does the selected
requirement determine pass/status: Strict accepts only numerical completeness, while Standard
also checks coverage ratio, critical-core deficit, per-component fallback, and whole-target
fallback. Area identities are checked against
`max(0.01 m², 1e-6 × target area)` and only tolerance-sized deviations are clamped.

The independent-review single-point correction removed the Strict early return. Standard and
Strict now complete identical metrics, critical-core, residual geometry, and fallback metadata
work before requirement-specific pass/status evaluation. Regression fixtures assert shared
truth for complete coverage, the M06 boundary-only shortfall, and the internal critical gap.

The quality comparator is limited to reliable quality evaluations. It orders the approved
quantized critical-uncovered and uncovered quality keys and returns NotComparable for
AssessmentError; it does not select planning candidates.

Production BCD now evaluates its assembled hard-safe result with this evaluator. Complete and
Acceptable retain the canonical path. Insufficient and AssessmentError use the transitional
failure result model, clear the returned canonical path, and preserve the evaluation. The
historical `NominalCoverageValidator` remains available unchanged as P1/P2 strict-like
regression infrastructure. The Task adapter copies the selected coverage requirement without
persisting quality into Artifact v3.

The new polygon-region intersection and inset operations support region sets, holes, and
multiple components. Inset is coverage-policy geometry and does not call or change safety
offset logic or `CoordinateScalePerM`.

## Changed files

- `custom/CMakeLists.txt`
- `custom/src/Marine/Geometry/PolygonRegion.cc`
- `custom/src/Marine/Geometry/PolygonRegion.h`
- `custom/src/Marine/Planning/BoustrophedonCoveragePlanner.cc`
- `custom/src/Marine/Planning/CoveragePlanningProblem.h`
- `custom/src/Marine/Planning/CoverageProblemValidator.cc`
- `custom/src/Marine/Planning/CoverageQualityEvaluator.cc`
- `custom/src/Marine/Planning/CoverageQualityEvaluator.h`
- `custom/src/Marine/Planning/CoverageQualityPolicy.h`
- `custom/src/Marine/Planning/CoverageTaskAdapter.cc`
- `custom/src/Marine/Planning/NominalCoverageValidator.h`
- `custom/src/Marine/PlanningInputIdentity.h`
- `custom/test/Marine/BoustrophedonCoveragePlannerTest.cc`
- `custom/test/Marine/CoverageProblemValidatorTest.cc`
- `custom/test/Marine/CoverageQualityEvaluatorTest.cc`
- `custom/test/Marine/CoverageQualityEvaluatorTest.h`
- `custom/test/Marine/CoverageTaskAdapterTest.cc`
- `custom/test/Marine/CoverageTaskAdapterTest.h`
- `custom/test/Marine/MarineGeometryTest.cc`
- `custom/test/Marine/MarineGeometryTest.h`
- `custom/test/Marine/PlanningInputIdentityTest.cc`

`AGENTS.md`, the frozen design text, Artifact v3, and `CoordinateScalePerM` were not changed.

## Validation evidence

- Windows incremental build after the independent-review correction: PASS,
  `build/v05-04-r4-build.log`.
- Required focused suites after the correction: PASS, 10 suites / 175 tests, zero failures,
  `build/v05-04-r4-focused.log`.
- Established Marine and QGC Mission regression selection after the correction: PASS,
  50 suites / 653 tests,
  zero failures; see
  `build/v05-04-r4-regressions.log` and per-suite XML/JSON evidence.
- clang-tidy: PASS on all five changed production `.cc` files; latest logs are
  `build/v05-04-r4-clang-tidy-*.log`.
- clang-format changed-line check, new-file dry-run, `vehicle_null_check.py`,
  `qt_translate_noop_check.py`, and `git diff --check`: PASS after the correction.
- Normal pre-commit: attempted once during V05-04 implementation and blocked by the
  read-only pre-commit SQLite database
  (`OperationalError: attempt to write a readonly database`; logging also failed with a
  permission error). It was not bypassed. Details are in `build/v05-04-evidence.json`.

## Scope and remaining boundary

This package adds no Auto planner, SimpleMonotone strategy, target/navigation-aware BCD rewrite,
Coverage Repair, readiness or diagnostic model, Artifact v3 quality persistence, MissionAdapter
upload gate, QML, calibration changes, geometry numeric policy redesign, or safety algorithm
changes. BCD integration is transitional: V05-08 still owns readiness, diagnostic issues,
artifact quality persistence, and upload gating. No V05-05 or later implementation is included.

No commit was created and no files were staged. HEAD remains
`6a1914bc3ba29c4482d53f2c1e344e9035c60e84` on `feature/marine-p2-complex-coverage`.
