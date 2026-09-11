# P1 ArduRover SITL Test Protocol

This protocol supplies the execution evidence required by P1-13. A MockLink or the lightweight mock vehicle
does not satisfy this protocol because neither executes an ArduRover mission or simulates vehicle dynamics.

## Test Baseline

- QGroundControl branch: `feature/marine-p1-coverage`
- QGroundControl commit: `5bb1cf4f3c02ebde5a5459a268ae779ca83da9a5`
- ArduPilot release: `Rover-4.7.0`
- Docker image: `sha256:001f20d07215138f2cb4aebc8eb691c8f4c3a23825a01f343b3f5397d262d3f0`
- Operating system: Windows 11 Pro `10.0.26200` (build `26200`)
- SITL frame: `rover`
- QGroundControl connection: TCP `127.0.0.1:5760`
- Mission commands: `MAV_CMD_NAV_WAYPOINT`
- Final action: remain at the last coverage waypoint; no automatic RTL

Record the QGroundControl commit, Docker image ID, operating system, `WP_RADIUS`, and exported telemetry log for
every completed run.

## Final Freeze Closure Revalidation

Status: **Pending**

The evidence below was captured at commit `5bb1cf4f3c02ebde5a5459a268ae779ca83da9a5`. Final freeze closure changes
the navigation-angle conversion and safety-margin lane placement, so that evidence is retained as the prior P1-13
baseline but does not validate the final closure candidate. S01 through S05 must be rerun against the final merged
commit before P1-13 can be closed for Freeze.

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

## Prior Evidence Record (`5bb1cf4f3c02ebde5a5459a268ae779ca83da9a5`)

| ID | Upload | AUTO/progression | Completion | Trajectory/log | Corner and `WP_RADIUS` notes |
| --- | --- | --- | --- | --- | --- |
| S01 | Pass — items accepted | Pass — WP 1→12 | Pass — final waypoint reached | `S01/S01.tlog`, SHA256 `61F72AA8DFE2EF6F86DF69F86889ECDF4953DD44A500D2A40B3F46E326512A88` | WP_RADIUS=3 m; visible corner cutting, no boundary exit |
| S02 | Pass — items accepted | Pass — WP 1→6 | Pass — final waypoint reached | `S02/S02.tlog`, SHA256 `ADB56B35F25F744F8E982C175D80B9E054D2309FBF84C51C8F2834C37A325489` | WP_RADIUS=3 m; visible corner cutting, no boundary exit |
| S03 | Pass — items accepted | Pass — WP 1→12 | Pass — final waypoint reached | `S03/S03.tlog`, SHA256 `E92F3E479A0CD872F0B0A331B4B5E20F9B5CD8A5F60C9898E9C48FEB0AA17CB0` | WP_RADIUS=3 m; visible corner cutting, no boundary exit |
| S04 | Pass — items accepted | Pass — WP 1→6 | Pass — final waypoint reached | `S04/S04.tlog`, SHA256 `6AF4A4E9B61EFE580241A72A79E31053DCC82172220F342BEBEC790016DB11B3` | WP_RADIUS=3 m; visible corner cutting, no boundary exit |
| S05 | Pass — items accepted | Pass — WP 1→10 | Pass — final waypoint reached | `S05/S05.tlog`, SHA256 `C9DE2A290A74208C42CD35FB1DF52F4D90AE422BC17F6D2B8A4494E2466A364E` | WP_RADIUS=3 m; visible corner cutting, no boundary exit |

## Final Closure Candidate Evidence

| ID | Upload | AUTO/progression | Completion | Trajectory/log | Corner and `WP_RADIUS` notes |
| --- | --- | --- | --- | --- | --- |
| S01 | Pending | Pending | Pending | Pending | Pending |
| S02 | Pending | Pending | Pending | Pending | Pending |
| S03 | Pending | Pending | Pending | Pending | Pending |
| S04 | Pending | Pending | Pending | Pending | Pending |
| S05 | Pending | Pending | Pending | Pending | Pending |

P1-13 is complete for Freeze only when all five final-closure candidate rows have evidence. Tool startup, prior-build
evidence, or unit tests alone do not satisfy it.
