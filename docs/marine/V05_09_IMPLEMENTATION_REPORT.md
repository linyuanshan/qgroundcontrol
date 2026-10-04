# V05-09 R2 implementation verification

Current status (2026-10-04): **V05-09 — READY FOR INDEPENDENT REVIEW**.
This is an implementer handoff, **not independent approval or approval for commit**. The owner's R2 scope
was F1 coordinate provenance/root-cause correction and F3 concrete reproduction against V05-08 commit
`4a948c8842a915fe3125322ec9eec9e66a007fb4`. The rejected height-profile setting proposal remains rejected and absent.
No staging, commit, push or V05-10 work occurred. Fresh independent implementation review must determine
finding closure and package acceptance; the implementer does not self-certify the final package DoD.

## R2 changes and root-cause evidence

Relative to reviewed R1, exactly three source/test files change, plus this report:

- `src/PlanView/TerrainStatus.qml`: nonzero finite viewports for empty distance and constant altitude, using
  the pre-existing empty-profile 100 m extent; stable diagnostic objectNames. The graph remains visible and
  every real series remains active. No mission coordinate, altitude, distance, canonical path or sample is clamped.
- `src/QmlControls/TerrainProfile.cc`: unknown-AMSL missing-terrain markers use the existing empty-profile
  display lower bound instead of appending NaN as the first point. Unknown domain altitude remains unknown;
  no terrain elevation is invented. Existing interior NaN interval separators remain.
- `custom/test/Marine/CoveragePlanViewUITest.cc`: actual graph visibility/size/axes/scale, geographic endpoints,
  segment/sample-distance/available-altitude/curve checks, original raw data assertions and recorded snapshots.
  Condition-based waiting allows the real queued segment rebuild after stale edits. All previous real PlanView
  upload callbacks, admission/refusal, dirty/sequence and Idle assertions remain; strict logs remain enabled.

F1 has **two demonstrated producers**. A finite original curve point `(0,50)` becomes `(NaN,+Inf)` in QtGraphs
when the empty/constant profile has degenerate axis bounds. Separately, with mission and terrain AMSL unavailable,
`_addMissingPoints` published `(0,NaN)` as the first missing-terrain marker, reaching the unconditional first-point
QPainter moveTo branch. An axis-only repair passes the Marine window suite but still reproduces old SaveAs;
the second repair is necessary to pass SaveAs. [Value-chain diagnosis and attempt history](../../build/v05-09-r2-repair/ROOT_CAUSE_AND_BASELINE.md)
links the source origins, queued Mission/terrain pipeline, public profile states and raw debugger parameters.
Private Qt frame nearest-export names are not treated as proven function identities.

Both corrections belong to shared QGC terrain rendering, which provides no custom/Marine extension for its viewport
or missing-marker producer. This is a narrow additional core repair authorized by R2's production-root-cause scope;
no new architecture is introduced. Domain algorithms, geometry, safety, repair, H/P/E, calibration, schemas,
semantic identities, codecs, translations, upstream regression fixtures and both frozen V05-09 design files are unchanged.

F2 remains closed by the prior independent R1 review. Its presenter fixtures/source remain unchanged in R2 and
the final CoveragePresentationTest rerun remains 16/0/0/0; no self-issued review closure is claimed for F1/F3.

## F3 baseline reproduction

The baseline is a separate full build of **all 5246 exact Git blobs** from the specified commit, not a copy of
the dirty worktree or an unattributed backup executable. The post-build source audit has zero drift.
`build-baseline.cmd` uses the same VS 18 / MSVC 14.51.36231 x64 / Qt 6.11.1 / Debug / Ninja configuration,
test/QML flags, GStreamer installation and pinned dependency source directories. Core build flags are compared
in the machine evidence. All 474 generated MAVLink headers match byte-for-byte. Both programs use the same
installed Qt debug DLLs and the final recorders prove equal PATH, language, QPA/backend and logging settings.
No English-language fixture, chart disabling, log suppression, failure expectation or new skip is introduced.

| Default-runtime case | Specified baseline | Final V05-09 | Attribution |
| --- | --- | --- | --- |
| MissionManagerTest `_testErrorAckFailureStrings` | Same strict QString::arg missing-argument failure | Same raw FAIL | Pre-existing, exact case/failure body reproduced |
| PlanViewUITest `_testPlanViewStates` | Same localized text vs `Return` failure | Same raw FAIL | Pre-existing, exact case/failure body reproduced |
| PlanViewUITest `_testRoverWaypointOnEmptyPlan` | Same localized text vs `Return` failure | Same raw FAIL | Pre-existing, exact case/failure body reproduced |
| PlanViewUITest `_testSaveAsMenu` | Strict QPainter failure | PASS, no painter warning | Root cause repaired in production |
| MarineSITLValidationTest three opt-in slots | Same 3 existing skips | Same 3 existing skips | Existing opt-in regression fixture; no actual SITL run |

