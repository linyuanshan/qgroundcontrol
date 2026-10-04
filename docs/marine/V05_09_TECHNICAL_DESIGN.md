# V05-09 Technical Design — editor and integrated result presentation

Status: **FROZEN — V05-09-DC-v1; independently reviewed and implementation-authorized.**

Frozen contract: `V05-09-DC-v1`, in [V05_09_DESIGN_CONTRACT.md](V05_09_DESIGN_CONTRACT.md).
This document does not authorize self-review or certify implementation, visual acceptance, SITL, or field readiness.

Freeze record (2026-10-03): fresh independent GPT-6.1 Sol R1 design review returned **APPROVED FOR COMMIT**;
F09-D1 is closed and no new discretionary product semantics were found. Owner §§H–I authorize implementation
of this reviewed contract. [Independent review](../../build/v05-09-design-r1-review/REVIEW.md) and
[machine evidence](../../build/v05-09-design-r1-review/review-evidence.json) record the inspected proposal and frozen hashes.
Author-stage/proposed-stage statements below describe historical workflow; implementation still requires its separate review gate.

## 1. Binding and authority

Author inspection binds to `F:\Projects\qgroundcontrol`, branch `feature/marine-p2-complex-coverage`,
HEAD `4a948c8842a915fe3125322ec9eec9e66a007fb4` (closed V05-08), with a clean worktree before authoring.
The owner's current long-horizon instruction §§H–I explicitly authorizes a separate design-author and design-review lane.
Its stage authorization supersedes older package-state text in AGENTS.md and the specification's historical header.
Those historical files are not edited by this design.

Product authority is [P2 v0.5](P2_COMPLEX_COVERAGE_SPEC_v0.5_ADAPTIVE_PLANNING_AMENDMENT.md), particularly
§§6–14, 15, 18–22, 25–31, 32–36. Approved CAL-01/CAL-02 remain `0.99`, `0.50 m`,
`coverage-quality.v1`; no calibration or planning semantic version changes are proposed.
Unaffected P0/P1 plan-scoped lifecycle, pure domain, coordinate convention, QGC extension boundaries and USV semantics remain intact.

Preflight read AGENTS.md, CODING_STYLE.md, .github/CONTRIBUTING.md, tools/README.md, test/README.md,
.github/ci-overview.md, the active specification, relevant P0/P1 architecture sections and historical P2 No-Go editor sections.
The current implementation and local Qt headers listed below were inspected before this first document edit.

## 2. Actual API audit

