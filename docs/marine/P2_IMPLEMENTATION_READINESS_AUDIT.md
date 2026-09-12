# P2 Implementation Readiness Audit

Audit date: 2026-09-12. Work package: P2-00. Review owner: project lead.

This audit inspects the frozen baseline and reruns its build/test gates. It implements no P2 production logic.
P2-01 remains subject to review of this audit and a separate instruction to proceed.

## 1. Branch and SHA baseline

| Preflight command | Result before audit edits |
| --- | --- |
| `git status --short` | Empty: clean index and working tree |
| `git branch --show-current` | `feature/marine-p2-complex-coverage` |
| `git rev-parse HEAD` | `d756f38c320b5cbab5fa3d032529799239ca7b81` |
| `git rev-parse marine/main` | `846bd75da9167100b0c9e53815db36e047447fb3` |
| `git rev-parse master` | `837871f4a3319a6134c393e4a1a85a4062a96b40` |
| `git rev-parse upstream/master` | `837871f4a3319a6134c393e4a1a85a4062a96b40` |
| `git merge-base marine/main HEAD` | `846bd75da9167100b0c9e53815db36e047447fb3` |

The branch contains current local `marine/main`. Its two subsequent commits change only `AGENTS.md` and the
P2 specification. No production implementation differs from the integrated P1 baseline. Remote freshness is
not inferred from a local remote-tracking reference; this audit does not update upstream or rebase the branch.

Requested six-commit log:

```text
d756f38c3 docs(marine): activate P2 development phase
4ac1500e9 docs(marine): add P2 complex coverage specification
846bd75da merge(marine): freeze P1 coverage engineering baseline
48e5cba90 docs(marine): approve P1 engineering freeze with field validation deferred
90a546650 test(gps): avoid MSVC self-capture in provider fixture
f22cdb6c6 Merge remote-tracking branch 'upstream/master' into feature/marine-p1-final-upstream-sync-2
```

Read before the first edit: `AGENTS.md`, `CODING_STYLE.md`, `.github/CONTRIBUTING.md`, `test/README.md`,
`tools/README.md`, `.github/ci-overview.md`, the P0/P1/P2 v0.2 specifications, and
`P1_ENGINEERING_FREEZE_AUDIT.md`. Also inspected the Windows workflow, pre-commit configuration, custom CMake
registration, active build caches, current Marine sources/tests, and QGC integration call sites.

## 2. P1 frozen baseline confirmation

[P1 Engineering Freeze](P1_ENGINEERING_FREEZE_AUDIT.md) is approved; its validated software candidate is
`90a54665028108e0ed5a7c436d7a4c3b284dd2a0`. P1 ArduRover SITL S01-S05 passed historically; it was not rerun here.
P1 real-USV validation is **DEFERRED and NOT EXECUTED**. The unchanged
[field protocol](P1_REAL_USV_FIELD_VALIDATION_PROTOCOL.md) must close before P2 field acceptance, not before P2
software development.

Current code explicitly converts navigation angles using `90 - navigationAngle`, normalized modulo 180.
Manual 0/90-degree tests check actual waypoint coordinates. Positive-safety tests check nominal outer-region
cross-track coverage, not merely coverage of the inset. These corrected behaviors are frozen.

The current implementation, P1 specification section 20, and regression fixtures take precedence over blindly
copying the older illustrative lane-placement formula in P1 section 28: current first/last lanes are constrained
by the nominal target and safe region together. No P1 algorithm, path order, error behavior, or mission semantics
is changed by this audit.

## 3. P2 architecture compatibility

The existing flow is suitable:

```text
MarineTask -> CoverageTaskAdapter -> CoveragePlanningProblem
           -> ICoveragePlanner -> CoveragePlanningSolution
           -> CoverageTaskAdapter -> PlanningResult
           -> ArduPilotMissionAdapter -> MissionItem[]
```

Planner inputs, outputs, and geometry are Marine-owned STL types. `MarinePlanContext` is a QObject child of
each `PlanMasterController`; `CustomPlugin::marinePlanContextFor` finds/creates that direct child and registers
planners there. This is not a global task/context registry. Existing custom plugin hooks suffice for P2.