[Final baseline focused records](../../build/v05-09-r2-repair/baseline-final-02-results.json) and
[final baseline actual-window records](../../build/v05-09-r2-repair/baseline-ui-final-02-results.json) retain raw
XML/stdout/stderr, separate executable hashes, commands and environment. All remaining failure and skip bodies
are matched case-by-case in the updated machine evidence. Raw failures are retained as FAIL, not relabeled PASS.
Baseline executable SHA-256: `2b387f3d44563dcc086b95e1fa36146808aca1e2560dfda764b6b4c9a13373ff`.

## Final build and validation

Windows build under VsDevCmd x64:
`C:/Qt/Tools/CMake_64/bin/cmake.exe --build build/P0-01-marine-debug --target QGroundControl --parallel 8`.
[Final build](../../build/v05-09-r2-repair/build-final-05-results.json): exit 0.
Final executable SHA-256: `b36f6e1b73aa2441097f512e55e9da3c895f3ac5e8aff1b6a9ef16a56efa6296`. Every final suite below ran against that one unchanged binary.
R2 changes no production header/member layout; incremental compilation refreshes the changed terrain/test TU
and QML resources without repeating the old header-dependency ABI issue.

[Final 61-suite focused/Marine/P0/P1/QGC matrix](../../build/v05-09-r2-repair/regression-final-02-results.json) and
[final actual windows](../../build/v05-09-r2-repair/ui-final-02-results.json): **63 registered suites, 837 XML cases,
3 failures, 0 errors, 3 skips**. There are 60 fully clean suites, two raw-failing suites
with the three concretely reproduced pre-existing localization failures, and one existing opt-in suite with three skips.
Focused suites use offscreen/software; actual windows use Windows/D3D11. No unregistered name is counted as executed.

| Required focused / affected suite | Final XML tests / failures / errors / skips |
| --- | --- |
| CoveragePresentationTest | 16 / 0 / 0 / 0 |
| CoverageComplexItemTest | 26 / 0 / 0 / 0 |
| IntegratedPlanningResultTest | 34 / 0 / 0 / 0 |
| MarineUploadGateTest | 24 / 0 / 0 / 0 |
| MarinePlanIntegrationTest | 3 / 0 / 0 / 0 |
| ArduPilotMissionAdapterTest | 6 / 0 / 0 / 0 |
| CustomPluginIntegrationTest | 12 / 0 / 0 / 0 |
| MarineTaskJsonTest | 60 / 0 / 0 / 0 |
| MarineTaskModelTest | 8 / 0 / 0 / 0 |
| PlanningInputIdentityTest | 9 / 0 / 0 / 0 |
| CoverageTaskAdapterTest | 13 / 0 / 0 / 0 |
| CoverageRepairTest | 12 / 0 / 0 / 0 |
| MissionControllerTest | 20 / 0 / 0 / 0 |
| PlanMasterControllerTest | 71 / 0 / 0 / 0 |
| MissionManagerTest | 7 / 1 / 0 / 0 |
| CoveragePlanViewUITest | 6 / 0 / 0 / 0 |
| PlanViewUITest | 6 / 2 / 0 / 0 |
| MarineSITLValidationTest | 5 / 0 / 0 / 3 |

The actual-window XML has zero QPainterPath or invalid-context warnings. Height-profile snapshots retain zero
Mission distance / 50 m ordinary altitude and zero-altitude Marine data while axes have positive finite ranges.
[Final public chart/sample snapshots](../../build/v05-09-r2-repair/ui-final-02-profile.jsonl) bind those checks to
the final binary. These are automated real-window observations, not human/manual acceptance. Previous residual
hole/palette captures and F2 presentation evidence remain source-bound historical observations; they were not
misrepresented as newly captured final-binary images.

[Core/source checks](../../build/v05-09-r2-repair/checks.json): five QML lint commands, QML formatter execution,
changed terrain C++ region formatting, both repository analyzers and diff check exit 0. The four final test-line
format corrections pass the final clang-format check. Qt/QML and legacy clang-tidy warnings remain preserved.
[TerrainProfile clang-tidy](../../build/v05-09-r2-repair/tidy-TerrainProfile-03.log), using the active compile database,
VsDevCmd and the established /Y- override, exits 0. Two earlier wrapper PATH-detection failures are retained;
they are not counted as analyzer executions. Unchanged original production TUs retain their prior independently
audited source-bound checks, not fictional new runs. Available clang-format is 22.1.8 versus hook pin 23.1.
Clazy is **supplemental static-analysis SKIP** because unavailable; Markdownlint/Vale/Typos remain NOT EXECUTED.

