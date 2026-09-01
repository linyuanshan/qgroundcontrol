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
   - `docs/marine/P0_FOUNDATION_SPEC_v0.2.md` during P0.
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

For P0, use a dedicated branch such as:

```text
feature/marine-p0-foundation
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

### 7. Current Phase: P0

The current implementation target is:

```text
P0 — Marine Robotics Platform Foundation
```

P0 has one purpose: prove the minimum architecture vertical slice.

Required P0 flow:

```text
MarineTask
    ↓
ICoveragePlanner
    ↓
MockCoveragePlanner
    ↓
PlanningResult
    ↓
CoverageInspectionComplexItem
    ↓
ArduPilotMissionAdapter
    ↓
MAVLink MissionItem
    ↓
.plan save/load
```

P0 is complete when this chain works reliably and is tested.

Do not expand P0 merely to make the platform look more complete.

### 8. P0 Design Authority

Before P0 changes, read:

```text
docs/marine/P0_FOUNDATION_SPEC_v0.2.md
```

That document defines the P0 implementation baseline.

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

During P0, `ArduPilotMissionAdapter` only needs to generate waypoint mission items required by the P0 specification.

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

### 14. Minimum P0 Data Model

Keep the P0 model intentionally small.

P0 needs only the concepts required by `P0_FOUNDATION_SPEC_v0.2.md`, including:

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

because P1 immediately depends on that structure.

Do not introduce future framework layers without a demonstrated P0 need.

### 15. P0 Features Explicitly Out of Scope

Do not implement the following during P0:

```text
real lawnmower coverage
polygon-offset safety geometry
no-go routing
BCD/cellular decomposition
automatic sweep-angle optimization
turn-radius / kinematic planning
real camera control
real sonar control
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

If a proposed abstraction exists only to support one of these future capabilities, defer it unless it is strictly necessary to complete the P0 vertical slice.

### 16. Avoid Premature Generalization

During P0:

- Do not add a generic `IPathPlanner` above `ICoveragePlanner`.
- Do not build a general planner capability/version negotiation system.
- Do not build a full execution-state machine.
- Do not build a ROS/telemetry bridge abstraction.
- Do not build a JSON migration framework before there is a real second schema version.
- Do not build a fleet/multi-robot assignment model.
- Do not create a 3D planning hierarchy.

Prefer the smallest interface that satisfies P0 and can be extended cleanly in P1.

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

P0 should use the minimum `MarinePlanContext` design required by the specification.

Do not add a global `MarinePlanContextRegistry` unless an actual lifecycle/use-case proves it necessary.

### 22. P0 Work-Package Order

Implement P0 incrementally in this order:

```text
P0-00  Implementation Readiness Audit

P0-01  MarineTypes + MarineTask
P0-02  MarineTaskJsonCodec
P0-03  ICoveragePlanner + PlannerRegistry + MockCoveragePlanner
P0-04  MarinePlanContext
P0-05  CoverageInspectionComplexItem
P0-06  ArduPilotMissionAdapter
P0-07  CoverageInspectionPlanCreator
P0-08  CustomPlugin integration
P0-09  CoverageInspectionEditor.qml + CoverageInspectionMapVisual.qml
P0-10  .plan Marine save/load integration
P0-11  integration/regression tests
P0-12  manual smoke test
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

### 23. P0 Required Tests

At minimum, maintain:

```text
MarineTaskModelTest
MarineTaskJsonTest
CoveragePlannerTest
CoverageComplexItemTest
MarinePlanIntegrationTest
```

Test the smallest layer first, then the full vertical slice.

The key integration test must cover:

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

### 24. P0 Planning State

P0 requires only the minimum planning state defined by the P0 specification.

When a planning-relevant task field changes, an existing plan must no longer be treated as current.

Do not implement hash/digest-based stale detection during P0 unless the approved P0 specification is revised.

That more rigorous mechanism belongs to P1.

### 25. P0 Definition of Done

P0 is done only when the following vertical slice works:

```text
Create Coverage Inspection
        ↓
MarineTask
        ↓
MockCoveragePlanner
        ↓
PlanningResult
        ↓
Map Display
        ↓
ArduPilotMissionAdapter
        ↓
MAV_CMD_NAV_WAYPOINT
        ↓
Save .plan
        ↓
Reload .plan
        ↓
Task + Path + Mission restored correctly
```

Additionally:

- modifying a planning-relevant task field invalidates the existing P0 plan state;
- Marine tests pass;
- relevant QGC regression tests pass;
- no unnecessary QGC core changes were introduced;
- no P1+ feature was implemented opportunistically.

Once the P0 Definition of Done is satisfied:

**STOP P0 DEVELOPMENT.**

Do not continue adding abstractions or features. Report completion and wait for P1 design/implementation instructions.

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