Material implementation details to preserve:

- `CoveragePlanningSolution` is defined in `Planning/CoveragePlanningProblem.h`, not a separate solution file.
- `CoverageInspectionComplexItem` lives under `custom/src/MissionManager`, not under `Marine/QGC`.
- `GeoReference::create/toLocal/toGeo` use optional results. `buildProblem` returns an optional reference to the
  caller; the reference is local to that planning conversion, not a persistent context member or global service.
- Artifact JSON is owned by ComplexItem; task JSON is centralized in `MarineTaskJsonCodec`.
- Context task-change signals invalidate the result. There is no input-digest/cache negotiation subsystem.

No QGC core change, ROS dependency, registry above `ICoveragePlanner`, or new execution command is needed.

## 4. CoverageTaskAdapter required changes

`Planning/CoverageTaskAdapter.cc::buildProblem` currently clears outputs, checks basic task validity, creates a
reference from the outer boundary, converts only that boundary, copies configuration, invokes the validator,
then rejects any non-empty geographic No-Go list with `UnsupportedNoGoRegion`. Thus the adapter gate is real;
the validator sees an empty local No-Go list on this route.

P2-02 minimum change: convert every No-Go with the existing `toLocalPolygon` helper and the **same** reference
used for the outer boundary, populate `Region2D::noGoRegions`, then perform generic validation. Publish outputs
only after successful conversion. Keep conversion failures atomic, report an appropriate No-Go input error,
and retain outer-boundary/WGS84/numeric failure handling. Do not create one reference per polygon.

Remove the adapter's capability gate. An adapter must not reject valid No-Go on behalf of all planners.
`MarineTask::isValid()` currently checks only ID and outer vertex count; it is not strict topology validation.
No-Go topology must be checked in the pure planning/geometry layer, not assumed valid because the task loaded.

## 5. Validator and capability boundary

`CoverageProblemValidator::validateAndNormalize` currently performs these checks in order: simple finite
non-degenerate outer polygon, positive finite swath, non-negative finite safety, angle mode/normalization,
then non-empty No-Go rejection. It does not currently impose a safety-versus-half-swath gate.

| Existing check/test | P2 disposition |
| --- | --- |
| Outer geometry, numeric checks, Manual modulo 180, Auto angle reset | Retain in generic validator |
| Final `!problem.region.noGoRegions.empty()` rejection | Move to Lawnmower capability handling after generic validation and before inset |
| `CoverageProblemValidatorTest::_testNoGoCapabilityGate` | Replace with valid No-Go preservation/generic acceptance; add direct Lawnmower rejection coverage |
| `CoverageTaskAdapterTest::_testUnsupportedNoGoRegion` | Replace with shared-reference conversion and round-trip checks; cover conversion failure/reset |
| `CoveragePlannerTest::_testCapabilityGate` | Retain: this tests Mock Planner, which also needs its own rejection gate |
| Validator `_testErrorMapping` assertion that message contains `P2` | Update with planner-specific unsupported-capability wording when the gate moves |
| Generic numeric and normalization tests | Retain, including acceptance of finite safety greater than half-swath at this layer |

`MockCoveragePlanner` currently relies on the shared validator. Removing its gate without adding a local
capability check would silently accept and ignore No-Go. Preserve its architecture-test-only behavior.
Add Lawnmower multi-invalid-input precedence tests so moving the gate does not accidentally change P1 errors.

P2's strict No-Go helper validates each polygon and pairwise topology: no outside, crossing, touching,
overlapping, or nested No-Go. Invalid topology maps to `InvalidInput`; valid post-inflation merges are allowed.
`BoustrophedonCoveragePlanner` invokes this validation and supports valid No-Go. Monotonicity, inset results,
connector feasibility, and coverage feasibility remain planner decisions. Keep the generic validator independent
of selected planner IDs; no capability framework is needed. This migration belongs to P2-02, not P2-00/P2-01.

## 6. MonotoneCoverage extraction plan

