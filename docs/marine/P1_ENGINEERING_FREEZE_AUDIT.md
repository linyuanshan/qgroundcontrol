# P1 Engineering Freeze Audit

Audit date: 2026-09-12

Status: **P1 ENGINEERING FREEZE APPROVED — REAL-USV FIELD VALIDATION DEFERRED**

Engineering Freeze closes the P1 software and engineering baseline. It is not field acceptance and must not be
described as real-USV validation. The software candidate validated by the final SITL and regression runs is
`90a54665028108e0ed5a7c436d7a4c3b284dd2a0`; the final freeze documentation commit is a documentation-only
descendant of that candidate.

## Stage Decision

```text
P1 ENGINEERING FREEZE: APPROVED
P1-13 ARDUROVER SITL: PASS
P1-14 REAL USV FIELD VALIDATION: DEFERRED
P1 FIELD VALIDATION CLOSURE: PENDING
```

The project currently lacks suitable conditions for real-USV field validation. To prevent field-resource
constraints from blocking software development, the project lead approved moving P1 real-USV validation from a
P2 software-development prerequisite to a deferred field-validation gate. The validation is not deleted,
substituted, or weakened and must be completed before P2 real-USV field acceptance.

## Audited Baseline

- Branch: `feature/marine-p1-coverage`
- Validated software commit: `90a54665028108e0ed5a7c436d7a4c3b284dd2a0`
- Included upstream baseline: `837871f4a3319a6134c393e4a1a85a4062a96b40`
- Platform: Windows 11 Pro `10.0.26200`, MSVC `19.51.36256.0`, Qt `6.11.1`
- ArduRover SITL: `Rover-4.7.0`
- SITL Docker image: `sha256:001f20d07215138f2cb4aebc8eb691c8f4c3a23825a01f343b3f5397d262d3f0`

## Freeze Review

| Review item | Result | Evidence |
| --- | --- | --- |
| Task / Plan / Mission boundaries remain separate | Pass | Planning core returns `CoveragePlanningSolution`; `CoverageTaskAdapter` maps it to `PlanningResult`; only `ArduPilotMissionAdapter` creates `MissionItem` objects. |
| Planner remains pure geometry | Pass | Planner interfaces and data structures have no QML, `QObject`, Fact, geographic, mission-item, vehicle, or controller dependency. |
| P1 navigation-angle semantics | Pass | Public/UI/JSON angles use 0 degrees North, 90 degrees East, clockwise positive, with explicit conversion to internal ENU mathematical angles. |
| Safety and nominal coverage semantics | Pass | Centerlines remain in the safety inset; fixed-swath coverage reaches the nominal WorkRegion target boundary; impossible combinations return `CoverageImpossibleWithSafetyMargin`. |
| Unsupported geometry and No-Go behavior | Pass | Unsupported geometry and non-empty No-Go input fail explicitly; No-Go is never silently ignored. |
| Save/load without replanning | Pass | Tests and retained final SITL plans restore task, generated path, selected angle, metrics, sensor configuration, and mission conversion state. |
| ArduRover SITL execution | Pass | S01-S05 final-candidate runs accepted mission items, entered AUTO, reached every waypoint in order, and emitted `Mission Complete`. See `P1_ARDUROVER_SITL_TEST_PROTOCOL.md`. |
| Real-USV execution | Deferred | The unchanged field protocol remains mandatory; all field evidence is Pending. |
| Third-party governance | Pass | Clipper2 remains pinned and attributed in `custom/src/Marine/THIRD_PARTY.md`, is linked privately by Marine geometry, and does not modify upstream `.github/COPYING.md` policy. No legal compatibility conclusion is made. |
| P2+ scope exclusion | Pass | No routing around No-Go, turn-radius planner, runtime sensor protocol, ROS integration, fleet framework, or other P2+ production feature entered P1. |

## P1-13R Final SITL Revalidation

All five scenarios passed at the validated software commit. `WP_RADIUS` was 3 m in every case.

| Scenario | Result | Ordered progression | Final state |
| --- | --- | --- | --- |
| S01 rectangle, manual 0 degrees, safety 0 m | Pass | WP 1 through 8 | `Mission Complete`; remained in AUTO |
| S02 rectangle, manual 90 degrees, safety 8 m | Pass | WP 1 through 12 | `Mission Complete`; no RTL during mission |
| S03 convex polygon, manual 30 degrees, safety 8 m | Pass | WP 1 through 14 | `Mission Complete`; no RTL during mission |
| S04 rectangle, auto, safety 8 m | Pass | WP 1 through 8 | `Mission Complete`; no RTL during mission |
| S05 convex polygon, auto, safety 8 m | Pass | WP 1 through 12 | `Mission Complete`; no RTL during mission |

Telemetry logs contain successful `MISSION_ACK` values, ordered `MISSION_CURRENT` / `MISSION_ITEM_REACHED`
progression, and `Mission Complete`. RTL states in S02-S05 occur before a new run or after completion and are not
automatic mission-final actions. Full artifact hashes are recorded in the SITL protocol.

## Builds and Automated Tests

### Build results

- Windows Debug custom build with tests: Pass; incremental verification reported no work required.
- Production Release custom build with `QGC_BUILD_TESTING=OFF`: Pass at the validated software commit.
- A first Release invocation without the Visual Studio developer environment could not locate MSVC standard
  headers or Windows SDK libraries. Loading the installed Visual Studio developer environment and rerunning the
  same configured build passed; this was an invocation-environment issue, not a source failure.

Non-blocking configure warnings remained unchanged: no compiler cache, cached ArduPilot parameter origin not
reachable, and optional Gettext/LibUSB packages not found.

### Marine and QGC regression

- All 15 Marine test classes: Pass, zero failures.
- `MissionItemTest`: Pass.
- `SimpleMissionItemTest`: Pass.
- `MissionControllerTest`: Pass in a writable Windows profile.
- `MissionControllerTreeTest`: Pass in a writable Windows profile.
- `SurveyComplexItemTest`: Pass.
- `StructureScanComplexItemTest`: Pass.
- `PlanMasterControllerTest`: 71 tests, 2 known upstream strict-log expectation failures and 69 passes.

The two unchanged `PlanMasterControllerTest` baseline failures are:

- `_testActiveVehicleChanged`: localized application messages do not match the ignored English message.
- `_testFailedLoadClearsFileAssociation`: the expected `Error loading Plan file` application message is not
  captured.

The controller tests initially produced strict-log failures under the repository sandbox because the generated
QGC profile could not write Windows user cache files. Rerunning in a writable Windows test profile removed those
permission messages and restored the expected baseline. Marine code was not changed to mask them.

## Frozen P1 Engineering Baseline

The following are frozen for compatibility during P2:

- Task / Plan / Mission separation;
- `MarineTask`, `WorkRegion`, and `CoverageConfig` persistence;
- `CoveragePlanningProblem`, `ICoveragePlanner`, and `CoveragePlanningSolution` boundaries;
- navigation sweep-angle semantics;
- centerline safety-margin and nominal Coverage Target semantics;
- P1 `LawnmowerCoveragePlanner` behavior;
- `CoverageTaskAdapter` conversion to `PlanningResult`;
- `ArduPilotMissionAdapter` waypoint-only mission semantics;
- save/load restoration without replanning.

Any P2 shared-layer evolution must preserve P1 compatibility and keep the P1 regression suite passing.

## Field Debt

P1-14 has not been executed. The retained `P1_REAL_USV_FIELD_VALIDATION_PROTOCOL.md` continues to define the
unmodified acceptance standard. Its evidence record remains entirely Pending, and closure is required before P2
real-USV field acceptance.
