# P2 Complex Coverage v0.4 — Execution Safety Amendment

Status: **DESIGN FROZEN FOR P2-13F IMPLEMENTATION REVIEW**

P2-13D is closed. P2-13 remains **BLOCKED**, S04 remains **FAIL**, and P2-14 must not start.

This amendment records a design audit and offline experiment. It does not modify production planning,
`ArduPilotMissionAdapter`, the Geometry backend, persistence v2, or the frozen S04 result.

## Confirmed System Problem

The current path is:

```text
Clipper backend geometry
        -> TrackFeasibleRegion
        -> BoundaryCoverageSupport
        -> backend boundary vertices
        -> CoveragePlanningSolution.path
        -> PlanningResult.path
        -> MAV_CMD_NAV_WAYPOINT sequence
```

`BoundaryCoverageSupport` correctly preserves backend geometry for static validation, but the same vertices are
also exposed directly as vehicle waypoints. Backend round-offset tessellation is therefore leaking across the
planning/execution boundary.

This is not a reason to reduce `CoordinateScalePerM`, enlarge `ArcToleranceClipperUnits`, or globally simplify
Clipper output. Backend precision serves polygon boolean operations, BCD, containment, and coverage
completeness. Vehicle execution geometry must be derived separately and must be validated against that precise
geometry.

## Evidence Baseline

P2-13 proved that every S03/S04 canonical centerline leg is inside `TrackFeasibleRegion` and that nominal
coverage is complete. Live Rover execution then produced:

- S03: original No-Go avoided, but the 1 m safety band was entered.
- S04: 30 of 939 samples inside the original No-Go, with `1.5458109509 m` maximum penetration.

P2-13D found strong radius sensitivity but no parameter-only safety contract. Half speed did not close the
failure, and ALL-STOP still entered the No-Go. The failure is therefore Case D: waypoint acceptance, tracking
and controller dynamics, and path geometry must be handled together. Vehicle tuning and stop hints are not the
P2-13E solution.

## Offline Method

The committed density analyzer reads the persisted S03/S04 v2 artifacts. It converts their geographic paths to
a WGS84 local tangent approximation; reconstructed total lengths differ from the stored planner metrics by less
than `0.0003 m`.

The frontier helper uses production pure APIs:

- the frozen evidence sweep angles: S03 `90 deg`, S04 `90.00014626 deg`;
- `safetyMarginM = 1 m`, `swathWidthM = 4 m`;
- the production BCD planner, segment-in-region predicate, and nominal completeness validator;
- the original `CoverageTarget` for every completeness check.

Round is the current production outer miter inset plus round No-Go inflation. Miter uses the same production
outer inset and an analytically miter-inflated rectangle for the two frozen rectangular No-Go scenarios. The
Miter construction is an experiment, not a general production implementation. Every successful path was
generated twice and was bitwise deterministic.

Generated JSON remains uncommitted under `build/P2-13E-analysis`.

## Canonical Waypoint Density

Length columns are `[minimum, P05, median, P95, maximum]` in metres. A role turn is counted only when both
adjacent legs have that role; mixed-role joins remain in the full-path turn count.

| Scenario | Scope | Points | Legs | Total length | Length distribution | Turns |
| --- | --- | ---: | ---: | ---: | --- | ---: |
| S03 | Full path | 173 | 172 | 237.578857 | `[0.043829, 0.043932, 0.044911, 10.418348, 18.000055]` | 171 |
| S03 | Coverage | 166 | 156 | 190.284227 | `[0.043829, 0.043906, 0.044777, 9.000008, 18.000055]` | 146 |
| S03 | Transit | 25 | 16 | 47.294630 | `[0.089269, 0.155862, 2.000000, 9.885547, 11.883549]` | 7 |
| S04 | Full path | 183 | 182 | 305.384081 | `[0.043829, 0.043932, 0.044944, 10.000030, 28.000005]` | 181 |
| S04 | Coverage | 168 | 158 | 216.284179 | `[0.043829, 0.043916, 0.044777, 10.000021, 28.000005]` | 148 |
| S04 | Transit | 33 | 24 | 89.099902 | `[0.044011, 0.134302, 0.700243, 18.918063, 25.020017]` | 15 |

| Scenario / role | Turn angles `[min, P05, median, P95, max]` degrees |
| --- | --- |
| S03 Coverage | `[1.076644, 1.250864, 2.551805, 3.951098, 90.000088]` |
| S03 Transit | `[6.810377, 7.067367, 10.220295, 19.365878, 20.530102]` |
| S04 Coverage | `[1.076646, 1.253816, 2.551805, 4.061014, 90.000088]` |
| S04 Transit | `[1.301954, 1.476853, 10.480841, 18.842581, 20.408676]` |