`LawnmowerCoveragePlanner.cc::generateCandidate` is not currently a polygon-only primitive. It reads both
`problem.region.outerBoundary` and `navigablePolygon`. For a 20 x 10 m rectangle, swath 4 m, safety 1 m,
Manual 90 degrees, the frozen lanes are y=2,5,8 with endpoints x=1,19 and total length 60 m. Treating the inset
as the nominal target would change the lanes and lose the frozen nominal boundary guarantee.

Recommended minimal adaptation to the specification's conceptual interface:

```cpp
MonotoneCoverageResult generateMonotoneCoverage(
    const Polygon2D& polygon,
    double swathWidthM,
    double navigationAngleDeg,
    std::span<const double> lanePositionsYM);
```

The last argument is a finite, ordered lane schedule in the fixed sweep frame. It is concrete geometric input,
not a safety margin, task, global target, or strategy interface. A three-argument convenience overload for P2
can derive a cell-local schedule through the same small lane-spacing helper; it must call the same core.
Review this signature adaptation before P2-01 rather than introducing an incompatible polygon-only extraction.

| Stay in Lawnmower orchestration | Enter pure `Planning/MonotoneCoverage.h/.cc` |
| --- | --- |
| Generic validation and planner capability gate | Fixed-angle polygon/sweep-frame handling and defensive monotonicity check |
| One safety inset and its existing error mapping | Scanline intersection, exactly one usable interval per requested lane |
| Nominal/safe cross-track extents and feasible first/last lane constraints | Shared arithmetic for spacing a supplied feasible lane range, if extracted |
| Auto edge candidates, deduplication, candidate filtering/ranking and failure priority | Alternating lane endpoints, canonical path, lane index ranges and Coverage/Transit annotations |
| Manual/Auto success messages and selected-angle orchestration | Finite points, spacing, full segment containment and unsafe-connector rejection |
| P1 nominal first/last lane reach checks | Path length and lane/turn metrics from the generated geometry |

Output should contain status/error, one canonical `path`, `legRoles`, lane ranges or endpoint indices (not a
second coordinate copy), spacing, and length/turn metrics required by the caller. Preserve current error codes
and waypoint order. Empty/failed output must not carry partial roles or coordinates.

The primitive receives already navigable geometry, never applies inset, never chooses Auto angles, never
accepts `CoveragePlanningProblem`, and never calls a complete planner. A P2 cell with an unsafe direct lane
connector fails explicitly; monotonicity alone is not proof that every zigzag connector is inside a concavity.
Global nominal completeness remains P2-09's responsibility.

P2-01 equivalence evidence must cover the existing rectangle 0/90, narrow one-lane, positive safety, impossible
safety, arbitrary convex, non-monotone, unsafe-connector and all Auto ranking/determinism fixtures. Capture
complete pre-extraction paths/errors/metrics for differential comparison; checking only total length is
insufficient. Do not change the angle deduplication threshold during the extraction.

## 7. PathLegRole impact analysis

Add a small `Planning/PathLegRole.h` enum (`Coverage`, `Transit`) and vectors in both existing result structures.
For N=0, roles are empty; otherwise roles have N-1 entries. Generated successful coverage has at least two
points. `legRoles[i]` describes `path[i] -> path[i+1]`; coordinates remain canonical and unique in ownership.

| Consumer/producer | Minimum impact |
| --- | --- |
| Monotone primitive / Lawnmower | Sweep legs Coverage; lane connectors Transit; preserve all P1 points |
| Mock Planner | Explicit role policy for its synthetic path, without claiming physical coverage |
| CoverageTaskAdapter | Validate role cardinality and copy roles unchanged through ENU-to-geographic mapping; clear both on conversion failure |
| ComplexItem | Store/move complete result; invalidation clears roles with path; later expose roles via normal property notification |
| MapVisual | Current single `MapPolyline` remains valid; later derive styled adjacent legs from path plus roles, without geometry logic in QML |
| Persistence | Version-aware metadata extension and legacy policy, detailed below |
| MissionAdapter | No production change: continue consuming `PlanningResult.path` only |

Avoid adding P2 cell/global metrics before their producing packages need them. New role checks must not reject
old loaded paths merely because v1 lacked metadata. Role-only display updates must use `planningResultChanged`
or an explicit role signal; current `generatedPathChanged` only detects coordinate changes.