The ordinary-cache [pre-commit attempt](../../build/v05-09-r2-repair/precommit.log) exits 1 at
store.mark_config_used with readonly SQLite **before hooks**: **ENVIRONMENT BLOCKED**, not PASS.
No alternate cache, cache repair or hook waiver was used. Final source/report diff checks and source identities
are recorded separately. Initial diagnostic/build/fixture/format failures and one permission-review timeout
before launching a test remain recorded in the attempt history; none replaces final verification.

## Stop gate

The authorized R2 F1/F3 work is concrete and reviewable. No newly introduced regression failure remains;
the three remaining raw localization failures have exact runtime baseline reproduction. Final package DoD,
finding closure and commit approval await the fresh independent implementation reviewer; no self-approval.
HEAD remains the specified V05-08 commit, staged=0, and V05-09 remains uncommitted. The next action is that
fresh review. Recommended next implementation package is V05-10 only after V05-09 closure and separate owner
authorization; it is not started. Current v0.5 SITL, manual/freeze and field validation are NOT EXECUTED.
P1 real-USV field validation remains DEFERRED / NOT EXECUTED and is not certified by these software checks.

## Historical R1 report (superseded current state; preserved evidence)

The following R1 text records its original reviewed state. Its then-pending chart proposal is resolved by the
owner's R2 rejection above. Historical binary/check claims remain bound to their own snapshots and are not
current R2 execution claims.

# V05-09 R1 implementation verification update

Current status (2026-10-04): **R1 PARTIAL REPAIR; VALIDATION FAIL; DoD NOT MET; NOT APPROVED FOR COMMIT.**
The original independent implementation review returned [REQUEST_CHANGES](../../build/v05-09-independent-review/REVIEW.md).
The [fresh independent R1 review](../../build/v05-09-r1-independent-review/REVIEW.md) returns **REQUEST_CHANGES**: F2 CLOSED, F1/F3 OPEN. No stage, commit, push or V05-10 work occurred.
This update governs current state; the complete prior implementation report below is historical and remains bound to
its original binary and source snapshot. It is not a claim that old checks ran again against the R1 binary.

R1 changes four source/test files: CoveragePresentationTest.cc/.h, CoverageInspectionEditor.qml, and
CoveragePlanViewUITest.cc. The complete worktree remains 25 files. Geometry, safety, planning, repair algorithm,
calibration, comparator, codec/schema/semantic identities, governance/specification/frozen design, translations,
existing MissionManager/PlanView test fixtures and terrain chart source remain unchanged relative to the approved HEAD.

## R1 findings and exact repair state

**F2 / R10 / T09-08:** CLOSED by fresh independent review and independent 16/0/0/0 presenter rerun. This is finding closure, not package commit approval.
Two supported real stored-result rows cover AppliedPolicyPass and attempted=true/applied=false NoUsefulRepair.
The latter repeats the real repair operation on the retained real Insufficient candidate, asserts no selected steps and
unchanged canonical path/roles/metrics, publishes that actual operation, and passes existing Artifact save/load validation.
It does not claim that the one-shot Auto planner produced this second-operation presentation fixture.
All selected components, entry/direction/cost/path/length/turn fields and all 20 before/after quality fields with
per-field availability/null/component/hole semantics are compared to the actual stored Artifact. Actual editor summary
and component labels have two stable objectNames and exact visible-text assertions. Full quality maps are exposed;
the component label displays the existing uncovered-area summary, not every quality scalar.
CoveragePresentationTest is **16/0/0/0**. The first 16/1 failure was a visible-label numeric-format oracle mismatch;
its raw XML remains and all exact numeric facts remain asserted. Expected label formatting now matches the product's
existing QString.arg(double) formatting. [Source handoff](../../build/v05-09-r1-repair/F2_REPAIR_NOTES.md).

