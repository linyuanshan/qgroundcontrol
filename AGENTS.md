# AGENTS.md

Instructions for AI coding agents working on this QGroundControl-derived Marine Robotics project.

## 1. Scope and Precedence

This repository is based on QGroundControl and adds a Marine Robotics layer for USV/ROV mission planning and intelligent operations.

When instructions conflict, use this precedence:

1. Explicit user instructions for the current task.
2. This `AGENTS.md`.
3. Marine design specifications under `docs/marine/`.
4. Upstream QGroundControl contribution, coding-style, testing, and CI guidance.

Marine-specific rules are additive to upstream QGroundControl rules. Do not weaken upstream safety, coding-style, testing, or CI requirements.

## 2. Required Preflight

Before modifying repository files:

1. Read `CODING_STYLE.md`.
2. Read `.github/CONTRIBUTING.md`.
3. Read `test/README.md` when changing or adding tests.
4. Read `.github/ci-overview.md` when changing build, CI, or test integration.
5. For Marine work, read the current Marine phase specification, especially:
   - `docs/marine/P1_COVERAGE_INSPECTION_SPEC_v0.2.md` during P1;
   - `docs/marine/P0_FOUNDATION_SPEC_v0.2.md` for frozen P0 architecture and persistence invariants.
6. Inspect the current repository implementation before assuming an API from documentation or previous discussion.
7. Before the first edit, report which instruction/design files were read and any important API differences discovered.

## 3. Upstream QGroundControl Rules

Follow upstream QGroundControl architecture and conventions.

In particular:

- Vehicle parameters must use the QGC Fact system.
- Always guard nullable `Vehicle*` / active-vehicle access.
- Firmware-specific behavior belongs behind firmware-plugin abstractions where appropriate.
- Expose QML-facing state through normal Qt/QML mechanisms used by the surrounding code.
- Do not introduce production `Q_ASSERT` usage as a substitute for defensive runtime handling.
- Do not use fixed test delays when signal/condition-based waiting is available.
- Prefer clear code over explanatory comments; comment only non-obvious intent, constraints, or workarounds.
- Match the style of the code being edited.

Use existing QGC implementations as architectural references before inventing new integration patterns.

## 4. Build and Test Workflow

Use the repository's current canonical build/test workflow. Inspect `tools/README.md`, CI configuration, and the active local build environment rather than guessing commands.

General workflow:

```text
small implementation step
        ↓
incremental build
        ↓
targeted test
        ↓
fix
        ↓
commit
        ↓
next step
```

For multi-file Qt/C++ changes, build incrementally instead of waiting until the end.

Before declaring a work package complete:

- Build succeeds.
- Relevant focused tests pass.
- Required lint/pre-commit checks pass.
- Related existing QGC tests still pass.
- No unrelated files were changed.

## 5. Git Development Model

Do not develop Marine features directly on the branch used to track upstream QGroundControl.

Recommended branch model:

```text
upstream/master
      ↓
master                  # tracks upstream
      ↓
marine/main             # stable Marine integration branch
      ↓
feature/marine-*        # phase/work-package development
```

For P1, use the dedicated branch:

```text
feature/marine-p1-coverage
```

Keep commits coherent and reviewable. Prefer Conventional Commit style, for example:

```text
feat(marine): add marine task model
feat(marine): add task JSON persistence
feat(marine): add coverage planner interface
feat(marine): add ArduPilot mission adapter
test(marine): add P0 integration test
```

Do not mix unrelated cleanup with a Marine work package.

---

## Marine Development

### 6. Product Direction

The long-term goal is a Marine Robotics intelligent-operation platform built on QGroundControl for USV and ROV applications.

The intended long-term system boundary is:

```text
QGroundControl
    ├── task definition
    ├── global mission planning
    ├── mission upload
    └── operation monitoring
          │
          │ MAVLink / Marine communication
          ▼
ArduPilot + Companion Computer
          │
          ▼
ROS 2
    ├── perception
    ├── local obstacle avoidance
    ├── local replanning
    ├── AI inference
    └── data processing
```