## 8. Geometry backend readiness

`GeometryTypes.h` has `Point2D`, `Polygon2D`, and input `Region2D`. `MarineGeometry.h/.cc` implements simple
polygon validation, inset, navigation/math angle conversion, sweep transforms, monotonicity, scanline intervals,
and point/whole-segment containment for a **single hole-free polygon**.

Clipper2 2.0.1 is pinned in `custom/CMakeLists.txt` and attributed in `Marine/THIRD_PARTY.md`.
`MarineGeometry` links `MarineClipper2` privately. The cached backend has boolean operations, `PolyTree64`, and
offset APIs; these capabilities are not yet exposed as Marine APIs. Planner headers must never include Clipper2.

| Missing Marine capability | Minimum addition behind Geometry |
| --- | --- |
| `PolygonRegion2D` / `PolygonRegionSet2D` | One outer plus holes, and a vector of connected components; equivalent to spec's `PolygonWithHoles2D`, not a new task schema |
| No-Go inflate and multi-output outer inset | Return region sets, retain empty/disconnected results instead of dropping components |
| Polygon union/difference | Build `W - Union(N)` and `Inset(W,s) - Union(Inflate(N,s))` |
| Region buffer | Reachability envelope of navigable region at swath/2 |
| Open-segment round-cap buffer | Nominal coverage footprint, Coverage legs only |
| Connected-component extraction | Preserve parent/hole ownership using backend tree output; do not count every ring as a component |
| Area and difference-area | Shared decomposition/completeness comparison |
| Point classification and segment containment with holes | Distinguish boundary, exterior and hole interior; whole-leg safety |
| Slab intersection and hole-aware scanline intervals | Clip the region by finite event bounds; return every piece with ownership |

Current inset uses integer millimetres (`CoordinateScalePerM=1000`), miter joins, and rejects multiple output
paths. Keep this API unchanged for P1. P2 region-set operations need explicit winding, deterministic ring start,
hole/component sorting, range checks, and post-quantization validity checks. Current `toClipperPath` normalizes
every input ring to positive winding; it cannot simply be applied to holes in a NonZero region representation.

Use strict input validation before booleans; do not auto-repair bad input. Offset-induced merging is derived
geometry, not input repair. Round buffers need an explicit arc approximation bound compatible with area tolerance;
new obstacle inflation must conservatively preserve the required centerline clearance at corners. Do not
silently change P1's miter inset or make legal compatibility conclusions.

## 9. Event-driven slab BCD implementation plan

Proposed files: `Geometry/PolygonRegion.h/.cc` for the region operations, and
`Planning/CoverageDecomposition.h/.cc` for `CoverageCell`, `CellAdjacency`, result, and the restricted algorithm.
`Planning/BoustrophedonCoveragePlanner.h/.cc` later orchestrates it; no registration is needed until P2-10.

```text
connected TrackFeasibleRegion
 -> convert selected navigation angle to math angle once
 -> transform outer and holes into sweep frame
 -> collect all vertex Y values; sort; deterministic epsilon merge
 -> probe each open slab at its midpoint; find all free-space intervals
 -> intersect full region with slab; associate polygon pieces to intervals
 -> compare limiting connectivity at shared event boundary
 -> continue 1:1 ownership; close/open cells at split/merge/birth/death
 -> union pieces per owner; validate and return ENU CoverageCell polygons
 -> emit deterministic cell IDs and lightweight adjacency
```

Do not infer connectivity merely from overlap of midpoint X intervals: slanted sides move between probes.
Use the common event boundary and the slab pieces. Require positive-width passage for connectivity; an isolated
point contact must not become a navigable corridor. At split/merge, record adjacency between the terminated and
new cell owners. Cell adjacency remains diagnostics/validation data and never substitutes for safe routing.

Define a deterministic event-cluster representative and avoid transitive epsilon chains collapsing a long span.
Current `intersectScanline` snaps a probe within 1 mm of a vertex and merges small intervals. A slab wider than
1 mm but narrower than 2 mm can therefore have its midpoint snapped to an event. Add a dedicated interior probe
operation for P2 or derive intervals from exact clipped pieces; do not change P1 scanline behavior opportunistically.
Reject numerically unresolved topology rather than inventing connectivity or silently deleting required cells.