| Inspected source | Current fact | V05-09 consequence |
| --- | --- | --- |
| `custom/src/Marine/MarineTask.h`, `MarineTypes.h` | Task already owns independent C/N/O, Standard/Strict, Swath, H/P/E, requested planner and sweep inputs. | Expose those Task fields; do not create QML-owned copies or defaults. |
| `custom/src/Marine/QGC/MarinePlanContext.cc` | `updateTask` stores edits and emits `taskChanged`; it does not validate topology or require a complete Task. | Temporary incomplete edits remain possible; planning/schema validation stays in existing backend. |
| `custom/src/Marine/Planning/PlanningOutcome.h`, `PlanningResult.h` | One geographic outcome owns readiness, optional tier, per-leg safety, optional quality, issues, suggestions, separate diagnostics and selected repair provenance. | Marshal existing facts; do not introduce a second outcome or readiness calculator. |
| `CoverageQualityEvaluator.h` | Each metric and geometry has explicit availability; residuals are region sets with holes. | Unavailable means unavailable, not numeric zero or a successful empty region. |
| `PlannerSource.h` | Requested planner, resolved strategy/version, escalation reason, requested sweep, selected angle and sweep semantic identity are separate fields. | Show planner Auto and sweep Auto independently. |
| `PlanningArtifactCodec.h/.cc` | `uploadAllowed(artifact, currentTask, error)` checks IntegratedV05, stale/current identity, source binding, Success, readiness and structural/Task consistency. | Use this function as Marine authorization truth. Do not encode its conditions again in QML. |
| `ArduPilotMissionAdapter.h/.cc` | Artifact + current Task overload returns bool; uncertified result overload fails closed; all coordinate/sequence validation precedes append. | Checked upload append must propagate this bool; diagnostics are never a fallback. |
| `CoverageInspectionComplexItem.h/.cc` | QML only has C as `workRegionPolygon`, O editor models, H as `safetyMarginM`, sweep and old scalar/path properties. `PlanningState::Planned` means current completed attempt, including failures. | Add N, P/E, requirement and integrated-result exposure. Never equate Planned with uploadable. |
| Same adapter | `setSafetyMarginM` currently raises P when H exceeds P. | Remove this hidden cross-field mutation for explicit independent H/P editing. Invalid P < H is reported by the backend, not silently repaired. |
| Same adapter | `generatedPathRoleRuns` groups only by role. `invalidatePlan` marks artifact stale and clears current result. ReviewRequired has a canonical path and is saveable by current `readyForSaveState`. | Group visual runs by role AND safety; show stale explicitly without reinstating stale executable geometry. Keep save/upload separate. |
| `CoverageInspectionEditor.qml`, `CoverageInspectionMapVisual.qml` | Editor shows old Work Region, H and scalar state; map only renders C/O and role paths. Neutral fallback is hardcoded white. | Add required semantic presentation using QGC controls/palette; remove uncertified neutral-route fallback. |
| `src/MissionManager/VisualMissionItem.h` | Only `readyForSaveState` and void `appendMissionItems` are available. | Neither is an upload authorization/result extension point. Add narrow virtual upload hooks. |
| `MissionController.cc` | `_convertToMissionItems`' bool means end-action-added, not conversion success. `sendItemsToVehicle` ignores void append refusal, then calls `writeMissionItems`. `sendToVehicle` clears dirty unconditionally. | Separate checked upload conversion from save/KML conversion; reject the entire outgoing mission on any blocked Marine item. |
| `PlanMasterController.h/.cc` | Sets `_sendSequence = Mission` before calling the mission controller. Static file upload calls void `loadFromFile` then unconditionally sends; the loader already computes actual text/JSON success but does not return it. Plugin/Task rejection can occur before any Marine item exists, leaving ordinary/home-only data that passes item admission. | Propagate the existing checked load result before static file upload admission; gate before starting the sequence and unwind checked refusal. Never infer load success from item count, dirty state or file association. |
| `src/PlanView/PlanView.qml` | Save/upload share readiness-to-save check; firmware mismatch confirmation calls backend send directly. | Preserve save check; add backend upload gate binding and direct-call protection. Confirmation cannot bypass Marine gate. |
| `PlanToolBarIndicators.qml` | Upload enabled only by sync/items/offline conditions. | Add aggregate backend `uploadAllowed`; keep existing vehicle/sync checks. |
| `QGCCorePlugin.h`, `custom/src/CustomPlugin.h` | No whole-mission preflight or checked append hook for custom item upload. | A small core extension is necessary; core must not depend on Marine types. |

`custom/src/MissionManager/CoverageInspectionPlanCreator.cc` initializes N from C only during creation.
`MarineTask` leaves H/P/E unspecified and Swath nonpositive until explicit configuration. This design introduces no new numeric defaults,
no copy/synchronize action, and no automatic C/N repair. Existing No-Go editor count policy is retained; this package does not add new topology capabilities.

The active Windows cache is `build/P0-01-marine-debug/CMakeCache.txt`: Debug, Qt 6.11.1,
`C:/Qt/6.11.1/msvc2022_64`. Inspected QtPositioning `qgeopolygon.h` exposes `setPerimeter`, `addHole`,
`holesCount`, `holePath`. QtLocation `qdeclarativegeomapitembase_p.h` declares inherited QML `geoShape`, and
`qdeclarativepolygonmapitem_p.h` overrides `setGeoShape`. These private headers are audit evidence only;
production must use public QGeoPolygon/QGeoShape APIs and QML MapPolygon, without a new private-header dependency.
MapPolyline's current QGC usage exposes line color/width, not a demonstrated dash-style API. This design does not invent one.

## 3. Ownership and exposure

One-way flow remains:

```text
MarinePlanContext Task -> explicit plan() -> existing domain/Artifact result
    -> CoverageInspectionComplexItem Qt presentation conversion -> QML editor/map
    -> existing Artifact/current-Task authorization -> checked Mission upload
```

ComplexItem converts coordinates, enums, strings and existing metadata only. It does not buffer, subtract, inset,
decompose, route, repair, reassess safety, rank candidates, regenerate advice, infer readiness, or certify artifacts.
QML formats values, chooses visual tokens, instantiates visual objects and forwards user edits/explicit plan commands.
No planning occurs on load, view creation, getters, selection, upload, or input change.