QGroundControl must remain ROS-independent. Do not directly link ROS 2 runtime libraries such as `rclcpp` into QGC unless a future approved design explicitly changes this boundary.

### 7. Current Phase: P1

The current implementation target is:

```text
P1 — USV Coverage Inspection V1
```

P0 is frozen. P1 adds a real, bounded coverage planner for general convex and supported sweep-monotone simple regions without weakening the P0 vertical-slice boundaries.

Required P1 flow:

```text
MarineTask
    ↓
CoverageTaskAdapter
    ↓
Local ENU CoveragePlanningProblem
    ↓
Safety Inset
    ↓
Manual / Auto Sweep Direction
    ↓
Lawnmower Coverage
    ↓
PlanningResult
    ↓
ArduPilotMissionAdapter
    ↓
MAVLink Mission
    ↓
ArduRover SITL / real USV validation
```

Do not reopen or redesign P0 merely to implement P1.

### 8. P1 Design Authority

Before P1 changes, read:

```text
docs/marine/P1_COVERAGE_INSPECTION_SPEC_v0.2.md
```

That frozen document defines the P1 implementation baseline. The P0 specification remains authoritative for the already-frozen Task/Plan/Mission separation, persistence, adapter boundaries, and plan-scoped context.

If the current QGroundControl API differs from the specification:

- adapt to the current QGC API;
- preserve the architecture boundaries in this file;
- do not force obsolete class names or signatures;
- report material deviations before introducing a larger alternative design.

### 9. Core Architecture Rule: Task != Plan != Mission

Always preserve this separation:

```text
Task
    what the robot should accomplish

Plan
    how a planner proposes to accomplish the task

Mission
    the MAVLink commands ArduPilot will execute
```

Canonical flow:

```text
MarineTask
    ↓
Coverage Planner
    ↓
PlanningResult
    ↓
Mission Adapter
    ↓
MAVLink Mission
```

Do not collapse these layers.

### 10. Coverage Planner Independence

`ICoveragePlanner` and concrete coverage algorithms must not depend on:

```text
QML
QObject
Fact
QGeoCoordinate
MissionItem
Vehicle
PlanMasterController
```

Planner logic should operate on Marine-owned data structures and be independently unit-testable.

A planner returns a `PlanningResult`.

A planner must never directly create `MissionItem` objects.

### 11. CoverageInspectionComplexItem Is an Adapter

`CoverageInspectionComplexItem` is the QGC integration adapter for a Marine coverage task.

Its responsibilities are limited to:

- maintaining the `taskId` association;
- accessing the corresponding Marine task/context;
- invoking the selected planner;
- storing/exposing the resulting plan;
- exposing state/path data required by QML;
- save/load integration;
- passing an existing planning result to the mission adapter.

It must not contain coverage-planning algorithms.

Never implement these inside `CoverageInspectionComplexItem`:

```text
sweep-line generation
lawnmower planning
polygon clipping
polygon offset
BCD decomposition
path ordering/optimization
turn-radius planning
current-aware optimization
coverage-quality algorithms
```

### 12. Mission Adapter Boundary

The only approved conversion path is:

```text
PlanningResult
    ↓
ArduPilotMissionAdapter
    ↓
MissionItem[]
```

`appendMissionItems()` must use an already-generated planning result.

It must not silently rerun the planner.

P0 waypoint behavior remains frozen. During P1, extend `ArduPilotMissionAdapter` only as explicitly required by the P1 specification and ArduRover validation.

Do not add speed, camera, sonar, RTL, hold, or other execution commands unless the active phase specification requires them.

### 13. Marine Task Persistence

Marine task definitions are application-level data and are not equivalent to MAVLink missions.

Store Marine task definitions in the top-level QGC plan extension:

```json
"marine": {
  "version": 1,
  "tasks": []
}
```

Keep the QGC/MAVLink mission in the normal QGC `mission` object.

A Marine coverage complex item references its task using `taskId`.

Do not duplicate the complete Marine task definition inside the complex mission item.

