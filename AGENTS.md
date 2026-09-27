# AGENTS.md

Repository guidance for work in this QGroundControl-derived Marine Robotics project.

## 1. Authority and scope

Apply one authority chain:

1. Explicit instructions from the project owner for the current task.
2. This repository governance file.
3. The reviewed P2 v0.5 amendment for matters it defines or supersedes.
4. Historical P2 requirements where v0.5 does not supersede them.
5. Unaffected frozen P0/P1 requirements.
6. Upstream QGroundControl guidance.

The v0.5 amendment supersedes conflicting P2 clauses only; it does not discard unaffected P0/P1
architecture. Historical alignment drafts are not normative.

Active V05 design authority:
`docs/marine/P2_COMPLEX_COVERAGE_SPEC_v0.5_ADAPTIVE_PLANNING_AMENDMENT.md`

Use that specification for exact domain, geometry, safety, status, persistence, test, and
acceptance
semantics. P2 v0.2 is historical authority only where not superseded. Frozen P0/P1
specifications and field
protocols remain authoritative for unaffected requirements.

## 2. Current phase and stop gates

If implementation requires a material change to the approved architecture, stop and report
the issue before making the broader redesign.

P2 v0.5 architecture Design Freeze is complete. V05 implementation, SITL validation, P2
software freeze, and
field validation are not complete.

V05 governance state:

| State | Work |
| --- | --- |
| Complete | V05-00A Repository Evidence Audit |
| Complete | V05-00A-1S Authority Baseline Delta Audit |
| Complete | V05-00A-2 Architecture Consistency Review |
| Complete | V05-00B Specification Authoring and independent review |
| Complete | P2 v0.5 Specification Architecture Design Freeze |
| Complete | V05-00C AGENTS.md Authority Reconciliation |
| Complete | V05-00C-OPT AGENTS.md Context Optimization |
| Complete | Pre-V05-01 working-tree and baseline reconciliation |
| Not authorized | V05-01 production implementation |

V05-01 must not begin until the pre-V05-01 gate is completed and the project owner explicitly
authorizes
implementation. Do not classify, stage, discard, archive, commit, or otherwise reconcile the
dirty working
tree as part of unrelated work. Discarding or archiving experimental/unapproved work requires
explicit
authorization.

After each authorized work package, stop for independent review. Do not advance automatically
to the next
package. Do not claim v0.5 implementation, SITL, software freeze, or field acceptance is
complete without
separate evidence and approval.

## 3. Required preflight

Before repository edits:

- Read `CODING_STYLE.md` and `.github/CONTRIBUTING.md`.
- When changing tests, read `test/README.md`; when changing build, CI, or test integration,
  read `.github/ci-overview.md`.
- For V05, read the active specification named above. Consult frozen P0/P1 specifications or
  protocols when needed to preserve unaffected authority.
- Inspect current implementation and APIs before relying on documented or remembered signatures.
- Before the first edit, state which instruction/design files were read and material API
  differences found.

Follow the current canonical workflow in `tools/README.md`, CI configuration, and the active
local
environment. Build incrementally for multi-file Qt/C++ changes. Run focused tests and relevant
P0/P1 and QGC
regressions for the active work package. Do not claim checks that were not run.

## 4. Permanent architecture boundaries

Preserve these invariants:

- **Task != Plan != Mission.** Tasks state what to accomplish; plans propose how; missions are
  executable MAVLink commands.
- Coverage planners are pure Marine domain logic and do not depend on QML, QObject, Fact,
  QGeoCoordinate,
  MissionItem, Vehicle, or PlanMasterController.
- QML is a presentation/editor layer and does not implement planning, geometry, coverage
  evaluation, route ordering, or repair algorithms.
- `CoverageInspectionComplexItem` is an integration adapter; it does not own geometry or
  planning algorithms.
- `ArduPilotMissionAdapter` converts an existing planning result; it does not plan, evaluate
  coverage, repair, or reroute.
- `MarinePlanContext` is scoped to its plan. Do not introduce a global task/context registry
  without an approved lifecycle need.
- Prefer QGC custom-build extension points and Marine-specific code. Minimize QGC core changes
  and justify
  any necessary core patch with the unavailable extension point, minimal scope, and regression
  evidence.
- QGC must not link ROS 2 runtime libraries such as `rclcpp`.
- Keep USV plans free of automatic UAV takeoff/landing semantics unless an approved phase
  requires them.
- Use QGC Facts for vehicle parameters, guard nullable `Vehicle*` access, and keep firmware-specific behavior behind firmware-plugin
  abstractions. Do not use production `Q_ASSERT` as runtime error handling or fixed test delays where condition/signal-based waits are
  available.

## 5. Active V05 semantic invariants

Use the v0.5 specification for exact definitions and formulas. Keep these architectural rules
explicit in
implementation and review:

- CoverageArea and NavigationArea are distinct. CoverageArea defines what must be observed;
  NavigationArea
  defines permitted static vehicle motion. Coverage generation follows CoverageTarget;
  navigation space does
  not become extra required coverage.
- H and E are hard safety requirements and are never automatically reduced. Hard safety cannot
  be traded for
  coverage or path quality. Follow the approved candidate-tier process for preferred-margin
  fallback.
