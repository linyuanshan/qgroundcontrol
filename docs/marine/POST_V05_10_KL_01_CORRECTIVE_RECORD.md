# Post-V05-10 KL-01 Corrective Record

## Purpose

This record documents the separately authorized post-freeze correction for **KL-01**:

> Repeated Plan View **Clear** operations could leave `PlanViewRightPanel / PlanTreeView` visually empty.

The historical V05-10 closure evidence remains immutable. In particular,
`docs/marine/V05_10_CLOSURE_RECORD.md` correctly records that KL-01 was open at the
time of the P2 v0.5 software freeze. This corrective package does not rewrite that
historical fact.

## Baseline and isolation

- Frozen closure commit used as the corrective baseline:
  `c215a3a39956214947c663d0f22a1486d54118ea`
- Corrective branch: `fix/kl-01-repeated-clear-idempotency`
- Accepted V05-10 frozen runtime/source identity remains historical and unchanged.
- Archived frozen binary SHA-256 remains:
  `751a8b8c6dea17520ee11cf1207b68c2189ab82203ea6b9840660b78fecb597b`
- No V05-11 work is authorized by this package.
- KL-02 is not modified by this package.

## Root-cause boundary

The defect is treated as a Plan View model/delegate lifecycle problem. The first
Clear legitimately replaces the mission visual-items model. A subsequent Clear on
an already pristine editor does not need another destructive replacement while
TreeView delegates from the previous reset may still be completing asynchronous
teardown.

The correction deliberately does **not** change `PlanMasterController::removeAll()`.
That method is retained unchanged because internal vehicle/controller lifecycle code
also calls it and may rely on its full reset semantics.

Instead, Plan View uses a new idempotent UI-facing entry point:

- `PlanMasterController::clearPlanEditor()`
- `PlanToolBarIndicators.qml` routes offline Plan Editor Clear through that method.
- A truly pristine editor returns without replacing `MissionController::visualItems`.
- Any meaningful clearable state still delegates to the existing `removeAll()` path.
- Online `removeAllFromVehicle()` behavior is unchanged.

## Scope

Production changes are limited to:

- `src/MissionManager/PlanMasterController.cc`
- `src/MissionManager/PlanMasterController.h`
- `src/PlanView/PlanToolBarIndicators.qml`

Regression coverage is added only in:

- `test/MissionManager/PlanMasterControllerTest.cc/.h`
- `test/QmlUITests/PlanViewUITest.cc/.h`

The following frozen semantic scopes have zero diff from the V05-10 closure baseline:

- `custom/src/Marine`
- `custom/test/Marine`
- `docs/marine/P2_COMPLEX_COVERAGE_SPEC_v0.5_ADAPTIVE_PLANNING_AMENDMENT.md`

No planner selection, BCD/lawnmower behavior, H/P/E interpretation, hard/warning
safety classification, coverage-quality evaluation, repair policy, persistence
schema, upload admission semantics, or M00-M09 frozen scenario semantics are
changed.

## Validation

Authoritative Windows build used the repository's existing VsDevCmd x64 build path
and completed successfully.

Focused validation on the resulting corrective binary:

| Validation | Result |
| --- | --- |
| `PlanMasterControllerTest` | 72 / 72 PASS |
| `_testRepeatedClearPlanEditorIsIdempotent` | PASS |
| Windows onscreen `PlanViewUITest` KL-01 case | PASS |
| `MissionControllerTreeTest` | 11 / 11 PASS |
| Windows onscreen `CoveragePlanViewUITest` | 6 / 6 PASS |
| Frozen Marine semantic regression set | 133 / 133 PASS |
| `git diff --check` | PASS |

The two raw failures in the full Windows onscreen `PlanViewUITest` remain exactly
the same failure names and messages as the V05-10 frozen baseline:

- `_testPlanViewStates`
- `_testRoverWaypointOnEmptyPlan`

They are the previously attributed localization-text baseline failures and are not
introduced by KL-01.

The six-suite frozen semantic regression comprises:

- `IntegratedPlanningResultTest`
- `AutoCoveragePlannerTest`
- `SimpleMonotoneCoveragePlannerTest`
- `CoverageSafetyTest`
- `CoverageQualityEvaluatorTest`
- `CoverageRepairTest`

Aggregate result: **133 tests, 0 failures, 0 errors, 0 skips**.

The default offscreen GUI path can hit the previously observed Qt
`qnumeric.h` assertion during UI startup, so GUI acceptance for this corrective
package uses the Windows `--onscreen` path, consistent with the project's actual
Windows GUI acceptance practice.

## Binary identity

Corrective working binary SHA-256 after the final narrowed implementation build:

`753065452481b22c92521cc74fc7b9e628160580cd015c2fb2ea914e0c07681c`

This is a new post-freeze corrective binary. It does not replace or mutate the
archived V05-10 frozen binary; the latter still hashes to
`751a8b8c6dea17520ee11cf1207b68c2189ab82203ea6b9840660b78fecb597b`.

## Status

- **KL-01: CORRECTED AND REGRESSION-VALIDATED on the corrective branch.**
- **KL-02: OPEN / NOT FIXED.**
- **V05-10 historical software-freeze record: unchanged.**
- **V05-11: NOT AUTHORIZED.**
- **FIELD VALIDATION DEFERRED BY FROZEN SPEC.**