**F1 / U5 / T09-03,15,16:** partly repaired; the actual-window gate remains **FAIL**.
The new public diagnostic walked both QObject and visual-child trees. At model replacement, actual Simple/Marine map
Loader and QQmlComponent status were still Loading while incubation count was zero. Installed DLL export-thunk/runtime
analysis resolves the actual async completion/statusChanged/Loader-create path, rather than relying on misleading
nearest export labels. A narrow test-fixture condition now waits for actual Loader non-Loading AND incubation completion
before model replacement and further UI actions, using public APIs and TestTimeout::mediumMs. No fixed sleep, hidden
callback or warning expectation was added. Temporary diagnostic output was removed from the final source after preserving it.
On the final source/binary, neither invalid-context warning reoccurs, but **CoveragePlanViewUITest remains 6/3/0/0 FAIL**
from QPainterPath invalid coordinates. All existing backend/callback/admission/dirty/sequence/Idle assertions remain.

The separate [read-only painter stack analysis](../../build/v05-09-r1-repair/painter-pe-readonly.md) exactly resolves
QPainterPath.moveTo and QGraphsView.updatePolish. Public snapshots in a separate run show the TerrainStatus chart's
X 0..0 and Y 50..50 / Marine 0..0 axes. Unchanged TerrainStatus/TerrainProfile source permits zero ranges and NaN
separators. These facts identify the chart path; the exact private coordinate producer and whether zero axes or NaN
separators cause the warning are **not proved**. No terrain/Qt production repair is claimed.

**F3 / U7 / T09-16:** remains **FAIL**. Current MissionManagerTest is 7/1, with the same zh-CN missing %1 warning;
PlanViewUITest is 6/3, with two English Return assertions against localized text and one SaveAs QPainter warning.
The relevant existing source/tests/translations are unchanged. Source equality is not a historical runtime baseline proof.
The default-language/default-chart runs are neither PASS nor waived. No English-only fixture or translation change was made.

Automatic approval review rejected a proposed focal-test showMissionItemStatus=false setting change because it could
hide a real regression and conflict with the owner's prohibition on warning suppression. That rejected patch was not
written or executed through another route. The owner clarification is still pending; continuation alone is not interpreted
as approval of this specific rejected action. No result or commit gate is waived by the proposal.

## R1 commands, identity and validation

Canonical Windows build: existing VsDevCmd x64 +
`C:/Qt/Tools/CMake_64/bin/cmake.exe --build build/P0-01-marine-debug --target QGroundControl --parallel 8`.
[Final incremental build](../../build/v05-09-r1-repair/build-r1-final-01.log) exits 0 and recompiles the changed test body.
The initial R1 build recompiles the changed test-slot MOC and QML resources. No production header/layout changed after
the previously verified full application-object refresh. Current executable SHA-256: **100bedca470b38dc565e3915481e4fbc3d4e83de3b09bb30e0807c7862b430f9**.

The serial command records and per-run raw XML/stdout/stderr/hash are in
[focused](../../build/v05-09-r1-repair/focused-r1-final-01-results.json),
[schema/repair/Mission regressions](../../build/v05-09-r1-repair/regression-r1-final-01-results.json), and
[actual window regressions](../../build/v05-09-r1-repair/ui-r1-final-01-results.json).
Actual runner form is `python build/v05-09-r1-repair/run-r1.py <unique-stage> focused|ui <registered suite...>`.
All current runs use the same unchanged binary. Focused tests use offscreen/software; actual windows use Windows/D3D11.
Strict logs stay enabled. These are automated tests, not manual acceptance.

| Current registered suite | Tests / failures | Classification |
| --- | --- | --- |
| CoveragePresentationTest | 16 / 0 | PASS |
| CoverageComplexItemTest | 26 / 0 | PASS |
| IntegratedPlanningResultTest | 34 / 0 | PASS |
| MarineUploadGateTest | 24 / 0 | PASS |
| MarinePlanIntegrationTest | 3 / 0 | PASS |
| ArduPilotMissionAdapterTest | 6 / 0 | PASS |
| CustomPluginIntegrationTest | 12 / 0 | PASS |
| MarineTaskJsonTest | 60 / 0 | PASS |
| MarineTaskModelTest | 8 / 0 | PASS |
| PlanningInputIdentityTest | 9 / 0 | PASS |
| CoverageTaskAdapterTest | 13 / 0 | PASS |
| CoverageRepairTest | 12 / 0 | PASS |
| MissionControllerTest | 20 / 0 | PASS |
| PlanMasterControllerTest | 71 / 0 | PASS |
| MissionManagerTest | 7 / 1 | FAIL |
| CoveragePlanViewUITest | 6 / 3 | FAIL |
| PlanViewUITest | 6 / 3 | FAIL |