P2-04 checks finite/simple/non-degenerate/connected/hole-free/monotone cells, pairwise interior non-overlap,
union approximately equal to TrackFeasibleRegion, and deterministic IDs/adjacency. No fixed Y resolution,
handwritten general boundary-tracing engine, topology framework, or separate graph subsystem is required.

## 10. Visibility Graph / Dijkstra readiness

Proposed `Planning/StaticSafeRouter.h/.cc` owns a small per-planning-invocation base graph and `StaticRoute`.
Nodes are feasible outer/hole vertices. Edges exist only after a whole-segment predicate succeeds, with finite
Euclidean length as weight. Query-local start/goal nodes connect to visible base nodes; direct visibility returns
the direct segment. Dijkstra uses deterministic node ordering and equal-cost predecessor rules. Return explicit
`SafeTransitNotFound` on failure and validate all returned legs again. Build the base graph once per invocation.

Minimum Geometry API: region point classification/containment, region segment containment, deterministic access
to all boundary vertices, and finite Euclidean distance. No A*, grid, triangulation, NavMesh, or persistent cache.

Current `containsSegment` already splits a leg at all polygon boundary intersections and classifies every
intervening interval; extend this pattern to **all** rings. Endpoints plus one midpoint are insufficient for
concave boundaries or multiple holes. Require the whole segment inside/on the outer and outside every hole
interior. Reject crossing through a hole even if both endpoints are valid.

Use the closure of TrackFeasibleRegion: boundary endpoints, tangencies, and boundary-following edges are allowed
when no open subsegment enters forbidden space. This is necessary for visibility vertices and start/goal on a
boundary. Distinguish a tangent from a crossing by adjacent interval classification, including collinear overlap
and reflex vertices. The same policy must be used by routing and final safety validation; do not implement
`!containsPoint(hole,p)` with today's boundary-inclusive single-polygon predicate. Near-boundary tolerance must
not become permission to cross a thin obstacle. Identical start/goal can return a zero-distance query result,
but final assembly must not append a duplicate zero-length leg.

## 11. Numerical tolerance recommendations

Current `LengthEpsilonM=1e-3` is used for vertex distinctness, orientation after length scaling, minimum polygon
area after perimeter scaling, monotone chains, scanline snapping/interval merging, containment parameter merging,
lane placement, and path-length candidate ties. The area and parameter uses convert units rather than blindly
comparing square metres or unitless segment parameters to metres.

The exception is `equivalentSweepAngles`: it compares `abs(sin(deltaRadians))` with `LengthEpsilonM`.
That is dimensionally wrong and couples Auto angle candidates to coordinate resolution. Its present effective
angular threshold is `asin(0.001) * 180/pi`, approximately 0.057295789 degrees, not 0.001 degrees.
`LawnmowerCoveragePlannerTest` also uses its length comparison helper for some selected-angle checks.

Minimum evolution:

- Keep `LengthEpsilonM=0.001` and existing P1 geometric decisions during extraction.
- Introduce a separately named `AngleEpsilonDeg`; initially preserve the effective P1 threshold above, with
  modulo-180 distance and boundary tests. Do not silently tighten it to 0.001 degrees. Separate any algorithmic
  threshold change from P2-01 and require P1 equivalence evidence.
- Define a single `CoverageAreaToleranceM2(targetAreaM2)` policy:
  `max(0.01, 1e-6 * targetAreaM2)`, per P2 section 65. It is not a length epsilon squared. Use the same policy
  for final footprint differences and documented decomposition area comparisons.

P2-02 must test millimetre quantization, near-touching topology, rotated near-coincident events, offset corners,
and buffer arc error against this policy. The initial area policy is a recommendation, not a backend accuracy
claim. Never loosen different local tolerances until a failing geometry appears to pass.

## 12. Persistence and MissionAdapter impact

