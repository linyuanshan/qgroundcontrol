# V05-05 Implementation Report

## Scope and state

V05-05 Auto planner and v0.5 SimpleMonotone implementation work is present in the working tree. The authorized boundary was preserved: V05-06 and later remain unauthorized. No commit was created, the current `HEAD` is `db223b34865cedfac330e07c724ce942d650f6d5`, and the index is empty.

The implementation adds SimpleMonotone capability assessment and target-relative lane generation, deterministic static-safe transit routing, safety-tier candidate generation, CoverageQuality-based candidate ordering, and Auto planner registration. Manual sweep keeps its validated requested angle. Auto sweep selects one deterministic global angle using the hard execution region; no alternate-angle retry is performed.

When topology/capability requires BCD, Auto records `marine.coverage.bcd` with semantic version `bcd.v0.5.pending-v05-06` and returns `ResolvedStrategyUnavailable` without invoking the legacy `BoustrophedonCoveragePlanner`. Multiple target components, target holes, and non-monotone target geometry are the only escalation reasons. Coverage incompleteness, assessment error, unsafe transit, and safety-tier infeasibility do not trigger escalation.

SimpleMonotone scan legs are clipped to CoverageTarget. Navigation/safety regions constrain feasible centerlines and transit only; N−C is not treated as required coverage. Successful policy-pass results retain the canonical path. Under this package's transitional semantics, reliable Insufficient and AssessmentError results fail without publishing a canonical path. No readiness, repair, artifact provenance persistence, QML, H/P/E redesign, or v0.5 BCD adaptation was added.

## Changed files

Modified tracked files:

- `custom/CMakeLists.txt`
- `custom/src/CustomPlugin.cc`
- `custom/src/Marine/Geometry/MarineGeometry.cc`
- `custom/src/Marine/Geometry/MarineGeometry.h`
- `custom/src/Marine/MarineTask.h`
- `custom/src/Marine/Planning/BoustrophedonCoveragePlanner.cc`
- `custom/src/Marine/Planning/BoustrophedonCoveragePlanner.h`
- `custom/src/Marine/Planning/CoveragePlanningProblem.h`
- `custom/src/Marine/Planning/CoverageProblemValidator.cc`
- `custom/src/Marine/Planning/CoverageQualityEvaluator.cc`
- `custom/src/Marine/Planning/CoverageQualityEvaluator.h`
- `custom/src/Marine/Planning/CoverageTaskAdapter.cc`
- `custom/src/Marine/Planning/ICoveragePlanner.h`
- `custom/src/Marine/Planning/LawnmowerCoveragePlanner.cc`
- `custom/src/Marine/Planning/LawnmowerCoveragePlanner.h`
- `custom/src/Marine/Planning/MockCoveragePlanner.cc`
- `custom/src/Marine/Planning/MockCoveragePlanner.h`
- `custom/src/Marine/Planning/PlanningPathMetrics.cc`
- `custom/src/Marine/Planning/PlanningPathMetrics.h`
- `custom/src/Marine/Planning/PlanningResult.h`
- `custom/src/Marine/PlanningInputIdentity.cc`
- `custom/src/Marine/PlanningInputIdentity.h`
- `custom/src/MissionManager/CoverageInspectionComplexItem.cc`
- `custom/test/Marine/CoverageComplexItemTest.cc`
- `custom/test/Marine/CoverageComplexItemTest.h`
- `custom/test/Marine/CoveragePlannerTest.cc`
- `custom/test/Marine/CoverageQualityEvaluatorTest.cc`
- `custom/test/Marine/CoverageQualityEvaluatorTest.h`
- `custom/test/Marine/CustomPluginIntegrationTest.cc`
- `custom/test/Marine/MarineGeometryTest.cc`
- `custom/test/Marine/MarineGeometryTest.h`
- `custom/test/Marine/PlanningInputIdentityTest.cc`
- `custom/test/Marine/PlanningInputIdentityTest.h`

New files:

- `custom/src/Marine/Planning/AutoCoveragePlanner.cc`
- `custom/src/Marine/Planning/AutoCoveragePlanner.h`
- `custom/src/Marine/Planning/CoverageStrategySemantics.h`
- `custom/src/Marine/Planning/PlannerSource.h`
- `custom/src/Marine/Planning/SimpleMonotoneCapability.cc`
- `custom/src/Marine/Planning/SimpleMonotoneCapability.h`
- `custom/src/Marine/Planning/SimpleMonotoneCoveragePlanner.cc`
- `custom/src/Marine/Planning/SimpleMonotoneCoveragePlanner.h`
- `custom/test/Marine/AutoCoveragePlannerTest.cc`
- `custom/test/Marine/AutoCoveragePlannerTest.h`
- `custom/test/Marine/SimpleMonotoneCapabilityTest.cc`
- `custom/test/Marine/SimpleMonotoneCapabilityTest.h`
- `custom/test/Marine/SimpleMonotoneCoveragePlannerTest.cc`
- `custom/test/Marine/SimpleMonotoneCoveragePlannerTest.h`

This report and `build/v05-05-evidence.json` are the V05-05 evidence artifacts. No governance, frozen specification, QML, or V05-06+ implementation files were changed.

## Validation