There are **17 registered suites: 14 PASS, 3 FAIL; 333 cases, 7 failures, 0 errors, 0 skips**.
The 14 clean suites contain 314 cases. Two attempts used unregistered names MarineTaskPersistenceTest and
PlanningArtifactCodecTest and were rejected with no XML: **NOT EXECUTED**, not PASS. The actual registered MarineTaskJsonTest,
MarineTaskModelTest and PlanningInputIdentityTest were subsequently executed as shown. The prior 61-suite run is historical;
it is not described as a fresh R1 run.

[R1 static/source checks](../../build/v05-09-r1-repair/r1-checks.json): test C++ format, custom editor QML format,
all four affected QML lint commands, both nine-file repository analyzers and diff check exit 0. QML lint warnings remain.
Available clang-format is 22.1.8, differing from hook pin 23.1. Prior independently audited production clang-tidy checks
remain source-bound because all production C++ TUs are unchanged in R1; they were not rerun or relabeled as current executions.
Clazy is supplemental SKIP; unavailable Markdownlint/Vale/Typos are NOT EXECUTED.

[Current pre-commit attempt](../../build/v05-09-r1-repair/precommit-r1-normal-01.log) again fails at store.mark_config_used
with readonly SQLite before hooks: **ENVIRONMENT BLOCKED**, not PASS. No alternate cache or cache repair was used.
This environmental condition is separate from the three genuine failing suites, and does not permit committing them.

The current [automated render manifest](../../build/v05-09-product-render-r1-final-01/manifest.json) binds the two palettes,
product QML/source, current successful presenter suite XML and binary. The actual hole pixel/component/label oracles pass;
implementer inspected both captures. It is not independent image approval or human/manual acceptance.

## Current stop gate

HEAD is 4a948c8842a915fe3125322ec9eec9e66a007fb4, branch feature/marine-p2-complex-coverage, staged=0.
V05-08 remains locally CLOSED at that commit with its preserved fresh R2 approval. V05-09 remains uncommitted.
Fresh independent review closes F2 and retains F1/F3; these failures still block package DoD and commit approval.
The specific rejected chart-setting action awaits owner decision. V05-10, current v0.5 SITL, manual and field validation
are NOT EXECUTED at this dependency gate. Field acceptance remains deferred/not certified. No push occurred.

Post-review metadata update only: reviewed report/evidence bytes remain in the immutable R1 snapshot. No source/test/binary changed after review. The review binds those inputs; this update records its verdict.

## Historical pre-R1 implementation report (immutable snapshot preserved)


Status: IMPLEMENTED FOR REVIEW; VALIDATION FAIL; DoD NOT MET. Independent implementation review is pending. No commit, stage, push, V05-10 implementation, current v0.5 SITL, manual acceptance, or field acceptance is claimed.

The owner attachment §§I/J/K authorizes implementation of the independently reviewed frozen V05-09-DC-v1. The newer owner authorization supersedes the stale package routing table in AGENTS.md. Required repository guides, active v0.5 specification, actual V05-08 APIs, QGC editor/map/save/upload code, and actual Qt 6.11.1 headers were read before production edits. The two frozen design documents remain byte-for-byte unchanged from the reviewed snapshots.

## Implementation and scope

The changed-file hashes, raw test results, checks, historical attempts, and review inputs are recorded in [implementation evidence](../../build/v05-09-evidence.json). The reviewed authority is [design contract](V05_09_DESIGN_CONTRACT.md) and [technical design](V05_09_TECHNICAL_DESIGN.md).

