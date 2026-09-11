# P1 Conditional Freeze Audit

Audit date: 2026-09-09

Status: **CONDITIONAL — P1-13 REVALIDATION AND P1-14 PENDING**

This is the P1-15A conditional freeze audit. It is not P1-15 approval and does not authorize the P1 branch to be
merged into `marine/main`. The final closure candidate changes sweep-angle and safety-margin planning semantics;
therefore P1-13 must be rerun, and P1-14 real-USV field validation remains open.

## Audited Baseline

- Branch: `feature/marine-p1-coverage`
- Commit: `5bb1cf4f3c02ebde5a5459a268ae779ca83da9a5`
- Upstream Marine baseline: `marine/main`
- Platform: Windows 11 Pro `10.0.26200`, MSVC `19.51.36256.0`, Qt `6.11.1`
- ArduRover SITL: `Rover-4.7.0`

The audit included the pending P1-13 evidence record, the P1-14 acceptance protocol, and the test-only fixture fix
listed below. No Marine production code was changed by P1-15A.

## Freeze Review

| P1-15 review item | Result | Evidence |
| --- | --- | --- |
| Task / Plan / Mission boundaries remain separate | Pass | Planning core returns `CoveragePlanningSolution`; `CoverageTaskAdapter` maps it to `PlanningResult`; only `ArduPilotMissionAdapter` creates `MissionItem` objects. |
| Planner remains pure geometry | Pass | `ICoveragePlanner`, `LawnmowerCoveragePlanner`, and their input/output types have no QML, `QObject`, `Fact`, `QGeoCoordinate`, `MissionItem`, `Vehicle`, or controller dependency. |
| No Marine-specific dependency was added to QGC Core | Pass | P1 changes are confined to the custom overlay, Marine documentation, and development tools. Clipper2 is linked privately by `MarineGeometry`. |
| Third-party governance remains fork-scoped | Pass | Clipper2 stays pinned and attributed in `custom/src/Marine/THIRD_PARTY.md`; upstream `.github/COPYING.md` is unchanged, and no legal compatibility determination is made. |
| Convex regions plan reliably | Pass | Fixed and property-oriented Marine planner tests pass for rectangles, rotated and irregular convex regions, and manual/auto angles. |
| Safety inset is enforced | Pass | Empty/disconnected inset handling, positive margin, path containment, and coverage-impossibility cases pass. |
| Auto angle is deterministic | Pass | Repeated-input angle/path determinism tests pass. |
| Unsupported geometry fails explicitly | Pass | Validation and planner tests cover invalid and unsupported geometry without silent fallback. |
| No-Go is not silently ignored | Pass | `CoverageTaskAdapter` and `CoverageProblemValidator` reject non-empty No-Go input; the UI remains read-only for No-Go. |
| Save/load restores without replanning | Pass | Complex-item and vertical-slice integration tests restore task, path, selected angle, metrics, sensors, and mission items from persistence. |
| ArduRover SITL executes the mission | **Pending revalidation** | S01-S05 evidence at `5bb1cf4f3` is retained, but the final closure candidate changes generated paths and requires a fresh run. |
| A real USV completes at least one mission | **Pending** | The field protocol is frozen, but no real-USV execution evidence exists yet. This is P1-14. |
| No P2+ capability entered P1 | Pass | No No-Go routing/editor workflow, turn-radius planner, sensor runtime protocol, ROS integration, fleet framework, or other prohibited P2+ feature was found. |

## Builds and Automated Tests

### Build results

- Full Debug custom build with tests: Pass.
- Fresh production Release custom build with `QGC_BUILD_TESTING=OFF`: Pass.
- Fresh Release build with `QGC_BUILD_TESTING=ON`: known upstream test-integration failure. The main target includes
  `test/UnitTestFramework/BaseClasses/VehicleTest.h` without a path that resolves `MockLink.h`. This is unrelated
  to Marine production code and does not affect the production Release build.