- Windows incremental build: **PASS**. `build/v05-05-run-build.cmd` invokes `cmake --build build/P0-01-marine-debug --target QGroundControl --parallel 8`; the final link completed. The executable SHA-256 and build-log SHA-256 are recorded in the machine-readable evidence file.
- Focused V05-05 suites: **14/14 PASS** after the final format pass: `AutoCoveragePlannerTest`, `SimpleMonotoneCoveragePlannerTest`, `SimpleMonotoneCapabilityTest`, `MonotoneCoverageTest`, `CoverageSafetyTest`, `CoverageQualityEvaluatorTest`, `StaticSafeRouterTest`, `CoverageProblemValidatorTest`, `CoverageTaskAdapterTest`, `CoveragePlannerTest`, `PlanningInputIdentityTest`, `CoverageComplexItemTest`, `CustomPluginIntegrationTest`, and `MarinePlanIntegrationTest`.
- Regression test localization corrections: `MissionManagerTest::init` and `PlanMasterControllerTest::_testActiveVehicleChanged` now filter non-empty `AppMessage` text without matching an English phrase. `PlanMasterControllerTest::_testFailedLoadClearsFileAssociation` and the three malformed-KML cases in `QGCMapPolygonTest::_testKMLLoad` likewise require a non-empty message while retaining assertions on operation failure and file-association state. Production messages and translations were not changed.
- The six previously failing suites were rerun with verbose CTest output, `QGC_TEST_VERBOSE=1`, per-suite JUnit, and the Windows font directory supplied to Qt. **5/6 PASS**: `MissionCommandTreeEditorTest`, `MissionControllerTest`, `MissionControllerTreeTest`, `PlanMasterControllerTest`, and `QGCMapPolygonTest`. The cache-write failures in the two controller suites disappeared when their per-suite temporary AppData writes were permitted; no test or production code change was made for those failures.
- `MissionManagerTest` remains **FAIL** only in `_testErrorAckFailureStrings`. HEAD `db223b34865cedfac330e07c724ce942d650f6d5` contains the zh-CN catalog entry at `translations/qgc_source_zh_CN.ts:16195-16196`: source `Frame: %1`, translation `框架1`; the translation omits required placeholder `%1`. HEAD `src/MissionManager/PlanManager.cc:706` formats this translated string with `.arg(item->frame())`, which causes Qt's `QString::arg: Argument missing` warning under this test. The catalog blob is `dc57002d2f09b2ecfec4e2bddbcdd9170f5eb052` both at V05-05 baseline `e5b3edd3a` and at HEAD. A baseline-to-current-worktree diff of the catalog and relevant production files (`PlanManager.cc`, `PlanManager.h`, and `Vehicle.cc`) is empty; their baseline and HEAD blob IDs also match. This supports classifying the failure as pre-existing and unrelated to V05-05. The only working-tree edit in `MissionManagerTest.cc` changes the fixture's localized AppMessage log filter; `_testErrorAckFailureStrings` itself and the relevant production path are unchanged. No translation or production source was edited for this evidence update.
- `MissionCommandTreeEditorTest` cause is verified as an environment issue. A pre-V05-05 Desktop build from 2026-09-26 (binary SHA-256 recorded in `build/v05-05-evidence.json`) reproduced the strict-log failure: `QFontDatabase: Cannot find font directory C:/Qt/6.11.1/msvc2022_64/lib/fonts`. The current binary passes all 3 cases when `QT_QPA_FONTDIR=C:\Windows\Fonts` is set. Its test and relevant application/CLI sources are identical to the V05-05 baseline commit `e5b3edd3a`.
- Full relevant Marine/QGC regression set: **49/50 PASS; 1 FAIL**. The sole failure is the same zh-CN translation placeholder warning in `MissionManagerTest`; the full run includes all 50 suites and its test list, CTest log, and captured JUnit files are recorded in the machine-readable evidence.
- After formatter-only line reflow in the three edited test files, the Windows incremental build passed again. On the final rebuilt binary, the three edited suites were rerun: **2/3 PASS** (`PlanMasterControllerTest`, `QGCMapPolygonTest`); `MissionManagerTest` remains failed on the same unchanged translation warning. The full 50-suite run used the immediately preceding binary, before whitespace-only reflow; its behavior-affecting test changes are identical. Scoped `clang-format --dry-run --Werror` checks passed for all edited test lines.
- `clang-tidy`: **PASS**, 14 changed production translation units.
- `clang-format`: **PASS**, 46 changed C++ files.
- `vehicle-null-check`: **PASS**.
- `qt-translate-noop-check`: **PASS**.
- `git diff --check`: **PASS** after the regression test edits.
- Normal pre-commit attempt: **BLOCKED by local cache permissions**. It failed before running hooks with `sqlite3.OperationalError: attempt to write a readonly database` at `C:\Users\Lin\.cache\pre-commit\pre-commit.log` and a subsequent `PermissionError` writing that log. No bypass was used.

## Review boundary

No architecture deviation was needed. No V05-06 BCD implementation, Coverage Repair, MissionReadiness, full Artifact v3 provenance persistence, upload gate, QML, or calibration work was introduced. The V05-05 changes remain unstaged and uncommitted at `db223b34865cedfac330e07c724ce942d650f6d5`; regression acceptance remains **not fully passed** because of the pre-existing zh-CN translation placeholder defect. Stop for independent review; do not proceed to V05-06 without separate authorization.