### 3.1 Task editor interface

Keep `workRegionPolygon` as a source-compatible C alias, and expose clearly named `coveragePolygon` and
independent `navigationPolygon` QGCMapPolygon objects. Each edits only its corresponding Task boundary.
Synchronize both from plan-scoped Task changes with separate recursion guards. O remains `noGoPolygons`.
At most one C/N/O polygon is interactive at a time; switching selection is UI state and does not dirty/stale the result.
Connect boundary path and drag changes to immediate invalidation; do not wait for drag completion to revoke upload.

Expose `hardSafetyMarginM` (existing H alias), `preferredSafetyMarginM`, `executionMarginM`, and
`coverageRequirement` alongside existing Swath and sweep properties. Domain identity uses Standard/Strict;
presentation strings are localized. H/P/E are explicit independent Task edits, with units and E's hard-reserve explanation.
Manual bearing uses existing 0° North/90° East convention and backend normalization. Auto angle is independently labeled.
Show requested Planner (normally Auto) without adding a new planner picker or unsupported algorithm choice.

No epsilon, policy calibration slider, auto parameter adjustment, automatic suggestion application or new Task schema is added.
Blank/nonfinite/unconfigured values display as unconfigured rather than silently filling a number. Incomplete values may remain
in editor/Task state; existing backend schema/topology checks determine whether planning/save is valid.
Changing a planning-critical value invalidates the artifact by existing identity rules; name/sensor edits do not invalidate it.

### 3.2 Read-only presentation interface

Use a read-only `QVariantMap planningPresentation` (NOTIFY existing `planningResultChanged`) and read-only
`bool resultStale`/`bool uploadAllowed` properties. Proposed map schema is an integration view, never persistence or certification data:

| Entry | Contents/source |
| --- | --- |
| `hasCurrentResult`, `status`, `readiness`, `tier` | Current lifecycle plus exact stored enum codes; absent tier represented absent/null, never fabricated D0. |
| `canonicalRuns` | `{firstLeg, legCount, role, safetyClass, path}` from current canonical points/roles/assessments. Split at either role or safety change; shared endpoint only. |
| `quality` | Status/error, requirement, strategy and policy identity, pass/fallback facts; availability/value for every metric and residual/core field. |
| `residualRegions` | Per available residual category `{kind, componentIndex, geoShape}`. QGeoShape wraps QGeoPolygon with all holes. Index preserves source vector order. |
| `diagnosticCandidate` | Separate identity, own role/safety runs and own metrics; never placed in canonical runs or executable getters. |
| `diagnosticOverlays` | Kind, explanation and per-component geoShape; no route joining or arrows. |
| `issues`, `suggestions` | Stable code, source severity/cause, localized display message, raw source explanation, typed reference. Preserve source order. |
| `repair` | Attempted/applied/reason; each selected component ID, entry/direction, transition cost, stored before/after evaluations and path/turn effects. |
| `plannerSource` | Requested ID, resolved ID/version, resolution status/reason, escalation, requested sweep, selected bearing and sweep semantic version. |
| `metrics` | Canonical total/Coverage/Transit length, turns, and meaningful cell count. No canonical metrics borrowed from diagnostics. |

Exact C++ getter/helper names may be finalized during implementation; these data distinctions and ownership are the contract.
Enum values become stable semantic code strings by exhaustive C++ switches, not QML English-text parsing or incidental ordinal tests.
Availability/value entries omit unavailable numeric values or use null; QML prints localized “Unavailable”/“Not applicable”.
Use domain flags for fallback and pass; do not compute a pass from displayed ratio.
Metric deltas for repair may be formatted from available stored before/after numbers; no quality reevaluation or candidate selection is involved.

Use exhaustive `tr()` message templates for all existing issue/suggestion codes, plus source detail where it carries
additional context. Code remains visible (or accessible in the issue detail) alongside localized text. Severity is preserved from the
record. References preserve CanonicalPathLegRange versus DiagnosticCandidateLegRange versus DiagnosticOverlay versus CoverageResidual,
including residual kind/index/leg range; they never become a shared unqualified index. No new advice is generated.