Centralize task JSON conversion in `MarineTaskJsonCodec`; do not scatter serialization across planners, QML, and mission adapters.

### 14. Frozen P0 Data Model

Keep the frozen P0 model and JSON schema backward compatible.

The retained concepts include:

```text
MarineTask
WorkRegion
CoverageConfig
SensorConfig
PlannerConfig
PlanningResult
```

`WorkRegion` must support:

```text
outerBoundary
noGoRegions[]
```

and P1 builds on that structure.

Extend only fields and types required by the frozen P1 specification; centralize persistence in `MarineTaskJsonCodec`.

### 15. P1 Features Explicitly Out of Scope

Do not implement the following during P1:

```text
no-go routing
full No-Go editor unless it is near-direct reuse with no new workflow
BCD/cellular decomposition
turn-radius / kinematic planning
sensor recording protocol
MarineTaskBridge
ROS 2 integration
custom ROS transport
current-aware planning
energy-aware planning
coverage-quality maps
adaptive swath width
ROV 2.5D planning
ROV 3D planning
multi-robot planning
FleetManager
TaskDispatcher
multi-vehicle assignment framework
```

If a proposed abstraction exists only to support one of these future capabilities, defer it to P2+.

### 16. Avoid Premature Generalization

During P1:

- Do not add a generic `IPathPlanner` above `ICoveragePlanner`.
- Do not build a general planner capability/version negotiation system.
- Do not build a full execution-state machine.
- Do not build a ROS/telemetry bridge abstraction.
- Do not build a JSON migration framework without an approved schema-version need.
- Do not build a fleet/multi-robot assignment model.
- Do not create a 3D planning hierarchy.

Prefer the smallest interface that satisfies the frozen P1 specification and can be extended cleanly in P2.

### 17. QGroundControl Extension Strategy

Prefer QGC custom-build extension mechanisms.

Use current implementations as primary references, especially:

```text
custom-example/CMakeLists.txt
custom-example/src/CustomPlugin.*
custom-example/src/MissionManager/PerimeterScanComplexItem.*
custom-example/src/MissionManager/PerimeterScanPlanCreator.*
custom-example/src/PlanView/PerimeterScanEditor.qml
custom-example/src/PlanView/PerimeterScanMapVisual.qml
```

Also inspect current:

```text
QGCCorePlugin
PlanMasterController
MissionController
ComplexMissionItem
SurveyComplexItem
TransectStyleComplexItem
MissionManager tests
```

Borrow QGC integration patterns, not UAV-specific Survey semantics.

Do not copy altitude, terrain, camera-calculation, takeoff, landing, or aerial-survey behavior into Marine coverage unless a future phase explicitly requires it.

### 18. Minimize QGC Core Changes

Marine development should primarily add files under the custom-build area and Marine-specific documentation/tests.

Do not modify QGC core solely for convenience.

Before modifying upstream core:

1. Demonstrate that an existing custom/plugin extension point cannot solve the requirement.
2. Keep the patch minimal.
3. Record why the change is necessary.
4. Add regression coverage.
5. Consider future upstream merge conflicts.

Long-term maintainability against `mavlink/qgroundcontrol` updates is a first-class requirement.

### 19. USV Plan Semantics

Marine USV plan creation must not automatically introduce UAV semantics such as:

```text
Takeoff
Land
```

P0/P1 USV plans should be based on the Marine task and the appropriate mission settings/waypoints for ArduPilot Rover.

Use current QGC and ArduPilot APIs rather than hardcoding assumptions.

### 20. QML Boundary

QML is responsible for presentation and user interaction.

QML may:

- display/edit Marine task parameters;
- request planning;
- display work-region polygons;
- display no-go polygons;
- display generated paths;
- display planning state.

QML must not perform:

```text
polygon intersection
polygon clipping
sweep-line generation
path ordering
route optimization
coverage calculation
```

Those belong in C++ planning/geometry layers.

### 21. Plan-Level Context

Marine task state must be scoped to the relevant QGC plan.

Do not introduce a global Marine task singleton that can accidentally mix state between different `PlanMasterController` instances.