| Scenario / role | `<0.01 m` | `<0.05 m` | `<0.10 m` | `<0.50 m` |
| --- | ---: | ---: | ---: | ---: |
| S03 Full path | 0 | 140 | 141 | 146 |
| S03 Coverage | 0 | 140 | 140 | 140 |
| S03 Transit | 0 | 0 | 1 | 6 |
| S04 Full path | 0 | 141 | 141 | 152 |
| S04 Coverage | 0 | 140 | 140 | 140 |
| S04 Transit | 0 | 1 | 1 | 12 |

The approximately `0.044 m` median is not a coverage-lane requirement. It is the round offset arc resolution.

### Structural attribution

| Scenario | Structure | Points | Legs | Length | Min / median / max leg | Turns |
| --- | --- | ---: | ---: | ---: | --- | ---: |
| S03 | No-Go boundary support | 145 | 144 | 22.284025 | `0.043829 / 0.044733 / 4.006008` | 143 |
| S03 | Outer boundary support | 13 | 8 | 144.000165 | `18.000000 / 18.000009 / 18.000055` | 3 |
| S03 | Cell coverage | 8 | 4 | 24.000037 | `6.000008 / 6.000009 / 6.000010` | 0 |
| S03 | Other / Transit | 25 | 16 | 47.294630 | `0.089269 / 2.000000 / 11.883549` | 7 |
| S04 | No-Go boundary support | 145 | 144 | 22.284018 | `0.043829 / 0.044733 / 4.006005` | 143 |
| S04 | Outer boundary support | 14 | 8 | 122.000100 | `8.000000 / 10.000029 / 28.000005` | 2 |
| S04 | Cell coverage | 11 | 6 | 72.000062 | `2.000001 / 14.000012 / 20.000020` | 1 |
| S04 | Other / Transit | 33 | 24 | 89.099902 | `0.044011 / 0.700243 / 25.020017` | 15 |

Both scenarios have the same 4 m by 4 m rectangular No-Go. Its 1 m round-inflated execution boundary has:

| Metric | S03 | S04 |
| --- | ---: | ---: |
| Backend hole vertices | 144 | 144 |
| Perimeter | 22.284025 m | 22.284018 m |
| Mean waypoint spacing | 0.154750 m | 0.154750 m |
| Minimum waypoint spacing | 0.043829 m | 0.043829 m |
| Median waypoint spacing | 0.044733 m | 0.044733 m |
| Mission waypoint occurrences caused only by round-arc tessellation | 136 | 136 |

Thus 136 of 173 S03 mission waypoints and 136 of 183 S04 mission waypoints are non-structural Clipper arc
samples. The backend-to-mission leak dominates waypoint count and turn count.

## Round Versus Conservative Miter

At the two margins that retain completeness for Miter:

| Scenario | Candidate | Execution margin | Free area | Hole vertices | Hole perimeter | Path points | Turns | Min leg | Complete |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| S03 | Round | 0.00 | 288.858254 | 144 | 22.284008 | 173 | 171 | 0.043829 | YES |
| S03 | Miter | 0.00 | 288.000000 | 4 | 24.000000 | 27 | 25 | 2.000000 | YES |
| S03 | Round | 0.25 | 265.340790 | 160 | 23.855149 | 0 | 0 | N/A | NO, unsafe connector |
| S03 | Miter | 0.25 | 264.000000 | 4 | 26.000000 | 27 | 25 | 1.500000 | YES |
| S04 | Round | 0.00 | 288.858254 | 144 | 22.284008 | 183 | 181 | 0.043829 | YES |
| S04 | Miter | 0.00 | 288.000000 | 4 | 24.000000 | 31 | 29 | 1.999946 | YES |
| S04 | Round | 0.25 | 260.340790 | 160 | 23.855149 | 0 | 0 | N/A | NO, unsafe connector |
| S04 | Miter | 0.25 | 259.000000 | 4 | 26.000000 | 31 | 29 | 1.500000 | YES |

Miter is more conservative at convex No-Go corners and gives slightly less free area. It also represents the
same rectangular obstacle with four structural corners. Round becomes more densely tessellated as the margin
increases: 144, 160, 172, 188, then 200 vertices in S03. Miter remains at four.

Ordinary Douglas-Peucker simplification is rejected. A chord between two round-arc samples can cross the
inflated obstacle. Any future coarse segment must first be generated from conservative support geometry and
then pass the segment-in-execution-region predicate.

## Execution-Margin Feasibility Frontier

`G` is non-empty geometry, `C` is connected, and `V` is complete coverage of the unchanged original
`CoverageTarget`. A dash means no valid path was produced.

### S03

