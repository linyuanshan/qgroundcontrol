# V05-10 Execution Wrapper

Wrapper identity: **V05-10-EXEC-v1**
Status: **AUTHORIZED — project-owner execution authorization issued 2026-10-04**
Frozen contract: `docs/marine/V05_10_DESIGN_CONTRACT.md` — **V05-10-DC-v1**
Frozen technical design: `docs/marine/V05_10_TECHNICAL_DESIGN.md`
Authorized baseline: `4c1414622293bbb23da881ba1c8f4a76b9a1b9eb` (`marine: freeze V05-10 validation contract`)
Branch: `feature/marine-p2-complex-coverage`

## 1. Purpose and authority

This wrapper records the project owner's explicit authorization to execute V05-10. It does not change any
frozen v0.5 semantic, V05-10 contract clause, calibration value, schema version, SITL acceptance set, or field
boundary.

The frozen V05-10 contract and technical design define the acceptance semantics. This wrapper only defines
execution scope, sequencing, stop conditions, and handoff requirements. If execution would require a material
change to the frozen architecture or semantics, stop and report the conflict instead of redesigning silently.

V05-11 and later work remain unauthorized.

## 2. Authorized work

V05-10 execution may:

1. Create or refine canonical M00–M09 fixtures and scenario identities required by the frozen contract.
2. Add or modify tests needed to close explicit V05-10 evidence gaps, including:
   - M00–M09 scenario-bound oracles;
   - Task/Artifact identity, exact restore and stale behavior;
   - D0–D3 and CoverageQuality state coverage;
   - §23 repair comparator ordering;
   - A10-04 Auto non-escalation after repair exhaustion;
   - backend Mission admission/rejection and sequence atomicity;
   - Terrain/PlanView regression oracles required by T10;
   - v0.5 SITL fixtures required by S10.
3. Make the smallest production-code correction necessary when an executed frozen oracle exposes a concrete
   implementation defect. Such a correction must preserve the frozen architecture, schemas, semantics,
   H/P/E rules, CAL-01/CAL-02, planner identities, and SITL acceptance set.
4. Build QGroundControl on the Windows acceptance platform and bind final evidence to the accepted source HEAD
   and executable SHA-256.
5. Run the focused Marine tests, affected QGC/P0/P1 regressions, M00–M09 acceptance matrix, and F3 baseline
   reproduction required by R10.
6. Execute the frozen v0.5 SITL set:
   - ALLOW+EXECUTE: M00 D0/Ready; M04 D1/ReadyWithWarning.
   - CONNECTED BACKEND REJECTION: M08 ReviewRequired; M05 D2/DiagnosticOnly; M09 valid D3; stale formerly-valid M00 Artifact.
7. Execute the frozen Windows GUI/manual acceptance checklist.
8. Produce V05-10 machine evidence and `docs/marine/V05_10_IMPLEMENTATION_REPORT.md`.
9. Perform the final V05-10 freeze audit and prepare a handoff for fresh independent implementation review.

## 3. Explicitly unauthorized

V05-10 execution must not:

- change `V05_10_DESIGN_CONTRACT.md`, `V05_10_TECHNICAL_DESIGN.md`, the active v0.5 specification, or
  frozen V05-08/V05-09 contracts;
- change Task v3, Artifact v3, Marine extension v2, `p2.v0.5.planning.2`, `coverage-quality.v1`,
  CAL-01 `0.99`, or CAL-02 `0.50 m`;
- weaken H, E, backend upload gating, stale handling, D0–D3 separation, or canonical/diagnostic separation;
- add new product capabilities, generic frameworks, 3D planning, dynamic vehicle-shape certification,
  automatic parameter search, or other V05-11/future-phase scope;
- reinterpret historical P2-13K SITL as v0.5 acceptance;
- perform real-USV field validation; field status remains **FIELD VALIDATION DEFERRED BY FROZEN SPEC**;
- push, merge, release, or start V05-11;
- self-approve V05-10 completion.

During execution, do not stage or commit implementation changes merely because local checks pass. Finish with
a reviewable worktree, evidence, and implementation report, then stop for fresh independent review. A final
implementation/freeze commit requires separate post-review authorization.

## 4. Required execution order

### E0 — Preflight and baseline

Before the first implementation edit:

- verify workspace, branch, HEAD, working-tree/index state, and upstream relation;
- read `AGENTS.md`, `CODING_STYLE.md`, `.github/CONTRIBUTING.md`, `test/README.md`,
  `.github/ci-overview.md`, `tools/README.md`, the active v0.5 specification, V05-10-DC-v1, and the
  frozen technical design;
- inspect the actual APIs/tests to be changed;
- preserve unrelated worktree state;
- record the execution baseline and intended evidence root.

### E1 — Canonical M00–M09 scenario identities

Establish one named canonical fixture/scenario identity and hash for every M00–M09. Composite evidence is
allowed only when all contributing layers bind to the same scenario/Task identity, the same Artifact/result
identity where applicable, and the same accepted source/binary.

Do not combine unrelated inputs into one Mxx PASS.

### E2 — Close explicit automated-oracle gaps