- `custom/src/MissionManager/CoverageInspectionComplexItem.h/.cc`: independent CoverageArea and NavigationArea editing, selectable No-Go editing, explicit H/P/E and requirement properties, synchronous stale invalidation, presentation exposure, and exact generic checked-upload overrides. H no longer silently raises P. Invalid explicit values remain available to backend validation. Task name/sensors retain their noncritical behavior. Pending C edits survive deferred N Task updates. Coordinate signals on polygon path-model objects immediately revoke admission and current executable/visual getters, before deferred polygon path signals; exact existing C/N/O-to-Task comparisons also guard admission. No planner or geometry algorithm moved into the adapter.
- New `CoveragePlanningPresentation.h/.cc`: stateless structured exposure of stored status/readiness/D0-D3/tier, canonical and diagnostic role/safety runs, availability-aware quality, all residual components and holes, issues/suggestions with stable codes and localized source templates, typed references, repair provenance/before/after/costs, actual PlannerSourceInfo fields, stale state, and IngressNotAssessed. Public QGeoPolygon hole support marshals stored GeoPolygon data into QVariant geoShape. Canonical and diagnostic namespaces remain separate.
- `custom/src/PlanView/CoverageInspectionEditor.qml`: actual QGC controls for independent C/N/O and explicit Task-authorized fields. Pending critical numeric text revokes current admission; committing invalid text does not manufacture a valid default. Generate is explicit. Read-only result presentation uses applicable typed reference fields and preserves tiny nonzero metrics through number string formatting. No QML admission, evaluator, repair, or planning logic was added.
- `custom/src/PlanView/CoverageInspectionMapVisual.qml`: canonical/diagnostic role and safety runs, noncolor labels/glyphs/line widths, distinct residual categories, and real MapPolygon.geoShape holes/components. Screen-space label placement avoids overlapping category names and changes no domain geometry. Diagnostic data is labeled non-executable. Existing dynamic map-item lifecycle conventions are retained.
- `custom/test/Marine/CoverageComplexItemTest.h/.cc`: independent H/P validation expectations and actual QML instantiation/control checks replace the former source-only registration oracle.
- New `CoveragePresentationTest.h/.cc`, `CoveragePlanViewUITest.h/.cc`, and `MarineUploadGateTest.h/.cc`: stored-result/editor/map/toolbar tests, actual PlanView callback tests, and connected MockLink whole-plan upload/loader/conversion tests. `custom/CMakeLists.txt` integrates the presenter and tests. Only scoped CMake formatting was performed.

The necessary generic core extension is limited to `VisualMissionItem.h`, `MissionController.h/.cc`, `PlanMasterController.h/.cc`, and `PlanView.qml` / `PlanToolBarIndicators.qml`. There is no Marine include in core. The old void append interface could refuse one Marine item while the static sender continued writing the remaining Mission; the old master started its send sequence before admission; the file sender could not distinguish loader refusal from a valid empty plan. None of those paths exposed a custom-build extension capable of enforcing whole-plan atomic refusal.

The generic extension therefore adds `readyForUpload(QString&)`, checked append, and readiness notification to VisualMissionItem, with ordinary-item defaults preserving existing behavior. MissionController exposes aggregate backend truth and guards all direct/static conversion before writing, using a temporary owner to discard a failed conversion atomically. PlanMasterController checks before starting its sequence and propagates actual existing text/JSON/plugin/Task/Artifact/Mission/fence/rally loader success through a private checked loader without duplicating parsers. Static file upload releases its transient controller on failure and sends only after true load success. A successfully loaded ordinary empty plan still intentionally clears Mission. PlanView and toolbar bind to backend truth and recheck real callback paths. Save/KML conversion remains separate, so ReviewRequired may save and retain its hard-safe canonical route while upload is refused.

There was no owner semantics decision or broader architecture redesign. Domain planning, geometry, safety, repair, comparator, calibration, Task/Artifact schemas/codecs, semantic identities, specification/governance, MissionAdapter, and translations remain unchanged. The new production paths contain no planning/evaluation/repair algorithm and no QML certification or upload truth.

## Build and binary integrity

Windows primary build: Visual Studio BuildTools 18, MSVC 14.51.36231, Qt 6.11.1, Debug. The command ran under VsDevCmd `-arch=x64`:

```text
C:/Qt/Tools/CMake_64/bin/cmake.exe --build build/P0-01-marine-debug --target QGroundControl --parallel 8
```

Final build [build22 log](../../build/v05-09-validation/build22-final-newlines.log) / [result](../../build/v05-09-validation/build22-final-newlines.json): PASS. Final executable SHA-256:

```text
d82600bc49e4824e60e4ed127e01525bc92b7555515ac2ba53433be6fff3917f
```

Local Ninja recorded zero MSVC header dependencies for existing application objects. Earlier incremental attempts consequently produced an ABI mismatch; those failed attempts are preserved and are not validation evidence. Before final validation, [full application-object refresh](../../build/v05-09-validation/final-app-object-refresh-02.json) verified the confined build paths and refreshed 1235 disposable application `.obj` files, retaining previous binaries and source/library/cache/history files. Build17 completed that fresh compile. Subsequent changes added only UI test method bodies/slots, not production header layouts; build18 explicitly rebuilt the MOC compilation object. Builds19-21 updated test bodies. The final base-file check found mixed line endings in three changed sources; [newline-only normalization](../../build/v05-09-validation/newline-normalization-final-01.json) retained before/after hashes and changed no semantic text. Build22 recompiled CItem and Qt resources, and every final suite below ran again against its one final binary. No source edit followed this build except this report.

## Actual validation