P1 must retain the plan-scoped `MarinePlanContext` design.

Do not add a global `MarinePlanContextRegistry` unless an actual lifecycle/use-case proves it necessary.

### 22. P1 Work-Package Order

Implement P1 incrementally in this order:

```text
P1-00  Implementation Readiness Audit and specification freeze
P1-01  Local Geometry + GeoReference
P1-02  CoveragePlanningProblem + CoverageTaskAdapter
P1-03  Input Validation + Capability Gate
P1-04  Clipper2 + Safety Inset
P1-05  Monotonicity + Scanline
P1-06  Manual-Angle Lawnmower
P1-07  Connector + Path Validation
P1-08  Auto Sweep Angle
P1-09  ComplexItem + PlannerRegistry integration
P1-10  Work Region UI + No-Go read-only visual
P1-11  MissionAdapter + ArduRover semantics
P1-12  Persistence + SensorConfig UI
P1-13  ArduRover SITL
P1-14  Real USV field validation
P1-15  Freeze
```

Do not skip ahead to later work packages without a concrete dependency reason.

For each package:

```text
inspect current API
    ↓
implement minimum change
    ↓
build
    ↓
run targeted tests
    ↓
fix
    ↓
commit
    ↓
stop/report
```

Do not implement the next package automatically unless requested.

### 23. P1 Required Tests

Maintain every P0 Marine test and add focused coverage for:

```text
GeoReference round-trip and known-distance accuracy
geometry validation and deterministic tolerance
safety inset empty/disconnected handling
safetyMarginM > swathWidthM / 2 explicit failure
monotonicity, scanline, connector, and path invariants
manual and auto angle determinism
No-Go rejection without silent ignore
save/load without replanning
PlanningResult to ArduRover mission conversion
SITL and field-validation evidence before Freeze
```

Test the smallest layer first, then the full vertical slice.

The P0 integration chain must remain covered:

```text
create task
    ↓
generate mock plan
    ↓
build mission items
    ↓
save .plan
    ↓
destroy/recreate plan context
    ↓
reload .plan
    ↓
verify task + planning path + mission
```

Also run relevant existing QGC mission/planning regression tests.

### 24. P1 Planning State

When a planning-relevant task field changes, an existing plan must no longer be treated as current.

Use only the stale-detection mechanism frozen in the P1 specification; do not invent a general cache/version negotiation framework.

### 25. P1 Freeze Definition of Done

P1 is frozen only when the following vertical slice works:

```text
Create Coverage Inspection
        ↓
Draw supported Work Region
        ↓
Validate and convert to local meter geometry
        ↓
Apply centerline safety inset
        ↓
Generate and validate fixed-swath coverage path
        ↓
ArduPilotMissionAdapter
        ↓
MAV_CMD_NAV_WAYPOINT
        ↓
Save .plan
        ↓
Reload .plan
        ↓
Task + path + selected angle + mission restored without replanning
        ↓
ArduRover SITL and real USV validation
```

Additionally:

- general convex regions pass the frozen acceptance matrix;
- safety/coverage impossibility and unsupported No-Go are explicit failures;
- modifying a planning-relevant task field invalidates the existing plan state;
- Marine tests pass;
- relevant QGC regression tests pass;
- no unnecessary QGC core changes were introduced;
- Clipper2 remains a pinned, attributed, Marine-private dependency;
- no P2+ feature was implemented opportunistically.

Once the P1 Freeze Definition of Done is satisfied:

**STOP P1 DEVELOPMENT.**

Do not continue adding abstractions or features. Report Freeze completion and wait for P2 design instructions.

### 26. Agent Reporting Requirements for Marine Work

At the end of each Marine work package, report:

1. Files changed.
2. Architecture decisions or deviations from the phase specification.
3. Build command and result.
4. Tests run and results.
5. Remaining known issues.
6. Whether the work package DoD is satisfied.
7. The recommended next work package, without implementing it automatically.

If an implementation requires a material change to the approved architecture, stop and explain the issue before making a broad redesign.