Current ComplexItem version 1 saves `generatedPath`, status, message, path length, selected angle and turn count
at the item level, together with `taskId`. Loading restores those fields and invokes `_applyPlanningResult`,
not a planner. The top-level `marine.version=1` and task codec version 1 remain unchanged.

P2-12 adds artifact v2 roles and needed metrics, validates enum values/cardinality/finite metrics, and preserves
the sole generatedPath coordinate array. Accept v1 without replanning or changing any waypoint. The minimal
two-enum compatibility policy permitted by the spec is all-Coverage metadata for legacy paths, with no claim
that this proves P2 completeness. Never infer alternating roles for arbitrary Mock/legacy paths. New v2 output
must retain exact generated roles; do not backfill malformed v2 arrays as if they were v1.

P2-01 should keep the v1 writer/version unchanged and add only a narrow legacy role fallback on load if strict
in-memory cardinality is enforced there. Exact role persistence and new metrics are P2-12 work; until then,
save/load guarantees P1 path/mission compatibility, not lossless restoration of newly generated role semantics.
Document and test this staging explicitly. Do not introduce an artifact migration framework.

`MarinePlanContext::updateTask` emits `taskChanged`; ComplexItem synchronizes presentation and invalidates the
whole result. Future No-Go editing must go through this path. No-Go already exists in task JSON and read-only
map display; no new task schema or digest is needed.

`appendMissionItems()` hands the existing result directly to `ArduPilotMissionAdapter::appendWaypoints`.
The adapter checks status/path/coordinates/sequence bounds, then emits one `MAV_CMD_NAV_WAYPOINT` per canonical
point in the same order, with global-relative-alt frame, surface altitude 0, existing zero parameters,
unchanged yaw and auto-continue semantics. It does not plan. **Do not refactor this adapter in P2-01.**

## 13. Build, test and lint baseline

Platform: Windows `10.0.26200`, MSVC `19.51.36256.0`, Qt `6.11.1`, CMake `3.30.5`, Ninja.
The production/test trees (`custom/src`, `custom/test`, `src`, `test`) have no diff from the P1 validated software
candidate. Build caches resolve to this repository, not another checkout.

Build commands from the repository root (PowerShell):

```powershell
& 'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/Tools/Launch-VsDevShell.ps1' `
    -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
& C:/Qt/Tools/CMake_64/bin/cmake.exe --build build/P0-01-marine-debug --parallel 4
& C:/Qt/Tools/CMake_64/bin/cmake.exe --build build/P1-15A-marine-release --parallel 4
```

| Build | Current result |
| --- | --- |
| Debug custom, `QGC_BUILD_TESTING=ON` | PASS, rebuilt and linked at audited source HEAD |
| Production Release custom, `QGC_BUILD_TESTING=OFF` | PASS, rebuilt and linked; final verification reports no work |

Debug's first invocation failed in AUTOMOC for upstream `ServoOutputMonitorController.h` with no diagnostic.
An unchanged retry passed AUTOMOC, compilation and linking. No source fix or warning suppression was applied;
the cause of that first transient failure is not established. Configure retained non-blocking no-compiler-cache,
cached ArduPilot parameter origin unavailable, and optional Gettext/LibUSB warnings. CMake successfully refreshed
its Python generator environment. Release was run in the Visual Studio developer environment from the outset.

Initial CTest command:

```powershell
& C:/Qt/Tools/CMake_64/bin/ctest.exe --test-dir build/P0-01-marine-debug `
    --output-on-failure --parallel 1 -R $auditRegex `
    --output-junit F:/Projects/qgroundcontrol/build/P2-00-audit/baseline.xml
