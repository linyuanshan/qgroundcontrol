# V05-09 Design Contract

Identity: **V05-09-DC-v1 (FROZEN).**

Author baseline: `4a948c8842a915fe3125322ec9eec9e66a007fb4`, branch `feature/marine-p2-complex-coverage`.
Status: fresh independent GPT-6.1 Sol R1 design review returned **APPROVED FOR COMMIT** on 2026-10-03.
F09-D1 is closed; the actual V05-08 APIs, frozen specification and QGC architecture were independently checked.
No new discretionary product semantics were identified. Owner §§H–I authorize implementation of this frozen contract.
[Independent review](../../build/v05-09-design-r1-review/REVIEW.md) and
[machine evidence](../../build/v05-09-design-r1-review/review-evidence.json) bind the reviewed and frozen hashes.
This design approval does not approve a V05-09 production commit or certify implementation/visual/SITL/field acceptance.
Author-stage statements below are historical; a fresh implementation review remains mandatory.

Technical rationale and inspected sources: [V05_09_TECHNICAL_DESIGN.md](V05_09_TECHNICAL_DESIGN.md).
Product authority: [P2 v0.5](P2_COMPLEX_COVERAGE_SPEC_v0.5_ADAPTIVE_PLANNING_AMENDMENT.md).
Current owner authorization §§H–I governs stage routing; no governance/specification file is rewritten.

## 1. Bounded scope and prohibitions

V05-09 implements QML editor/result presentation, independent N editing, warning/diagnostic/residual styles,
existing structured advice and selected repair/planner provenance, stale indication, ingress scope statement,
and UI consistency with backend upload admission. Required minimal generic upload integration protects whole-plan sends.

Task != Plan != Mission. Result truth is the V05-08 current integrated result/Artifact model.
ComplexItem and QML must not plan, choose candidates, route, perform geometry/coverage evaluation, repair,
generate advice/diagnostics, calculate readiness, certify artifacts, or create upload authorization from displayed values.
No domain schema, CAL-01/CAL-02, safety/coverage algorithm, semantic version, unsupported topology, numeric default,
automatic Task correction, planner choice framework, sensor protocol or future phase is added.

## 2. Required editor contract

| ID | Contract | Authority |
| --- | --- | --- |
| E1 | C and N have independent QGC polygon editor objects and explicit Coverage Area/Navigation Area labels. Editing C preserves N and vice versa; no automatic expansion/copy/sync after creation. | §§6–8, 28, 31 |
| E2 | Existing indexed O add/select/edit/delete remains; backend validates O against N and C relationships. QML does not reject O merely because it lies outside C, clip it or repair topology. Preserve existing editor count policy. | §7, unaffected P2 editor |
| E3 | Expose Swath, H, P, E, Standard/Strict, Auto/Manual sweep and Manual bearing. Requested Planner and resolved strategy are distinct; planner Auto does not imply sweep Auto. Existing numeric defaults are not invented. | §§6, 9, 15, 20, 31 |
| E4 | H/P/E are independent explicit inputs. Remove existing H setter's automatic P raise; no automatic reduction/increase of other safety inputs or Task geometry. Backend reports invalid values/P < H. | §§6, 9; owner explicit no automatic correction |
| E5 | Planning-critical edits and in-progress polygon drag revoke current admission immediately, expose stale/out-of-date, clear current executable result, and never trigger implicit replan. Name/non-planning sensor/UI changes preserve matching artifact. | §§29–30 |
| E6 | C/N/O editing selection is exclusive UI state, not planning input. Incomplete editor geometry (including unsaved O draft) cannot permit uploading an old artifact. Backend remains responsible for validity. | §§7, 27, 30–31 |
| E7 | No epsilon/calibration editor; localized units and E hard-reserve/Manual bearing explanations use QGC controls, palette and ScreenTools sizing. | §§9, 19, 31 |

## 3. Required result exposure and visual contract

Read-only Qt exposure must preserve these distinctions; exact helper names are implementation details.
Proposed properties are `planningPresentation`, `resultStale`, `uploadAllowed`, and clearly named Task editor properties.
Existing path getters remain canonical-only. All presentation invalidation/load/result changes emit appropriate notifications.