`resultStale` comes from the artifact's stale flag and/or existing `matchesCurrentInput` identity check, not QML dirty state.
Current result clears on invalidation as today. The minimal UI shows “Result out of date — generate a new plan” and no stale route;
retaining historic diagnostic drawings is not required. All current presentation getters clear together and notify when invalidated.
If implementation retains any old explanatory text, it must be explicitly stale and cannot populate current result/route/authorization.
Matching artifact restore presents saved facts without invoking planners/evaluator/router/repair.

## 4. Visual and editor composition

Keep the two existing Marine QML resources and QGC controls, QGCPalette and ScreenTools sizing.
The editor groups common controls (C/N editor selection, Swath, H, Standard/Strict, sweep, requested planner),
advanced P/E with explanation, existing sensors/O controls, then result and issues/suggestions/repair detail.
A failed current attempt is still shown; result visibility is not based on `path.length` or Planned-as-success.

The result panel displays stored readiness, status, tier, safety legend, requested/resolved strategy, sweep provenance,
quality state/coverage ratio/uncovered and critical area, available canonical lengths/turns/cells, selected repair effects,
issues and suggestions. A ReviewRequired panel explains coverage insufficiency OR assessment failure using its actual code.
D2/D3 explain absence of an executable route. Failed/invalid/unplanned scalar defaults are labeled not applicable.

Display localized IngressNotAssessed adjacent to every displayed readiness/result:
“The route from the current vehicle position to the first Mission waypoint has not been assessed.”
For D0/D1 also show the actual source issue record. D2/D3/invalid/unplanned have no executable-entry certification;
the presentation notice is still a scope statement, without synthesizing a domain issue or changing persisted data.

### 4.1 Non-color distinctions and palette

| Object | Theme role | Non-color cue |
| --- | --- | --- |
| C | Primary/map task palette | Explicit C/Coverage Area label and editable boundary. |
| N | Secondary/navigation palette | Explicit N/Navigation Area label, outline treatment distinct from C, independent selected editor. |
| O | Error/high attention palette | Indexed “No-Go” label and prohibited-region fill. |
| PreferredSafe Coverage | Primary mission trajectory | Thicker canonical line and Coverage label in legend/detail. |
| PreferredSafe Transit | Neutral secondary | Thinner canonical line and Transit label. |
| HardSafeWarning canonical leg | Warning/amber palette (`colorOrange` or `colorYellow`) | Warning glyph/label linked to affected run; retains Coverage/Transit width distinction. |
| DiagnosticCandidate | Error palette (`colorRed`) | Dedicated “Diagnostic — NOT EXECUTABLE” map label/marker and distinct thinner treatment, no canonical/waypoint arrows. |
| DiagnosticOverlay | Explanatory area palette | Area outlines/fills with diagnostic label; never a connected route. |
| BoundaryShortfall | Translucent warning | “Boundary shortfall” category label/legend and thinner outline. |
| CriticalUncovered | Translucent error/high attention | “Critical gap” category label/legend and stronger outline. |
| Current/stale and readiness | Status palette | Explicit current/out-of-date and readiness words; stale data never uses current route label. |

Do not use `warningText` as an assumed amber route token: current QGCPalette defines it red.
Use existing semantic orange/yellow/red palette entries; no new hardcoded color or global palette redesign.
Map labels can use existing MapQuickItem/QGCLabel conventions and stored first coordinates; they do not calculate new geometry.
Diagnostic legends alone are insufficient if visible geometry can be mistaken for a route: attach its not-executable label to the map representation.

### 4.2 Regions and holes

For every geographic region component, C++ sets QGeoPolygon perimeter and calls addHole for every source hole,
then exposes QGeoShape in QVariant. QML binds MapPolygon.geoShape. Never bind only the outer path,
paint holes with an opaque map-background polygon, triangulate/clip residuals in QML, merge components,
or derive BoundaryShortfall by subtracting in QML. Boundary and critical layers use their respective backend residual region sets.
Unavailable residual geometry shows an explicit unavailable message, not an empty-success overlay.
Strict fallback components/core may be explained with their available source shapes and stored fallback flags.
Actual QML object instantiation must prove the inherited geoShape binding preserves holes; visual checks verify hole interiors remain unfilled.
If the active renderer cannot do this, stop at the rendering blocker; do not replace it with a misleading solid polygon.

QGCDynamicObjectManager can retain current map item lifecycle (rebuild on result change; destroy on teardown).
Rendering order keeps inputs/residual areas below routes and labels; no append to normal mission waypoint models.
Map display toggles/selection have no impact on readiness, Task fingerprint or upload admission.