| Candidate | Execution margin | G | C | V | Path points | Turns |
| --- | ---: | --- | --- | --- | ---: | ---: |
| Round | 0.00 | YES | YES | YES | 173 | 171 |
| Round | 0.25 | YES | YES | NO | - | - |
| Round | 0.50 | YES | YES | NO | - | - |
| Round | 0.75 | YES | YES | NO | - | - |
| Round | 1.00 | YES | YES | NO | - | - |
| Miter | 0.00 | YES | YES | YES | 27 | 25 |
| Miter | 0.25 | YES | YES | YES | 27 | 25 |
| Miter | 0.50 | YES | YES | NO | 27 | 25 |
| Miter | 0.75 | YES | YES | NO | 27 | 25 |
| Miter | 1.00 | YES | YES | NO | 22 | 20 |

S03 Miter uncovered area is `0.060303`, `0.992063`, and `3.434412 m2` at margins 0.50, 0.75, and 1.00,
respectively, versus the unchanged `0.01 m2` tolerance.

### S04

| Candidate | Execution margin | G | C | V | Path points | Turns |
| --- | ---: | --- | --- | --- | ---: | ---: |
| Round | 0.00 | YES | YES | YES | 183 | 181 |
| Round | 0.25 | YES | YES | NO | - | - |
| Round | 0.50 | YES | YES | NO | - | - |
| Round | 0.75 | YES | YES | NO | - | - |
| Round | 1.00 | YES | NO | NO | - | - |
| Miter | 0.00 | YES | YES | YES | 31 | 29 |
| Miter | 0.25 | YES | YES | YES | 31 | 29 |
| Miter | 0.50 | YES | YES | NO | - | - |
| Miter | 0.75 | YES | YES | NO | 32 | 30 |
| Miter | 1.00 | NO | NO | NO | - | - |

S04 Miter at 0.50 fails cell-lane generation before assembly. At 0.75 it produces geometry but leaves
`1.240142 m2` uncovered. At 1.00 the conservative obstacle closes the available connection. These are real hard
limits; tolerance must not be enlarged.

The common offline feasible interval for both frozen scenarios is therefore:

```text
conservative Miter No-Go + executionMarginM in [0.00, 0.25]
```

The frozen Rover candidate is `executionMarginM = 0.25 m`. It is the largest sampled common margin that retains
geometry, connectivity, and nominal completeness. It is not claimed as a universal vehicle constant.

## Waypoint-Density Answer

With no added execution margin, replacing round backend support with four-corner conservative Miter support
does satisfy all three offline gates for S03 and S04:

1. every leg is inside its execution region;
2. original nominal coverage remains complete;
3. path complexity falls from 173/183 points to 27/31 points.

Backend tessellation leakage is therefore a demonstrated primary defect. It is not, by itself, a complete
execution-safety contract. P2-13D showed that actual tracking and waypoint acceptance can depart materially
from a geometrically valid path. A separate execution-margin dimension remains necessary, and P2-13F must
validate the `0.25 m` candidate in SITL rather than infer success from the offline result.

## Frozen Architecture Decision

### Decision: planner-integrated execution region

P2-13F should not introduce a second `PlanningResult -> ExecutablePath` replanning layer. The BCD planner must
produce one execution-safe canonical path:

```text
Task CoverageTarget
        + pure ExecutionSafetyProfile
        -> precise NominalTrackFeasibleRegion
        -> conservative ExecutionTrackFeasibleRegion
        -> BCD / coverage / routing / assembly
        -> unique execution-safe CoveragePlanningSolution.path + legRoles
        -> PlanningResult
        -> unchanged ArduPilotMissionAdapter
        -> MAV_CMD_NAV_WAYPOINT
```

Reasons:

- Miter changes the obstacle, cells, boundary support, routing, and sometimes connectivity. It is not a safe
  post-process on an already assembled polyline.
- A post-planning adapter would duplicate decomposition/routing responsibilities or silently lose coverage.
- Two stored paths create ambiguity about which one is canonical and complicate stale detection and v2 load.
- Planner integration keeps the Geometry and completeness gates adjacent to path generation while retaining
  planner purity.

`ICoveragePlanner` must not read `Vehicle*`, ArduPilot parameters, `Fact`, or `MissionItem`. A higher layer may
select a pure-data profile, but the planner receives only values.

Candidate V1 data:

```cpp
struct ExecutionSafetyProfile
{
    double executionMarginM = 0.0;
};
```

No turn-radius, speed, acceleration, PID, or hydrodynamic model is approved for V1.

### Margin semantics

`safetyMarginM` remains unchanged: it is the nominal planned-centerline clearance from WorkRegion and No-Go
boundaries. `executionMarginM` is additional clearance reserved for waypoint acceptance and tracking/turn
dynamics.

```text
NominalTrackFeasibleRegion
  = Inset(W, safety) - RoundInflate(N, safety)

ExecutionTrackFeasibleRegion
  = Inset(W, safety + executionMargin)
    - ConservativeMiterInflate(N, safety + executionMargin)
```