Non-blocking configure warnings remained unchanged: no compiler cache, cached ArduPilot parameter origin not
reachable, and optional Gettext/LibUSB packages not found.

### Marine and tooling tests

- 15 Marine test classes: Pass, zero failures.
- `CustomPluginIntegrationTest`: Pass in five consecutive focused runs after the fixture lifetime correction.
- ArduRover and Windows clang-tidy Python tests: 6 passed.
- Direct clang-format and clang-tidy checks on the modified C++ test: Pass. Clang-tidy reported only three
  pre-existing member-function-to-static suggestions and returned success.
- Branch diff whitespace validation: Pass.

### QGC regression baseline

| Test class | Tests | Failures |
| --- | ---: | ---: |
| `PlanMasterControllerTest` | 71 | 2 |
| `MissionControllerTest` | 20 | 0 |
| `MissionControllerTreeTest` | 11 | 0 |
| `MissionItemTest` | 13 | 0 |
| `SimpleMissionItemTest` | 12 | 0 |
| `SurveyComplexItemTest` | 9 | 0 |
| `StructureScanComplexItemTest` | 5 | 0 |

The two remaining `PlanMasterControllerTest` failures are upstream strict-log expectation mismatches:

- `_testActiveVehicleChanged`: two localized application messages do not match the ignored English message.
- `_testFailedLoadClearsFileAssociation`: the expected `Error loading Plan file` application message is not
  captured.

They reproduce in a writable, test-specific Windows profile. The other 134 assertions in the selected regression
classes pass. Per the frozen P1 specification, these failures are recorded and are not fixed by changing Marine or
native QGC expected behavior.

## Defect Found and Corrected During P1-15A

`CustomPluginIntegrationTest::_testMarinePlanPreloadValidation` created a parented complex item that survived until
fixture teardown. Repeated execution exposed an intermittent access violation/heap-corruption report. The test now
owns that temporary item with `std::unique_ptr`, so it is destroyed within the test before controller teardown.

This correction changes test ownership only. Planner behavior, C++ planning results, persistence, QML, and mission
generation are unchanged.

## Final Closure Blocker Corrections

- Public, UI, JSON, and selected sweep angles use navigation bearings: 0 degrees North, 90 degrees East, clockwise
  positive. The planner explicitly converts those bearings to and from ENU mathematical angles.
- Lane placement now uses the nominal WorkRegion cross-track extent while keeping every centerline in the safety
  inset. A candidate that cannot reach both nominal boundaries returns
  `CoverageImpossibleWithSafetyMargin` instead of Success.
- The current Debug and production Release custom builds pass. All 15 Marine test classes pass. MissionItem,
  Survey, StructureScan, and MissionController regressions pass; PlanMasterController retains the same two
  upstream strict-log baseline failures.
- Fetching the latest `upstream/master` was attempted but the GitHub connection was reset. Controlled merge and
  post-merge regression remain pending until the remote can be fetched.

## Conditional Freeze Decision

P1-01 through P1-12 satisfy their audited software gates. P1-13 requires final-candidate revalidation and P1-14
remains open because real-USV execution cannot be replaced by SITL, desktop testing, or an assumption. Therefore:

```text
P1-15A CONDITIONAL FREEZE: PENDING REVALIDATION
P1-13 ARDUROVER SITL: PENDING REVALIDATION
P1-14 REAL USV FIELD VALIDATION: PENDING
P1-15 FORMAL FREEZE: NOT APPROVED
```

Until P1-14 evidence is complete:

- do not claim `P1 FREEZE APPROVED`;
- do not merge the P1 feature branch into `marine/main` as a frozen release;
- do not begin P2 feature work;
- software defect fixes may continue only when they preserve the frozen P1 scope and are covered by focused tests.

After P1-14 completes, rerun the affected smoke/regression checks, update this audit with the field-evidence
references and hashes, and then perform the formal P1-15 review.