```

`$auditRegex` is anchored to the 22 classes listed below (the 15 Marine classes plus the seven requested QGC
classes). Qt was added to PATH; CTest sets offscreen/software rendering and per-test temporary directories.
APPDATA/LOCALAPPDATA were redirected under `build/P2-00-audit/profile`. This initial sandbox run took 293.25 s:
19 classes passed and three controller classes failed. It is not reported as a fully passing baseline.

All 15 Marine classes passed with zero class failures:

```text
CustomPluginIntegrationTest        CoverageInspectionPlanCreatorTest
ArduPilotMissionAdapterTest        CoverageComplexItemTest
CoveragePlannerTest                CoverageProblemValidatorTest
CoverageTaskAdapterTest            GeoReferenceTest
GeometryTypesTest                  LawnmowerCoveragePlannerTest
MarineGeometryTest                MarinePlanContextTest
MarinePlanIntegrationTest          MarineTaskModelTest
MarineTaskJsonTest
```

`MarinePlanIntegrationTest` retains the P0 task -> mock plan -> mission -> .plan -> new context -> reload chain.
`CoverageComplexItemTest` covers P1 Lawnmower integration, invalidation, QML properties and artifact restoration.

| QGC class | Initial CTest result | Writable-profile verification |
| --- | --- | --- |
| MissionItemTest | PASS | Not repeated |
| SimpleMissionItemTest | PASS | Not repeated |
| MissionControllerTest | Failed in the initial redirected-profile CTest run | PASS, 20/20 tests; no failures |
| MissionControllerTreeTest | Failed in the initial redirected-profile CTest run | PASS, 11/11 tests; no failures |
| SurveyComplexItemTest | PASS | Not repeated |
| StructureScanComplexItemTest | PASS | Not repeated |
| PlanMasterControllerTest | Failed in the initial redirected-profile CTest run | 69/71 pass; exactly the two historical strict-log failures |

Controller diagnostics use the same Debug binary with `--unittest:<class> --allow-multiple
--unittest-output:<absolute-prefix>.xml`, offscreen/software rendering and separate writable APPDATA,
LOCALAPPDATA and TEMP directories. `Start-Process -WindowStyle Hidden -Wait -PassThru` ensures serial execution
of the Windows GUI binary. An earlier asynchronous launch was stopped and is excluded from the baseline.

Historical expected PlanMaster failures, to compare with the current XML:

- `_testActiveVehicleChanged`: localized application message versus ignored English message.
- `_testFailedLoadClearsFileAssociation`: expected `Error loading Plan file` message not captured.

The writable-profile XML confirms those exact two failures and no others. The first failure reports localized
application text where the test expects the ignored English message; the second reports that the expected English
`Error loading Plan file` pattern was not captured while a localized message was emitted. These are unchanged
upstream strict-log expectations, not Marine regressions. MissionItem, SimpleMissionItem, SurveyComplexItem and
StructureScanComplexItem also passed in the initial run.

Lint/check status:

| Check | Current result |
| --- | --- |
| `python -m pre_commit run --all-files` | INCOMPLETE: hook environment setup did not reach hook verdicts |
| Pinned `markdownlint-cli@0.49.1` on the new document | PASS using installed Node and `npm exec` |
| `check-merge-conflict`, `check-added-large-files`, `trailing-whitespace`, `end-of-file-fixer`, `mixed-line-ending` | PASS on the new document |
| UTF-8, LF, final newline, trailing whitespace and `git diff --check` | PASS |

The full pre-commit command first failed fetching GitHub through the sandbox. With approved network access,
repository setup advanced but the TruffleHog Go installation stalled; the audit stopped that installation.
Cleanup then reported Windows error 145 (non-empty directory). No full-hook pass is claimed. The standalone
pre-commit Markdown hook also failed downloading Node with TLS unexpected EOF. Using the installed Node runtime
and the same pinned Markdown linter succeeded. These are tooling limitations, not waived lint requirements.
No tracked lint configuration, hook version, production source or test expectation was changed to bypass them.

Local evidence is retained in ignored `build/P2-00-audit/`: `debug-build.log`, `release-build.log`, `ctest.log`,
`baseline.xml`, `serial-*.xml`, `pre-commit.log`, `markdownlint.log`, and `markdownlint-direct.log`.
These are machine-local evidence files; the only committed artifact is this audit document.

## 14. Known risks and review conditions

1. The primitive's extra lane-schedule input is needed to preserve current P1 nominal/safe lane placement.
   Approve this concrete signature adaptation; do not replace it with double inset or duplicated coverage code.
2. Role cardinality must coexist with legacy artifact v1 during P2-01. Review the explicit compatibility/staging
   policy before enforcing the invariant at every consumer.
3. Removing the shared No-Go gate also affects Mock Planner. Preserve its direct rejection test in P2-02.
4. Backend capabilities exist, but Marine hole-aware booleans/containment/components are not implemented or
   validated. Quantization, winding, tangencies, narrow events, and arc error are P2-02/P2-04 gates.
5. Sweep monotonicity does not guarantee safe direct connectors or global nominal completeness. Preserve explicit
   failure; do not add repair passes or change global angle automatically after a failed P2 plan.
6. Visibility graph construction is quadratic in boundary vertices; repeated predicate validation can add cost.
   Keep graph lifetime local and benchmark representative finite inputs before considering extra abstractions.
7. Two historical upstream strict-log failures must be tracked separately from any new regression. Field debt
   remains open and must not be described as accepted real-USV operation.

## 15. Recommended P2-01 file list and scope

Paths below are relative to the repository. This is a proposal, not implemented by P2-00.

| Files | Purpose |
| --- | --- |
| `custom/src/Marine/Planning/PathLegRole.h` (new) | Two-role enum shared by local/geographic results |
| `custom/src/Marine/Planning/MonotoneCoverage.h/.cc` (new) | Pure fixed-angle coverage core and lane schedule/result contract |
| `custom/src/Marine/Planning/CoveragePlanningProblem.h` | Solution roles, retain current definition location |
| `custom/src/Marine/Planning/PlanningResult.h` | Geographic result roles |
| `custom/src/Marine/Planning/LawnmowerCoveragePlanner.cc` | Delegate core generation while retaining P1 orchestration/nominal target checks |
| `custom/src/Marine/Planning/MockCoveragePlanner.cc` | Explicit synthetic output role policy |
| `custom/src/Marine/Planning/CoverageTaskAdapter.cc` | Result metadata mapping only; no No-Go conversion yet |
| `custom/src/MissionManager/CoverageInspectionComplexItem.cc` | Only narrow legacy load fallback if needed; no v2 persistence or algorithm |
| `custom/test/Marine/MonotoneCoverageTest.h/.cc` (new) | Core contract, lane/connectors and differential P1 equivalence |
| Existing `LawnmowerCoveragePlannerTest`, `CoverageTaskAdapterTest`, `CoveragePlannerTest`, `CoverageComplexItemTest`, `MarinePlanIntegrationTest` | Role invariants, errors, invalidation, legacy load and unchanged path/mission |
| `custom/CMakeLists.txt` | Register new sources and focused test using existing pattern |

Run unchanged `ArduPilotMissionAdapterTest` and all required P1/QGC regressions. No production MissionAdapter,
Geometry region types, Validator gate migration, No-Go conversion, MapVisual styling, BCD, routing, ordering,
editor, or completeness work belongs in P2-01. Keep numerical threshold changes separate from extraction.

## 16. Final verdict

**Readiness status:** CONDITIONAL

The branch, P1 frozen behavior, Marine architecture boundaries, Debug/Release builds, all Marine tests, and the
requested QGC regression suites are ready for P2-01 planning. No implementation blocker was found in the current
P1 code. The conditions for proceeding are:

1. Project lead reviews and accepts the lane-schedule adaptation required to extract `MonotoneCoverage` without
   double inset or a P1 nominal-coverage regression.
2. Project lead reviews and accepts the explicit P1 artifact-v1 role fallback/staging policy before role
   cardinality is enforced across all consumers.
3. Full `pre-commit run --all-files` is rerun in an environment where the pinned hook environments install and
   completes successfully. This audit could run only document-specific hooks because Go/Node hook environment
   setup failed (network/TLS and Windows cleanup errors); no hook verdict was bypassed.
4. The two PlanMasterController strict-log failures remain recorded as upstream baseline and are not changed by
   Marine work. P1 real-USV validation remains deferred and is a required gate before P2 real-USV acceptance.

Subject to those conditions, the recommended next package is P2-01 with the file list and scope in section 15.
The P2-00 audit document is complete; do not begin P2-01 in this turn. The requested Git commit remains the
final packaging step once `.git` write access is available.