| ID | Contract | Authority |
| --- | --- | --- |
| R1 | Publish current status/readiness/optional D0–D3 tier directly from stored outcome. Planned is a lifecycle value, not Success or Ready. Failed and invalid attempts remain visible. | §§11–13, 31 |
| R2 | Canonical runs split by BOTH Coverage/Transit and PreferredSafe/HardSafeWarning, retaining ordered points and qualified leg ranges. HardUnsafe/ExecutionUnsafe never occur in canonical exposure. | §§14, 31 |
| R3 | Coverage uses primary/thicker trajectory; Transit secondary/thinner. HardSafeWarning has amber/warning role plus warning glyph/label, while preserving canonical identity and role distinction. | §31 |
| R4 | D2 has separate diagnostic runs/metrics with error role and attached “Diagnostic — NOT EXECUTABLE” map label. D3 overlay is explanatory region geometry, never a synthesized joined route. Neither gets mission waypoint numbering/arrows or encoding. | §§13–14, 27, 31 |
| R5 | C/N/O, canonical, diagnostics, boundary shortfall and critical residual are distinguishable using labels/style as well as theme color. Current/stale and all readiness values have explicit text. No color-only certification or English-message parsing. | §§25, 31 |
| R6 | Quality shows exact state/error/requirement/policy/strategy, ratio, target/covered/uncovered/critical metrics and tolerance when available. Unavailable metrics/geometries are explicit, not zero, empty success or Insufficient. AssessmentError is “assessment unavailable/failed”, never policy failure. | §§18–21, 29, 31 |
| R7 | Display backend BoundaryShortfall and CriticalUncovered region sets as translucent warning/error areas. Preserve every component and hole via public QGeoPolygon/QGeoShape + MapPolygon.geoShape. No outer-ring-only fill, fake hole masking, QML subtraction or triangulation. | §§8, 18, 21, 31 |
| R8 | ReviewRequired retains current hard-safe path, available residuals and reason for upload block. AssessmentError retains path and available facts while stating unavailable assessment/residual fields. | §§12, 18, 31 |
| R9 | Source shows requested/resolved ID and semantic identity, resolution/escalation reason, requested sweep mode, selected angle and sweep identity. Unit/cell metrics absent/not meaningful for noncanonical results display Not applicable. | §§15, 29, 31 |
| R10 | Repair shows attempted/applied/reason, selected component IDs/entry/direction/transition cost, stored before/after quality and path/turn effects. No “repair available” inference, new repair action or automatic suggestion application. | §§22–26, 29, 31 |
| R11 | All 15 issue codes and 6 suggestion codes have localized user-facing templates, stable visible/detail code, source severity/cause, raw explanatory detail and typed optional references. Preserve source order and geometry namespace. Empty advice remains empty. | §§25–26 |
| R12 | IngressNotAssessed scope statement appears adjacent to displayed result/readiness: route from current vehicle position to first Mission waypoint is not assessed. Stored issue is also shown where present; presentation notice does not invent a persisted issue for D2/D3/None. | §§9, 12, 27, 31 |
| R13 | Stale displays Unplanned/None and out-of-date notice, with no current canonical/diagnostic route. Matching restore presents saved outcome without planner/evaluator/router/repair calls. Retained explanatory text, if any, is explicitly stale. | §§29–30 |

## 4. Upload/save backend contract

The authoritative Marine admission remains `PlanningArtifactCodec::uploadAllowed(actualArtifact, actualCurrentTask, reason)`.
Its current integrated/non-stale identity + Success + Ready/ReadyWithWarning + certification validation cannot be reproduced in QML.
Missing context/task/artifact, pending incomplete edits and malformed metadata are refused. ReadyWithWarning is accepted with warning visible.

The actual QGC void append and end-action-bool conversion cannot propagate Marine refusal; no custom plugin whole-plan hook exists.
Minimal allowed core integration is generic virtual readiness/checked append in VisualMissionItem, aggregate MissionController admission
and checked upload conversion/submission, PlanMaster checked-load propagation/sequencing protection, and PlanView/toolbar binding.
Static file upload cannot use item admission to detect earlier load rejection: Task/plugin validation may reject the file before
any Marine item is created, leaving an otherwise admissible home-only or ordinary list. Core must not include Marine headers.
Default upstream-item hooks preserve ordinary QGC behavior, file compatibility and existing non-upload conversion.