## 5. Whole-plan upload protection and minimal core patch

The adapter already rejects one blocked Marine artifact. That is insufficient at the current whole-plan call site:
void append refusal leaves other items in `rgMissionItems`, and `writeMissionItems` still sends a truncated mission.
§27 rejection and §31 UI consistency require closing that actual path, including direct/static upload calls.
This is a missing integration extension point, not a new authorization policy or planning responsibility.

Add these narrow core hooks, with ordinary QGC default behavior preserved:

1. `VisualMissionItem::readyForUpload(QString& reason) const`: virtual, default true for upstream items.
   An upload-readiness-change signal permits aggregation without referring to Marine in core.
2. `VisualMissionItem::appendMissionItemsForUpload(items, parent, reason)`: virtual bool; default invokes existing
   void append and returns true. CoverageInspectionComplexItem overrides it and returns the existing adapter's bool.
   Existing void append remains for source compatibility and non-upload uses.
3. MissionController read-only `uploadAllowed` and `uploadBlockingReason`, aggregating the virtual gate for every item.
   Connect/disconnect through existing item lifecycle; recompute on item add/remove/reset, Task/artifact/result invalidation,
   and emitted gate change. A live preflight always rechecks; UI-cached values never authorize sending.
4. A checked MissionController upload entry reports whether upload was actually submitted; the existing override
   `sendToVehicle()` delegates. Static `sendItemsToVehicle` also reports success/refusal and gates independently.
   The bool here means submission/conversion success, not remote vehicle acceptance or end-action presence.
5. A generic checked file-load helper in PlanMasterController (proposed private
   `bool _loadFromFileChecked(const QString& filename)`, declared in `.h`) returns the actual existing loader success.
   The public void `loadFromFile` remains a compatibility wrapper invoking that helper. Move/reuse its existing load body
   once, retaining messages and ordinary file/dirty association behavior; do not introduce a second parser or validation pipeline.
   The helper returns false for empty filename, file-open failure, text loader failure, JSON parse/structure failure,
   plugin/Marine Task rejection, Artifact/Mission load rejection, or any fence/rally sub-controller failure.
   Only the existing successful text/JSON path returns true; post-load behavior stays unchanged.

CoverageInspectionComplexItem's gate calls `PlanningArtifactCodec::uploadAllowed` with its actual current artifact and Task,
also rejecting missing context/task/artifact and incomplete current geometry editing (including unfinished O editor polygons
not yet represented in Task). It does not promote the stored result or derive readiness. The checked append rechecks admission
and then calls Artifact/current-Task MissionAdapter; it propagates sequence/coordinate refusal too.

Upload flow:

```text
Static file upload: create/start transient controller -> checked existing file load
 -> load refusal: release transient, zero send/write/clearing, no send sequence starts
 -> load success (including valid empty ordinary plan): continue to whole-plan admission below
PlanMasterController checks aggregate backend admission before _sendSequence starts
 -> MissionController checked send rechecks all items
 -> static sendItemsToVehicle checks every item before any conversion
 -> build temporary full Mission with checked append results
 -> any refusal: destroy temporary items, no writeMissionItems, dirty unchanged, sequence Idle
 -> all accepted: submit complete ordinary+Marine mission, retain existing send sequencing
```

Use temporary ownership during conversion and transfer successful items to the existing MissionManager ownership path.
No partial write, no “empty list means refusal” guess, no silent skip and no diagnostic substitution.
The upload conversion success result must be separate from `_convertToMissionItems`' existing end-action bool.
Leave save/KML conversion semantics untouched; JSON save does not require uploadability. ReviewRequired path/artifact remains saveable.
MissionController clears dirty only after actual submission. PlanMasterController resets `_sendSequence` to Idle on checked refusal,
does not continue fence/rally upload, and reports a user-visible blocking reason. Static `sendPlanToVehicle` must call
the checked file-load helper first and only proceed to this protected admission/send path when it returns true.
Load refusal may leave home/default or previously displayed ordinary items, or partially loaded sub-controller state;
none of those may be sent or used to infer success. Release the transient controller on load refusal and later send refusal,
rather than waiting forever for a send-complete signal. Reuse existing error messages (including the existing silent empty-filename
behavior); no file-associated property, dirty flag, `containsItems()` or visual-item count substitutes for actual load success.
Successfully loaded valid empty/home-only ordinary plans retain intentional mission clearing. This distinction adds no new
empty-plan policy and requires no rollback framework for an isolated transient controller that is discarded on refusal.
Null Vehicle access must be guarded. Existing offline/high-latency/armed/firmware checks continue; Marine admission is not bypassable
by firmware mismatch confirmation or a direct C++/QML call.