Implement the minimum test/fixture changes required by the frozen contract. In particular, directly cover:

- M00–M09 predicates and frozen expected outcomes;
- M06 same-geometry Standard/Strict policy contrast;
- M07 repair success and M08 repair exhaustion;
- Q10-08 seven-layer §23 repair lexicographic order;
- A10-04 repair exhaustion/coverage insufficiency alone does not cause Auto → BCD escalation;
- exact Artifact restore without planner/evaluator/router/repair;
- stale and malformed fail-closed behavior;
- backend zero-write/zero-partial-conversion rejection semantics;
- Terrain unknown-first-marker and non-degenerate chart/display regression requirements.

If a frozen oracle fails because the implementation is wrong, make only a bounded correction. If satisfying the
oracle would require changing frozen semantics or architecture, stop and report instead.

### E3 — Windows build and binary binding

The current V05-10 acceptance source must build successfully under the established VsDevCmd x64 Windows kit:

`cmake --build build/P0-01-marine-debug --target QGroundControl --parallel 8`

Record source HEAD, toolchain, full command, exit code, raw build log, and resulting
`QGroundControl.exe` SHA-256. All final focused/regression/SITL/manual evidence must identify that accepted
binary, or a later explicitly recorded rebuild and its evidence invalidation/re-run consequences.

Any unexplained build failure or binary/evidence mismatch is blocking.

### E4 — Automated validation and F3 attribution

Run the frozen focused and affected regression matrix. Record every suite/case as PASS, FAIL, SKIP,
ENVIRONMENT BLOCKED, or NOT EXECUTED.

For each current failure claimed PRE-EXISTING/UNRELATED, reproduce the exact case against V05-08 baseline
`4a948c8842a915fe3125322ec9eec9e66a007fb4` under equivalent Windows build/runtime conditions and retain
both raw outputs. At minimum re-check:

- `MissionManagerTest::_testErrorAckFailureStrings`;
- `PlanViewUITest::_testPlanViewStates`;
- `PlanViewUITest::_testRoverWaypointOnEmptyPlan`.

Historical V05-09 attribution is a locator, not current V05-10 proof.

### E5 — Frozen v0.5 SITL acceptance

Use the frozen S10 contract. Default environment is the P2-13K ArduRover 4.7.0 environment recorded in
V05-10-DC-v1.

For M00/M04 ALLOW+EXECUTE:

- use current/non-stale matching Task + Integrated Artifact;
- use the real backend product chain through MissionAdapter and MissionController/PlanMasterController;
- anchor the SITL vehicle start/current position to the first canonical Mission waypoint within the explicitly
  recorded scenario tolerance;
- require accepted Mission Start ACK, observed ACTIVE state, ordered waypoint progression, Mission Complete,
  and no Marine-generated RTL;
- do not treat unassessed ingress as safety evidence.

For M08/M05/M09-valid-D3/stale rejection:

- keep the SITL vehicle connected but do not enter AUTO;
- require backend refusal, zero `writeMissionItems`, zero new executable mission, zero partial send/conversion,
  unchanged mission/sequence, no accidental mission clear, and no stuck sync/send state.

Environment absence or fixture inability is not PASS; classify it accurately.

### E6 — Windows GUI/manual acceptance

Execute the frozen manual checklist with the actual Windows QGC build and real PlanView/Marine task surfaces.
Bind screenshots/recordings, plan files, logs, fixture identity, source HEAD, and binary SHA to the same acceptance
run. Human observation does not replace backend or automated safety oracles.

### E7 — Freeze audit and handoff

Produce:

- machine-readable V05-10 evidence with source/binary/fixture identities;
- `docs/marine/V05_10_IMPLEMENTATION_REPORT.md`;
- complete M00–M09 traceability;
- build/test/static-analysis/SITL/manual results;
- exact baseline attribution for remaining failures;
- known defects/environment blockers;
- field status exactly recorded as **FIELD VALIDATION DEFERRED BY FROZEN SPEC**.

Then stop with one of:

- **V05-10 — READY FOR INDEPENDENT REVIEW**, or
- **V05-10 — BLOCKED**, with exact blocking findings and evidence.

Do not declare V05-10 CLOSED or software freeze complete without fresh independent review and separate owner
approval.

## 5. Stop conditions

Stop execution and report immediately if any of the following occurs:

- a frozen contract/spec conflict is discovered;
- a required fix would materially change architecture or frozen semantics;
- a schema/semantic version change appears necessary;
- CAL-01/CAL-02 would need recalibration;
- H/E or backend safety gating would need weakening;
- a new SITL acceptance interpretation is required;
- a test failure cannot be classified without changing the acceptance contract;
- real-USV field work would be required to continue;
- unrelated worktree changes cannot be safely preserved.

## 6. Completion boundary

This authorization starts V05-10 execution only. It does not itself prove any M00–M09 case, build, regression,
SITL, GUI acceptance, software freeze, or field readiness.

The execution implementer must not self-certify final acceptance. Fresh independent implementation review is
the mandatory next gate after the V05-10 evidence package is complete.
