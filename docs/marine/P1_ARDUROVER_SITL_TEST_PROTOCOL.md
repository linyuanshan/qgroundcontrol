# P1 ArduRover SITL Test Protocol

This protocol supplies the execution evidence required by P1-13. A MockLink or the lightweight mock vehicle
does not satisfy this protocol because neither executes an ArduRover mission or simulates vehicle dynamics.

## Test Baseline

- QGroundControl branch: `feature/marine-p1-coverage`
- ArduPilot release: `Rover-4.7.0`
- SITL frame: `rover`
- QGroundControl connection: TCP `127.0.0.1:5760`
- Mission commands: `MAV_CMD_NAV_WAYPOINT`
- Final action: remain at the last coverage waypoint; no automatic RTL

Record the QGroundControl commit, Docker image ID, operating system, `WP_RADIUS`, and exported telemetry log for
every completed run.

## Required Scenarios

| ID | Region | Sweep | Safety margin | Expected result |
| --- | --- | --- | --- | --- |
| S01 | Rectangle | Manual 0 degrees | 0 m | Upload and mission completion |
| S02 | Rectangle | Manual 90 degrees | Positive | Upload and mission completion |
| S03 | General convex polygon | Manual oblique angle | Positive | Upload and mission completion |
| S04 | Rectangle | Auto | Positive | Upload and mission completion |
| S05 | General convex polygon | Auto | Positive | Upload and mission completion |

## Procedure

For each scenario:

1. Start the pinned ArduRover SITL and connect QGroundControl over TCP.
2. Confirm the connected vehicle is ArduPilot Rover and record `WP_RADIUS`.
3. Create the Coverage Inspection task, draw the requested region, set sweep and safety parameters, and generate.
4. Save the `.plan`, close it, reload it, and confirm the path and selected angle are restored without replanning.
5. Upload the mission and verify every item is accepted.
6. Arm using the normal SITL safety workflow, switch to AUTO, and start the mission.
7. Observe waypoint progression through every generated waypoint and confirm mission completion at the final waypoint.
8. Export the telemetry log and retain screenshots of the planned path and actual trajectory.
9. Record corner cutting, turn shape, cross-track behaviour, and any interaction with `WP_RADIUS`.

## Acceptance Criteria

- Mission upload completes without rejected or altered commands.
- AUTO progresses through all generated waypoints in order.
- The mission completes and does not append or execute RTL.
- The actual trajectory and corner-cutting observations are retained as evidence.
- Any safety-envelope concern is recorded for P2; it must not be hidden by changing the P1 planner.

## Evidence Record

| ID | Upload | AUTO/progression | Completion | Trajectory/log | Corner and `WP_RADIUS` notes |
| --- | --- | --- | --- | --- | --- |
| S01 | Pending | Pending | Pending | Pending | Pending |
| S02 | Pending | Pending | Pending | Pending | Pending |
| S03 | Pending | Pending | Pending | Pending | Pending |
| S04 | Pending | Pending | Pending | Pending | Pending |
| S05 | Pending | Pending | Pending | Pending | Pending |

P1-13 is complete only when all five scenarios have evidence. Tool startup or unit tests alone do not satisfy it.