UI binds toolbar Upload's existing availability checks AND MissionController.uploadAllowed. `PlanView.upload()` also checks it
and displays the backend reason. Existing Save/Save As readiness checks remain separate. ReadyWithWarning remains admitted;
ReviewRequired, DiagnosticOnly, None, stale, invalid, failed, malformed and InfrastructureOnly remain rejected.
No acknowledge/override button changes this result. New generic core hooks use no Marine headers/types or custom feature dependence.

Expected core scope: `VisualMissionItem.h` (and .cc only if needed), `MissionController.h/.cc`,
`PlanMasterController.h/.cc` (minimal checked-load declaration/body and upload sequencing),
`PlanToolBarIndicators.qml`, and `PlanView.qml`.
All domain presentation/editor changes remain in custom source/QML; test/resource registration is limited to changed resources/tests.
No planner/evaluator/repair/schema/version or general mission-state refactor is proposed.

## 6. Validation and review gate

The contract's T09-01–T09-16 matrix is the implementation acceptance plan, not a claim that checks ran during design.
Tests must instantiate actual Marine QML with required properties and read bound values/visual objects; `component.isReady()`
or source text search alone cannot establish behavior. Use existing Qt UnitTest/MissionTest and QSignalSpy/condition waits,
with real domain fixtures or structurally certified Artifact fixtures. Do not relax malformed-artifact checks to manufacture UI cases.

Windows incremental build uses the established VS/Qt environment and
`cmake --build build/P0-01-marine-debug --parallel 2` (or the equivalent existing local build wrapper).
Run focused CoverageComplexItem, new presentation/upload integration tests, IntegratedPlanningResult,
ArduPilotMissionAdapter, MarinePlanIntegration/Context/TaskJson/Identity and affected planner/quality/repair regressions.
Core patches require MissionController, PlanMasterController, selected Mission item/manager regression and PlanView UI suites,
including ordinary non-Marine plans, malformed Marine Task/Artifact file rejection before/within item creation,
empty-name/open/parse/text/sub-controller load failures and successfully loaded valid-empty/home-only intentional clearing.
Assert zero writes, zero accidental clearing/partial resend/fence/rally continuation and transient teardown on load refusal.
Choose actual registered suite names after inspecting CTest.
Keep offline UI tests offline; use MockLink for connected upload verification rather than forcing UI booleans to simulate backend success.

Run clang-tidy for affected production translation units with active compile database; changed-region C++ formatting,
QML format/lint where available, repository vehicle/translation analyzers, `git diff --check`, and a pre-commit attempt.
Record genuine FAIL/BLOCKED separately; Clazy absence is supplemental SKIP. Preserve the known unrelated zh-CN defect and
historical V05-08 review evidence. Do not change translation catalogs during V05-09 merely to make a regression pass;
new source strings still use tr()/qsTr().

Capture representative automated/render/manual observations for D0, D1, ReviewRequired, D2, D3, AssessmentError,
holes, selected repair, stale and dark/light palettes. Inability to observe a manual render remains explicitly unverified;
V05-10's full M00–M09/SITL/manual/freeze certification is a later package.
Produce the required V05-09 implementation report and machine evidence only during implementation,
then stop for a fresh independent reviewer before local commit. No push is authorized.

## 7. Decisions and author-stage outcome

No material frozen-specification conflict was found in this inspection. The missing generic upload extension and
Qt result marshaling are mechanically required implementations of frozen §§27/31, with the minimal scope above.
If review or implementation reveals a need to change coverage/safety/readiness rules, approved input topology,
default H/P/E/Swath, calibration, geometry ownership, or certification behavior, mark **OWNER DECISION REQUIRED**
and stop the affected work package; this proposal does not grant discretionary semantics.

Author-stage validation consists of baseline/status verification and specification/API/Qt-header/visual-convention traceability.
No production build, tests, static analysis or manual render were run for these document-only changes.
Only this design and its proposed contract are written. Independent design review/freezing is the next gate;
implementation and V05-10 are not performed by this author stage.