The serial runner uses separate raw stdout/stderr and QTest XML for each suite, records child environment and binary hash, and permits only authorized ordinary Qt cache writes. Strict warnings remain enabled. Actual window tests use Windows QPA/D3D11; domain/editor/map tests use offscreen/software. Console helpers are hidden. These are automated tests, not human acceptance.

| Final run | Result |
| --- | --- |
| [61-suite regression matrix](../../build/v05-09-validation/final-stable-02-results.json) | 823 XML tests; 1 failure, 0 errors, 3 skips. 59 suites pass with nonzero tests and no failure/error/skip. |
| CoveragePresentationTest | 14 tests, 0 failures/errors/skips. Actual QML controls/models/signals, editor, toolbar, polygon holes, two palettes, and render oracles. |
| CoverageComplexItemTest | 26 tests, 0 failures/errors/skips. |
| MarineUploadGateTest | 24 tests, 0 failures/errors/skips. Real connected MockLink direct/master/static/file refusal, atomic conversion, loader refusal, accepted complete Mission/empty clear, and lifecycle notifications. |
| MissionControllerTest / PlanMasterControllerTest | Actual new-binary suites PASS, not inferred from unavailable or historical suites. |
| MarineSITLValidationTest | PARTIAL: 5 tests, 3 skipped; this is a code regression fixture. No current v0.5 SITL execution. |
| MissionManagerTest | FAIL: 7 tests, 1 failure caused by known existing zh-CN QString::arg translation missing `%1`. Relevant source/translation unchanged; this run remains a failure. |
| [CoveragePlanViewUITest actual window](../../build/v05-09-validation/ui-windows-d3d11-final-06-results.json) | FAIL: 6 tests, 3 failures, 0 errors/skips. See unresolved issue below. Ordinary global/app dialog lifecycle baseline passes. |
| [Existing PlanViewUITest actual window](../../build/v05-09-validation/old-ui-windows-final-01-results.json) | FAIL: 6 tests, 2 failures, 0 errors/skips; hardcoded English `Return` expectation sees localized text. Upstream test and translations unchanged. Raw XML encoding limitations are preserved. |

The 61-suite list includes all 21 Marine focused suites and the union of available relevant prior Mission/persistence/vehicle/component regressions, plus the new presenter/gate suites. It is not a whole-repository test run. Aggregate counts include Qt init/cleanup cases; evidence retains each real case and skip independently.

## Frozen validation matrix

| Contract | Implemented evidence and practical limit |
| --- | --- |
| T09-01 | Ready/D0 stored-result editor/model, restore without planner registry, and accepted wire Mission PASS. |
| T09-02 | Warning/D1 role/safety runs and accepted wire Mission PASS. |
| T09-03 | Review/D0 canonical/residual/issue/save/restore and connected refusal PASS. Actual window save/refusal assertions pass, but its strict-log suite FAIL remains. |
| T09-04 | Diagnostic/D2 canonical empty, diagnostic presentation, and backend refusal PASS. |
| T09-05 | D3 DiagnosticOnly and invalid InvalidInput/None exposure/refusal PASS. |
| T09-06 | AssessmentError unavailable flags, retained canonical, actual issue delegate, no below-requirement issue/repair, backend refusal PASS. |
| T09-07 | Actual product QML MapPolygon holes/all components, unchanged source geometry, rendered hole unfilled/category labels distinct for both palettes PASS. Automated render only. |
| T09-08 | Applied repair fixture, stored provenance/before/after/path/turn facts and enum/suggestion exposure PASS. Attempted-not-applied repair is not explicitly asserted by the new focused presentation test; this remaining verification gap is disclosed for review. |
| T09-09 | Synchronous critical C/N/O/draft/drag and numeric text invalidation, stale load, cleared executable getters/current visual facts, no automatic replanning PASS. |
| T09-10 | Independent C/N/O values, deferred C/N race, O immediate adjust, draft/drag, editor switching PASS. |
| T09-11 | Real editor Standard/Strict/H/P/E/Manual/Auto fields, invalid P<H, noncritical name/sensors and notifications PASS. |
| T09-12 | Every stable issue/suggestion code, localized source templates, actual issue delegates, applicable typed namespaces, source detail/order and empty lists PASS. Not every rendered scalar label's text is separately asserted. |
| T09-13 | Six actual blocked planner states, mixed ordinary plan, 10 real loader-refusal rows including Task/Artifact/precreation/text/structure/Mission/fence/rally, zero Mission/fence/rally activity, unchanged live dirty/sequences, transient destruction/Idle PASS. |
| T09-14 | Checked append late rejection after allocation, complete temporary cleanup, zero wire write/partial resend and Idle PASS. |
| T09-15 | Product toolbar binding/forced enabled forwarding real PMC PASS. Actual product PlanView upload and real captured firmware acceptance callbacks execute; stale/Review refusal, no write, dirty/sequence/Idle assertions pass. Overall actual UI suite FAIL due invalid-context warnings; this contract gate is not marked complete. |
| T09-16 | Ordinary, Ready/Warning mixed complete coordinate/order/count/alt wire comparison, home/empty intentional clear seeded with a previous nonempty Mission, end-action/lifecycle PASS. Existing actual PlanViewUI localization failures remain separately recorded. |