- Use the simplest applicable approved strategy; escalate only when capability or topology
  requires it.
  Coverage deficit is not automatically topology failure. The P1 `LawnmowerCoveragePlanner`
  remains a
  regression baseline, not the v0.5 Auto planner.
- Coverage Repair is conditional on a reliable Insufficient assessment. AssessmentError never
  triggers
  repair. Do not make whole-boundary traversal, synthetic NavigationArea perimeter coverage, or
  boundary-first assembly unconditional.
- Canonical executable geometry contains hard-safe path data only. Diagnostic candidates and
  overlays are
  separate, non-executable, and never enter MissionAdapter.
- PlanningStatus Success alone does not authorize upload. MissionAdapter admits only current,
  non-stale
  canonical results with PlanningStatus Success and MissionReadiness Ready or
  ReadyWithWarning. Enforce the
  gate in the backend, not only in QML.

## 6. Marine persistence

Active v0.5 Marine-private versions:

| Layer | Version |
| --- | --- |
| Top-level Marine extension | `marine.version = 2` |
| MarineTask | v3 |
| Planning Artifact | v3 |

This Clean Break applies only to Marine-private schemas. Preserve ordinary upstream
QGroundControl `.plan`
compatibility. Do not require runtime compatibility or migration for obsolete Marine-private
schemas.

A matching supported artifact restores without replanning. Semantic or fingerprint mismatch
makes it stale;
discard executable planning state and do not replan automatically. Refer to v0.5 §§28–30 for
exact
ownership, fingerprint, load, and invalidation semantics. Keep persistence conversion
centralized in
`MarineTaskJsonCodec`; retain Task != Plan != Mission and plan-scoped context.

## 7. V05 work-package routing and calibration gates

| Package | Scope |
| --- | --- |
| V05-01 | Clean-break domain/schema/artifact/fingerprint infrastructure |
| V05-02 | Coverage / Navigation / No-Go geometry split |
| V05-03 | H/P/E safety model and candidate tiers |
| V05-04 | Unified CoverageQualityEvaluator; Standard/Strict; residual geometry |
| V05-05 | Auto planner and SimpleMonotone |
| V05-06 | Target/navigation-aware BCD |
| V05-07 | Conditional Coverage Repair |
| V05-08 | MissionReadiness, diagnostics, issue/suggestion model, artifact persistence, MissionAdapter gate |
| V05-09 | QML/editor/result visualization |
| V05-10 | M00–M09, regressions, persistence, Mission integration, SITL, manual acceptance, freeze audit |

Two calibration decisions await project-owner approval; do not guess values:

- CAL-01 — `StandardCoveragePolicy.minimumCoverageRatio`
- CAL-02 — `StandardCoveragePolicy.boundaryToleranceM`

V05-04 Standard numerical threshold implementation is blocked until both are approved. The
architecture
remains frozen.

Before V05-01, classify the tracked and untracked working-tree changes, preserve or commit
approved
historical work, and establish a clean named v0.5 baseline commit. Do not discard or archive
experimental/unapproved work without explicit authorization. This gate is mandatory and is not
performed
implicitly during other packages.

## 8. Validation, field status, and scope guardrails

For exact V05 acceptance and persistence/Mission/SITL validation, use the active specification
§§32–33. Run
focused tests and relevant P0/P1 and QGC Mission regressions for the active work package.
M00–M09 are
semantic test oracles; a dirty-worktree experimental path is not an expected result merely
because it
exists.

Historical P2-13K SITL evidence is regression evidence only. It does not certify v0.5 or field
readiness. P1
real-USV field validation is **DEFERRED and NOT EXECUTED**; preserve
`docs/marine/P1_REAL_USV_FIELD_VALIDATION_PROTOCOL.md` and complete its requirements before P2
real-USV
field acceptance. Specification/design approval, software completion, SITL, and field
acceptance are
separate claims.

Keep V05 within its approved scope. For detailed exclusions, use v0.5 §34; do not
opportunistically add
future-phase capabilities or generic frameworks.

Clipper2 remains pinned, Marine-private, and hidden behind the Marine geometry backend.
Preserve fork-level
attribution in `custom/src/Marine/THIRD_PARTY.md`; do not change upstream license files to
establish a new
policy.

## 9. Marine Windows acceptance and reporting

Keep work-package changes scope-limited and reviewable; do not mix unrelated cleanup or
refactoring into the package.

Windows is the primary Marine development and acceptance platform. Windows build, focused
tests, and core
static-analysis results are authoritative local work-package evidence; Linux/macOS and
Linux-only tools are
supplemental and do not supersede valid Windows results.

If Clazy is unavailable, report **supplemental static-analysis SKIP**. Its absence alone does
not block a
package. This policy changes gate classification only; it does not waive available core checks
or conceal
failures. A package is blocked by a genuine code failure, test failure, or failure of
required/core static
analysis.

Before reporting a Marine work package complete, state:

1. Files changed and architecture decisions/deviations.
2. Build command and result.
3. Tests and required checks run, with results.
4. Remaining known issues and whether the package DoD is met.
5. The recommended next package without implementing it automatically.

For document-only or audit tasks, state the applicable validation performed and any
unperformed checks. Stop
when the requested DoD is reached.
