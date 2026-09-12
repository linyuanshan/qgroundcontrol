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
   - `docs/marine/P2_COMPLEX_COVERAGE_SPEC_v0.2.md` during P2;
   - `docs/marine/P1_COVERAGE_INSPECTION_SPEC_v0.2.md` for the frozen P1 coverage baseline;
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

### 7. Current Phase: P2

The current implementation target is:

```text
P2 — Complex Coverage Planning V1
```

P0 and the P1 software baseline are frozen. P1 Engineering Freeze is approved; P1 real-USV field validation is
deferred and not executed. P2 adds static complex coverage for a simple outer boundary with finite static No-Go
polygons while preserving P0/P1 behavior and architecture.

Required P2 flow:

```text
MarineTask
    ↓
CoverageTaskAdapter
    ↓
Local ENU CoveragePlanningProblem with outer boundary and No-Go regions
    ↓
CoverageTarget + TrackFeasibleRegion
    ↓
Restricted event-driven slab BCD
    ↓
P1 MonotoneCoverage primitive per cell
    ↓
Forward / Reverse cell traversal
    ↓
Visibility Graph + Dijkstra transit
    ↓
Greedy oriented-cell ordering
    ↓
Canonical path + Coverage/Transit leg roles
    ↓
Nominal coverage completeness
    ↓
CoveragePlanningSolution
    ↓
CoverageTaskAdapter
    ↓
PlanningResult
    ↓
ArduPilotMissionAdapter
    ↓
MAVLink Mission
    ↓
ArduRover SITL
```

P2 solves static complex coverage planning. Do not reopen or redesign P0/P1 merely to implement P2.

### 8. P2 Design Authority

Before P2 changes, read:

```text
docs/marine/P2_COMPLEX_COVERAGE_SPEC_v0.2.md
```

That frozen document defines the P2 implementation baseline. The P0 and P1 specifications remain authoritative
for frozen Task/Plan/Mission separation, persistence, adapter boundaries, plan-scoped context, navigation-angle
semantics, centerline safety, nominal coverage-target semantics, and P1 planner compatibility.

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
CoverageTaskAdapter
    ↓
CoveragePlanningProblem
    ↓
Coverage Planner
    ↓
CoveragePlanningSolution
    ↓
CoverageTaskAdapter
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

A planner returns a `CoveragePlanningSolution`. `CoverageTaskAdapter` converts that solution to a `PlanningResult`.

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
visibility graph construction
static safe routing
cell traversal ordering
nominal coverage completeness
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

P0/P1 waypoint behavior remains frozen. P2 path-leg metadata must not force a MissionAdapter refactor;
`ArduPilotMissionAdapter` may continue consuming only the canonical `PlanningResult.path`.

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

and P2 uses both parts of that structure.

Extend only fields and types required by the frozen P2 specification; centralize task persistence in
`MarineTaskJsonCodec`. P2 planning-artifact evolution must load P1 version 1 artifacts without replanning or
changing waypoint order.

#### P2 Shared-Layer Evolution

- `CoverageTaskAdapter` converts the outer boundary and every No-Go polygon through the same plan-scoped
  `GeoReference`. It must not retain a P1-specific No-Go rejection gate.
- `CoverageProblemValidator` retains generic numeric validity, generic outer-geometry validity, and angle
  normalization. Concrete planners own capability checks: the P1 Lawnmower planner rejects No-Go, while the P2
  BCD planner supports valid No-Go input.
- P2 must extract the tested P1 monotone coverage core into a pure `MonotoneCoverage` primitive. Do not call the
  complete `LawnmowerCoveragePlanner::plan()` for each P2 cell and do not duplicate the algorithm.
- The canonical coordinate source remains `path`. Add `PathLegRole` metadata for Coverage/Transit semantics; do
  not replace the canonical path with geographic path-segment objects.
- `MissionAdapter` remains path-only during P2-01.

Clipper2 remains pinned, Marine-private, and hidden behind the Marine geometry backend. Keep fork-level
attribution in `custom/src/Marine/THIRD_PARTY.md`; do not modify upstream `.github/COPYING.md` to establish a
license policy, and do not make new legal compatibility conclusions.

### 15. P2 Features Explicitly Out of Scope

Do not implement the following during P2:

```text
dynamic obstacle avoidance
online replanning
vehicle footprint modelling
turn-radius / kinematic planning
Dubins or Hybrid A*
Grid A* or NavMesh
TSP or global optimization
per-cell sweep optimization
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

If a proposed abstraction exists only to support one of these future capabilities, defer it to P3+.

### 16. Avoid Premature Generalization

During P2:

- Do not add a generic `IPathPlanner` above `ICoveragePlanner`.
- Do not build a general planner capability/version negotiation system.
- Do not build a full execution-state machine.
- Do not build a ROS/telemetry bridge abstraction.
- Do not build a JSON migration framework without an approved schema-version need.
- Do not build a fleet/multi-robot assignment model.
- Do not create a 3D planning hierarchy.

Do not introduce decomposer, router, or orderer registries while P2 has only one implementation of each. Prefer
plain functions, small classes, and pure data structures behind the existing `ICoveragePlanner` plugin boundary.

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

P0/P1/P2 USV plans should be based on the Marine task and the appropriate mission settings/waypoints for ArduPilot Rover.

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

P2 must retain the plan-scoped `MarinePlanContext` design.

Do not add a global `MarinePlanContextRegistry` unless an actual lifecycle/use-case proves it necessary.

### 22. P2 Work-Package Order

Implement P2 incrementally in this order:

```text
P2-00  Implementation Readiness Audit
P2-01  Path Leg Semantics + P1 Monotone Coverage Primitive Extraction
P2-02  No-Go Validation + Free-Space Geometry
P2-03  Restricted Event-driven Slab BCD
P2-04  Decomposition Validation
P2-05  Cell Coverage + Traversal States
P2-06  Visibility Graph + Dijkstra
P2-07  Greedy Oriented-Cell Ordering
P2-08  Complex Plan Assembly
P2-09  Nominal Coverage Completeness
P2-10  Planner Integration
P2-11  No-Go UI
P2-12  Persistence + Mission Integration
P2-13  Integrated + SITL Validation
P2-14  Real USV Validation
P2-15  Final Freeze
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

### 23. P2 Required Tests

Maintain every P0/P1 Marine test and add the P2 matrix defined by the frozen specification, including focused
coverage for:

```text
strict No-Go topology validation
CoverageTarget and TrackFeasibleRegion geometry
reachability and connectivity failures
event-driven slab BCD cell invariants
MonotoneCoverage primitive P1 equivalence
Forward/Reverse traversal equivalence
Visibility Graph and Dijkstra route safety
deterministic oriented-cell ordering
canonical path and PathLegRole invariants
nominal coverage completeness
P2 save/load and P1 version 1 compatibility
unchanged MissionAdapter waypoint semantics
P2-A through P2-D SITL validation
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

### 24. Planning State

When a planning-relevant task field changes, an existing plan must no longer be treated as current.

No-Go add/edit/delete is planning-relevant in P2. Continue using the stale-detection mechanism frozen in P1; do
not invent a general cache/version negotiation framework.

### 25. Frozen P1 Baseline and P2 Freeze Definition of Done

P1 Engineering Freeze is approved. The following P1 behavior is frozen and must remain compatible:

```text
Task / Plan / Mission separation
MarineTask, WorkRegion, and CoverageConfig
CoveragePlanningProblem and ICoveragePlanner boundaries
CoveragePlanningSolution → CoverageTaskAdapter → PlanningResult
0 degrees North, 90 degrees East, clockwise-positive navigation angles
safetyMargin as centerline clearance and WorkRegion as nominal Coverage Target
P1 LawnmowerCoveragePlanner behavior
ArduPilotMissionAdapter waypoint semantics
P1 persistence and save/load without replanning
```

Shared-layer evolution must preserve P1 compatibility and pass P1 regression.

P1 real-USV field validation is **DEFERRED and NOT EXECUTED**. Do not claim it is complete. Do not delete or
weaken `docs/marine/P1_REAL_USV_FIELD_VALIDATION_PROTOCOL.md`; it must be completed before P2 real-USV field
acceptance.

P2 is frozen only when the following vertical slice works:

```text
Draw WorkRegion and static No-Go polygons
        ↓
Build CoverageTarget and TrackFeasibleRegion
        ↓
Restricted event-driven slab BCD
        ↓
Cover every cell with the P1 MonotoneCoverage primitive
        ↓
Connect cells with Visibility Graph + Dijkstra transit
        ↓
Assemble canonical path + Coverage/Transit leg roles
        ↓
Validate static centerline safety and nominal coverage completeness
        ↓
Save/load without replanning and preserve P1 version 1 compatibility
        ↓
Generate unchanged ArduRover waypoint missions
        ↓
Pass P1 regression, P2 SITL, and required field gates
```

Additionally:

- simple concave regions and finite valid static No-Go polygons pass the frozen P2 matrix;
- every coverage cell is valid, monotone, deterministic, and visited once;
- every transit leg lies in `TrackFeasibleRegion`;
- `CoverageTarget` passes nominal completeness validation;
- modifying any planning-relevant task field invalidates the artifact;
- Marine tests pass;
- relevant QGC regression tests pass;
- no unnecessary QGC core changes were introduced;
- Clipper2 remains a pinned, attributed, Marine-private dependency;
- no P3+ feature was implemented opportunistically.

Once the P2 Freeze Definition of Done is satisfied:

**STOP P2 DEVELOPMENT.**

Do not continue adding abstractions or features. Report Freeze completion and wait for the next approved phase.

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