## Render and static checks

[Immutable final render manifest](../../build/v05-09-product-render-final-stable-02/manifest.json) binds the final binary, actual product MapVisual source, focused XML, two palettes, baseline and holes PNGs. Pixel oracles compare holes against the no-residual baseline and filled residual pixels independently; QGeoPolygon hole counts and source geometry equality are asserted before rendering. Category label rectangles are pairwise disjoint. Implementer inspection confirms unfilled hole interiors, a filled independent inner component, and readable C/N/Coverage/Boundary shortfall/Critical gap labels. This is AUTOMATED PRODUCT RENDER plus IMPLEMENTER IMAGE INSPECTION, not manual acceptance or independent review.

Required/core clang-tidy uses the repository configuration/current compile database, no fixes, for every changed production C++ translation unit: MissionController, PlanMasterController, CItem, and the new presenter. All exit zero with zero promoted core errors. Warnings remain, including pointer/bounds/style/naming diagnostics; no warning-free or all-baseline claim is made. Logs and source hashes are retained. The accidental intermediate CItem tidy invocation used a nonexistent P0-02 compile-database path; it is NOT accepted evidence. Final CItem tidy explicitly uses P0-01 and is retained separately.

Changed C++ regions pass available clang-format 22.1.8; new C++ files pass whole-file formatting. This differs from the repository hook pin 23.1 and is disclosed. New custom QML passes qmlformat comparison. Actual Qt qmllint runs on both custom QML and both affected core QML exit zero; inherited context/unqualified access and existing core warnings remain. Changed CMake passes actual cmake-format check and cmake-lint. Actual nine-file vehicle-null and translation-template analyzers pass. UTF-8, trailing whitespace, merge markers, EOF, line endings, size, frozen bytes, protected scope, local report links, unstaged index, and `git diff --check` are checked separately.

Normal pre-commit was attempted and blocked by a readonly SQLite database before any hook ran: ENV_BLOCKED, not hook PASS. No cache repair or alternate cache was used. Markdownlint, Vale, and Typos are unavailable and NOT EXECUTED; local link/base-file checks do not claim equivalence. Clazy is unavailable: supplemental SKIP per AGENTS, not a sole blocker. Frozen design prose is not reformatted without renewed review.

## Unresolved failures and stop

The actual CoveragePlanViewUITest fails strict logs in both Marine callback rows with `QQmlComponent: Cannot create a component in an invalid context`. Its non-Marine ordinary firmware confirmation control also fails with that warning plus `QQmlContext: Cannot set context object on invalid context`. All callback/refusal/cleanup/save behavioral assertions complete. The standalone ordinary global/application dialog baseline passes. Exact fixture warnings for MockLink's two unsupported initial message intervals are individually expected and verified; the invalid-context warnings are neither ignored nor expected.

Debugging preserved an actual warning stop and our `closeDialogs` wait callsite. Qt private symbols were unavailable: nearest exported names plus large DLL offsets are not evidence of particular private Qt functions or root cause. Reproduction on this current changed-core binary is not proof of a pre-existing/environment defect. Earlier software-window qnumeric fatal occurs during QmlUITestBase.startUI before Marine test bodies and also reproduces with untouched existing PlanViewUITest, but remains failed renderer-attempt evidence. Windows/D3D11 avoids that fatal; it does not waive the current strict-log failures. No speculative Qt/core workaround or warning suppression was added.

MissionManager's known translation failure and existing PlanViewUI's English/localized assertions also remain FAIL. T09-08's attempted-not-applied focused presentation assertion remains a verification gap. Required executable checks have been performed within the local environment; unavailable tools and partial/skipped fixtures are classified explicitly. The package DoD is not met. STOP for a fresh independent V05-09 implementation review, then an authorized repair loop if requested. V05-10 remains unimplemented; the owner already conditionally authorizes it only after V05-09 approval, commit, and clean-worktree closure gates. No new permission request replaces those dependencies.