`CoverageTarget` remains exactly `W - N`. The final Coverage-role footprint must cover that original target or
return `CoverageIncomplete`. The execution region must also be proved to be a subset of the nominal feasible
region. Geometry backend precision and coverage tolerance remain unchanged.

### Task, Plan, executable plan, and Mission

- **Task:** the user's WorkRegion, No-Go regions, swath, nominal safety requirement, and planner selection.
- **Plan:** one complete, continuous, role-labelled path generated under an explicit execution-safety profile.
- **Executable plan:** in P2 it is the Plan's same canonical path, not a second path object.
- **Mission:** a lossless MAVLink encoding of that already-approved canonical path.

`ArduPilotMissionAdapter` remains a translator. It must not offset polygons, simplify paths, check collisions,
reroute, or validate coverage.

## Persistence Recommendation

No v2 schema changes are made by this amendment. Because the preferred design retains one path, a future v3
should persist:

- the execution-safe canonical path and roles;
- the pure `ExecutionSafetyProfile` used to generate it;
- the execution-boundary policy identifier needed for deterministic audit.

Load must continue to use the stored path without replanning. Existing v1/v2 artifacts load unchanged and
retain their historical semantics; they must not silently gain a nonzero margin. If a future design ever
introduces distinct planning and executable paths, both must be stored, not rebuilt on load, to preserve the
frozen no-replanning invariant.

## Runtime Geofence Defence-in-Depth

Marine No-Go polygons should be evaluated for ArduRover exclusion-fence upload as a second barrier after
P2-13F proves the executable path. Benefits are independent runtime enforcement and protection against mission
or tracking faults. Costs and risks are firmware/capability dependence, upload/verification failure modes,
limited fence capacity, and a breach action that may stop or redirect the vehicle and therefore abort coverage.

Recommendation: add a later, explicitly vehicle-capability-gated geofence package. Fence upload must be
verified before arming, failure must fail closed, and mission continuation after a fence action must require an
explicit recovery decision. Geofence must never substitute for an execution-safe path and is not part of
P2-13F.

## P2-13F Production Scope

P2-13F should implement only the frozen direction:

1. Add pure `ExecutionSafetyProfile` input to `PlannerConfig` and `CoveragePlanningProblem`, with a single
   validated `executionMarginM` scalar and a zero default only for legacy artifacts.
2. Add a Marine-private geometry operation for conservative Miter No-Go inflation; do not expose Clipper types.
3. Build and validate `ExecutionTrackFeasibleRegion` as a subset of `NominalTrackFeasibleRegion`.
4. Run BCD, cell coverage, boundary support, static routing, ordering, and assembly against the execution region.
5. Validate every final leg against the execution region and validate Coverage roles against the unchanged
   `CoverageTarget`.
6. Preserve one canonical path and leave `ArduPilotMissionAdapter` path-only and unchanged.
7. Introduce planning-artifact v3 to persist the profile, execution-boundary policy, canonical path, and roles;
   load v1/v2 unchanged and never replan on load.
8. Add deterministic Round-versus-Miter, margin-frontier, no-short-arc-waypoint, safety, completeness, and
   backward-compatibility tests.
9. Integrate the frozen `0.25 m` Rover profile candidate and repeat S03/S04 SITL. S04 must have zero samples
   inside the original No-Go and still complete its mission and CoverageTarget.

Explicitly excluded from P2-13F: parameter tuning, stop hints, turn-radius models, path smoothing frameworks,
geofence integration, P2-14 field validation, and any MissionAdapter planning behavior.

## New Acceptance Criteria

P2-13 cannot close until all of the following pass:

- Geometry: every canonical leg is inside `ExecutionTrackFeasibleRegion`, which is inside the nominal feasible
  region.
- Coverage: the unchanged `CoverageTarget` is complete at the existing tolerance.
- Complexity: rectangular No-Go execution support uses structural Miter corners, not round backend arc samples;
  frozen S03/S04 contain no sub-0.5 m execution legs and remain deterministic.
- Purity: planner input is pure data; no Vehicle/Fact/MissionItem dependency enters planning.
- Persistence: the eventual versioned artifact reloads without replanning and preserves older artifacts.
- Mission: `ArduPilotMissionAdapter` remains a lossless path translator.
- SITL: S03 and S04 upload, execute, reach Mission Complete, and record zero samples inside every original No-Go.
- Regression: all Marine tests, Debug/Release builds, and related QGC tests add no code regression.

Priority remains: No-Go execution safety, coverage completeness, deterministic geometry, planner purity,
waypoint count, path length, then turns.

## Final Status

Recommended direction: **conservative Miter execution geometry plus a pure `executionMarginM`, integrated inside
the BCD planner before decomposition**. The frozen S03/S04 candidate margin is `0.25 m`.

P2-13 remains **BLOCKED**. S04 remains **FAIL**. P2-14 remains **DO NOT START**.
