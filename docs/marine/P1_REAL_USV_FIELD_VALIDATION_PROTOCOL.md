# P1 Real USV Field Validation Protocol

Status: **DEFERRED — NOT EXECUTED**

This protocol is the acceptance test for P1-14. It does not replace execution on a real USV. P1-14 remains
incomplete until the field run and its evidence satisfy every acceptance criterion below.

P1 Engineering Freeze does not close this protocol. It remains mandatory and must be completed before P2
real-USV field acceptance. Deferral changes scheduling only; it does not delete, substitute, or weaken any
precondition, procedure, acceptance criterion, or evidence requirement below.

## Test Scope

- Use one general convex work region with no internal No-Go region.
- Exercise the P1 Coverage Inspection path exactly as generated and uploaded by QGroundControl.
- Sensor recording may be started manually and is not a P1 acceptance blocker.
- Do not change the planner or add turn-radius behaviour to make the field result pass.

## Preconditions

- P1-13 SITL evidence is complete for S01 through S05.
- The test area, weather, water conditions, traffic, and local operating constraints are suitable for the run.
- A qualified operator has tested manual control, disarm, emergency stop, and mission abort before AUTO operation.
- A spotter maintains visual observation and the operator can immediately take manual control.
- Position, heading, propulsion, battery, communication link, and failsafe behaviour have been checked.
- The work region and safety margin leave adequate clearance from shore, vessels, people, structures, and hazards.
- QGroundControl telemetry-log saving is enabled and the evidence clock is synchronized.

If any precondition is not satisfied, do not arm or start AUTO.

## Baseline Record

Record these values before the run:

| Field | Value |
| --- | --- |
| Date/time and timezone | Pending |
| Test location or site identifier | Pending |
| QGroundControl commit | Pending |
| Vehicle and autopilot hardware | Pending |
| ArduRover firmware version | Pending |
| Positioning source | Pending |
| `WP_RADIUS` | Pending |
| Work-region vertex coordinates | Pending |
| Swath width and safety margin | Pending |
| Sweep mode and selected angle | Pending |
| Weather, water, and traffic conditions | Pending |
| Operator and spotter | Pending |
| Evidence archive location | Pending |

## Acceptance Procedure

1. Verify manual control, mission abort, disarm, and emergency-stop behaviour in a safe state.
2. Create a Coverage Inspection task and draw the approved convex work region without a No-Go region.
3. Set swath width, safety margin, and sweep mode; generate the plan and retain a planned-path screenshot.
4. Save the `.plan`, close it, reload it, and verify the path and selected angle are restored without replanning.
5. Upload the mission and confirm that every mission item is accepted without alteration.
6. Arm using the approved field procedure, enter AUTO, and start the mission.
7. Observe ordered waypoint progression and maintain readiness to abort throughout the run.
8. Confirm completion at the final waypoint and return control using the approved field procedure.
9. Export the `.tlog`, actual trajectory, final `.plan`, screenshots, and operator notes.
10. Record cross-track behaviour, turns, total coverage duration, interventions, and abort/failure behaviour.

## Acceptance Criteria

- Task creation, planning, save/reload, and mission upload complete without replanning or rejected mission items.
- AUTO follows the generated waypoint order and reaches the final coverage waypoint.
- The USV completes the coverage mission without leaving the approved operating area.
- Planned and actual paths can be compared from retained evidence.
- Cross-track and turn behaviour, coverage duration, and any intervention are recorded.
- Manual takeover or mission abort remains available and its verified behaviour is recorded.
- No safety concern is hidden by changing the P1 planner or introducing a P2 feature.

## Evidence Record

| Evidence | Result or reference |
| --- | --- |
| Preconditions and baseline | Pending |
| Generated `.plan` | Pending |
| Planned-path screenshot | Pending |
| Mission upload and item acceptance | Pending |
| AUTO waypoint progression | Pending |
| Mission completion | Pending |
| Telemetry `.tlog` and SHA256 | Pending |
| Actual-path export or screenshot and SHA256 | Pending |
| Cross-track observations | Pending |
| Turn observations | Pending |
| Coverage duration | Pending |
| Abort/failure behaviour | Pending |
| Operator/spotter sign-off | Pending |

P1-14 is complete only when all evidence entries are resolved with retained, reviewable evidence. A desktop test,
MockLink run, SITL run, or an unrecorded real-USV run does not satisfy this protocol.