| ID | Contract |
| --- | --- |
| U1 | Every direct, controller, master-controller and static `sendItemsToVehicle` upload rechecks the whole visual-item list in C++ before conversion. One blocked Marine item blocks the entire send, including mixed ordinary/Marine lists. |
| U2 | Checked upload append propagates actual MissionAdapter failure. Build outgoing items with temporary ownership; any conversion failure destroys temporary items and submits nothing. Do not use empty append or end-action bool as conversion success. |
| U3 | Rejected upload performs zero `writeMissionItems` calls, generates no transmitted partial Mission, leaves visual sequences/items and dirty state unchanged, starts no fence/rally follow-on send, and cannot leave `_sendSequence`/sync stuck. Gate before starting master sequence and unwind checked refusal to Idle. |
| U4 | Static file upload proceeds to whole-plan admission/send only after a checked helper returns the existing actual text/JSON load success. Empty filename, open/text/parse/structure/plugin/Task/Artifact/Mission/fence/rally load rejection returns false: zero writes, no accidental clearing/ordinary or partial resend, no send sequence or fence/rally continuation, and transient release. Reuse the loader once; preserve ordinary loading messages/behavior and void public wrapper. Do not infer success from item count, dirty state, file association or containsItems. Release transient controllers on later send refusal and completion too. Nullable Vehicle access is guarded; no implicit planning/diagnostic encoding. |
| U5 | UI Upload enabled = existing offline/sync/items availability AND backend aggregate admission. Direct invocation and firmware-confirmation callback still recheck backend. Operator acknowledgement/visual toggle/property tampering cannot authorize blocked outcomes. |
| U6 | Save/Save As use data readiness independently. Valid ReviewRequired artifacts remain saveable/reloadable with canonical path and residual intact; failed diagnostic artifacts remain saveable as supported by existing codec. Upload gate must not be substituted for readyForSaveState. |
| U7 | Preserve ordinary QGC upload, MissionSettings end-action behavior, zero-item/home-only clear path and non-Marine .plan compatibility. A successfully loaded valid empty/home-only ordinary file remains eligible for intentional clearing; a rejected file never implies an empty successful plan. JSON save/KML conversion retain their existing contract; no unrelated MissionManager/translation changes. |

Allowed core source extent: VisualMissionItem.h/.cc if needed, MissionController.h/.cc, PlanMasterController.h/.cc
(including the minimal generic checked-loader declaration),
PlanView.qml and PlanToolBarIndicators.qml. Domain changes are forbidden except a demonstrable presentation-only helper;
new generic architecture requires separate owner decision. No safety exception/confirmation path is added.

## 5. Required verification matrix

Test cases can share data-driven fixtures and existing suites, but evidence must identify each oracle separately.
Instantiate real QML objects with required bindings; registration/source text checks alone do not satisfy UI integration.
No fixed delay where condition/signal waits exist. Rendering and remote transmission evidence are separate from model assertions.

| Case | Fixture/action | Required observation |
| --- | --- | --- |
| T09-01 | Current D0 Ready, e.g. M00 semantic fixture | Current readiness/tier/source/quality shown; canonical roles visible; repair false where stored; ingress notice; backend upload accepted. |
| T09-02 | D1 ReadyWithWarning, e.g. M04 | Both role and safety run boundaries preserved; warning glyph/label/amber role; backend accepts without acknowledgement bypass. |
| T09-03 | D0/D1 Insufficient ReviewRequired, e.g. M08 | Canonical route and available residual retained; coverage-blocking issue visible; Save/restore retains facts; zero upload write. |
| T09-04 | D2 DiagnosticOnly, e.g. M05 | Canonical empty; separate diagnostic visible and explicitly not executable; zero mission encoding/upload. |
| T09-05 | D3 valid Failed/DiagnosticOnly and invalid InvalidInput/None | Correct distinct status/readiness; meaningful overlay/explanation, no fake route or scalar success; backend refused. |
| T09-06 | Hard-safe AssessmentError ReviewRequired | Canonical retained; unavailable flags reflected as unavailable; CoverageAssessmentFailed, no CoverageBelowRequirement; no repair action; upload refused. |
| T09-07 | Residual set with multiple components and holes | QML MapPolygon instantiated from geoShape retains all hole paths/counts; boundary/critical categories distinct; rendered hole interior unfilled; source geometry unchanged. |
| T09-08 | Applied repair, e.g. M07, plus attempted-not-applied result | Selected stored component provenance/before/after/path/turn facts shown; applied flag not inferred; suggestions use existing codes. |
| T09-09 | Critical input edit and stale supported artifact load | Immediate Unplanned/None/out-of-date, cleared current visual data, admission false, no automatic planning. Matching load counters stay zero. |
| T09-10 | C/N/O edits, draft/drag, editor switching | Independent C/N values; O outside C not filtered; unfinished draft/drag revokes old admission; no hidden C/N/H/P/E changes; switching alone does not stale. |
| T09-11 | Standard/Strict, H/P/E, Manual/Auto sweep, name/sensor edit | Actual Task fields/notifications/identity update; P < H remains explicit invalid value; no new defaults; Auto planner/sweep distinct; noncritical edits preserve result. |
| T09-12 | Every issue/suggestion enum, typed refs, empty list | Stable codes plus localized templates and retained source detail; source order/severity/cause; canonical/diagnostic/overlay/residual reference namespace preserved. |
| T09-13 | Mixed ordinary + blocked Marine via master/direct/static/file sender; separate empty filename, file-open/text/JSON-parse/structure failures, malformed Marine Task/plugin rejection before Marine item creation, malformed Artifact/Mission rejection, and fence/rally load rejection | Zero `writeMissionItems`/MockLink upload; no accidental mission clearing, existing ordinary/partial resend, or fence/rally follow-on. Existing live-plan dirty/sequences unchanged; no send sequence starts on load refusal; sync Idle and rejected transient released. Fixture must distinguish real load refusal from a valid empty list. |
| T09-14 | Gate initially accepted but checked append rejects (e.g. sequence overflow) | Whole temporary conversion aborted/freed; zero write and no partial send; failure surfaced and no dirty clear/sequence hang. |
| T09-15 | UI forced enabled/direct upload/firmware confirmation, stale or ReviewRequired | Backend still refuses; display toggles never alter admission; real toolbar binding updates on item lifecycle/Task/result changes. |
| T09-16 | Ordinary plan, allowed mixed Ready/ReadyWithWarning, empty/home-only path, successfully loaded valid empty/home-only ordinary file, reset/remove/reload | Existing complete mission/end-action/upload behavior preserved. Actual successful file load permits intentional clear; unlike T09-13 refusal it can submit the expected empty Mission. Ordinary loading messages/file association/compatibility preserved; aggregate lifecycle notifications observed; no new Marine dependency in core. |

Automated focused/domain integration, connected MockLink Mission regression and real QML instantiation are required.
The hole visual oracle and non-color/palette readability require captured renderer/manual evidence; if unavailable, state UNVERIFIED,
never infer visual PASS from a serialized shape or component readiness. V05-10 retains full semantic M00–M09/SITL/manual freeze obligations.

## 6. Implementation DoD and independent gates

1. All editor/result/upload contracts above are implemented without changing frozen domain semantics or unsupported capabilities.
2. Windows incremental build passes using active VS/Qt kit; focused matrix and affected Marine/P0/P1/QGC Mission/UI regressions run.
3. Core static checks for changed production TUs, changed-region formatting, available QML lint/format, repository analyzers,
   diff check and pre-commit attempt have honest results. Clazy absence is supplemental SKIP; cache/DB failure is environment blocked.
4. Report changed files/core extension justification, build command/identity, tests/checks, visual evidence, known issues and remaining
   validation in `docs/marine/V05_09_IMPLEMENTATION_REPORT.md` and `build/v05-09-evidence.json`.
5. A fresh implementation reviewer checks frozen contract, actual complete diff and independently verified evidence, returning
   APPROVED FOR COMMIT or concrete REQUEST_CHANGES. Local commit only after approval; no push.
6. Do not self-certify V05-10, software freeze, SITL or real-USV acceptance. The author stage stops after these two proposed documents;
   next action is independent design review, then freeze only if mechanically derived semantics remain conflict-free.

Any material specification/API semantic conflict is **OWNER DECISION REQUIRED** and stops the affected package.
The reviewer must not silently freeze new product choices. As authored, no such material conflict is identified;
Qt rendering support remains an implementation validation obligation rather than an assumed visual PASS.
