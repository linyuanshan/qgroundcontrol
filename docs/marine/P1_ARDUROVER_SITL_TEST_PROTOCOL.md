# P1 ArduRover SITL Test Protocol

This protocol supplies the execution evidence required by P1-13. A MockLink or the lightweight mock vehicle
does not satisfy this protocol because neither executes an ArduRover mission or simulates vehicle dynamics.

## Test Baseline

- QGroundControl branch: `feature/marine-p1-coverage`
- QGroundControl commit: `90a54665028108e0ed5a7c436d7a4c3b284dd2a0`
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

Status: **PASS**

S01 through S05 were rerun on 2026-09-12 against final software candidate
`90a54665028108e0ed5a7c436d7a4c3b284dd2a0`. The retained plans verify the corrected navigation-angle semantics,
positive-safety-margin cases, and persistence of the generated path and selected angle. Telemetry independently
confirms accepted mission transactions, AUTO execution, ordered waypoint completion, and `Mission Complete`.

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

Evidence archive: `build/P1-13R-evidence/` on the validation workstation. The archive is intentionally kept as a
test artifact rather than committed to source control; the hashes below identify every retained primary artifact.

| ID | Parameters and persisted result | Upload and AUTO progression | Completion and no automatic RTL | Telemetry SHA256 | Observation |
| --- | --- | --- | --- | --- | --- |
| S01 | Rectangle; manual 0 degrees; swath 20 m; safety 0 m; selected 0 degrees; 8 path points | Pass; accepted; WP 1 through 8 in order | Pass; `Mission Complete`; remained in AUTO | `4EA5802DC723609B09CC7346F3B98AA144D93899BDA3CC7ABB7CC040565971D0` | North-South lanes; rounded corner cutting; actual track follows the planned lanes; no boundary exit observed; `WP_RADIUS=3 m` |
| S02 | Rectangle; manual 90 degrees; swath 20 m; safety 8 m; selected 90 degrees; 12 path points | Pass; accepted; WP 1 through 12 in order | Pass; `Mission Complete`; RTL selected 16 s later, outside mission execution | `B049E5BDE58A8952991BFD4C18975EC72771B0105103C4FC8D3C35959CD362AB` | East-West lanes; centerlines visibly inset; nominal swath reaches the target edges; rounded corner cutting; no boundary exit observed; `WP_RADIUS=3 m` |
| S03 | Convex 6-vertex region; manual 30 degrees; swath 20 m; safety 8 m; selected 30 degrees; 14 path points | Pass; accepted; WP 1 through 14 in order | Pass; `Mission Complete`; RTL selected 11 s later, outside mission execution | `9385C323213E80C203DC08BB86E5FD6139A62B3117809FC3018FACBE3F183FDE` | Oblique lanes; rounded corner cutting; actual track follows the planned lanes; no boundary exit observed; `WP_RADIUS=3 m` |
| S04 | Rectangle; auto; swath 20 m; safety 8 m; selected approximately 0 degrees; 8 path points | Pass; accepted; WP 1 through 8 in order | Pass; `Mission Complete`; RTL selected 107 s later, outside mission execution | `142B88DCC4D7CDB8F913DAF6088DACF957B7885A97430C3317680403A6035033` | Deterministic North-South auto lanes; rounded corner cutting; actual track follows the planned lanes; no boundary exit observed; `WP_RADIUS=3 m` |
| S05 | Convex 6-vertex region; auto; swath 20 m; safety 8 m; selected approximately 90 degrees; 12 path points | Pass; accepted; WP 1 through 12 in order | Pass; `Mission Complete`; RTL selected 118 s later, outside mission execution | `64B7C8979FC7D901E0C01D90F7F7B8189FB2E0BAD778BAC8C036CDAF70E0D6CD` | Deterministic East-West auto lanes; rounded corner cutting; actual track follows the planned lanes; no boundary exit observed; `WP_RADIUS=3 m` |

### Saved plan and screenshot hashes

| ID | `.plan` SHA256 | Planned-path screenshot SHA256 | Actual-trajectory screenshot SHA256 |
| --- | --- | --- | --- |
| S01 | `151A2E0A1547FADD6EC7CD64E2B4B55E37E414072ABC841F92C4256F33EF6D79` | `16B0DF42396613FA2CACD1596C8AF60324101BFC6D93DD1704BCD0A71AFDD823` | `E16743C38987F29E8E0C98878C8C60DD6B5AA1825C54864C3A61A9EBAE4E69D5` |
| S02 | `8A180CC8BC2D4CC945F46EA4F0B127D2FDE7C4F641FB0A2DAB2579C774CF8DA8` | `DC66650F60920FD524E9C23F2A2975E7DE7950E3DAB9D1A4917C0515186B08AE` | `8F313B361B4784AF79687DD3CBC7D1021D8F9BD2E391A3227E3D525A809C825D` |
| S03 | `61B32CBD9E303ECD13A4510BADB7703758864CD2CD6D3331968B6007CE46972A` | `4BEF2752497DF22D4A6D650A356A15B56399D7D1196AE7C187F345E321E5479F` | `0BB699A0D426E2E6A0688D0FCE3A5B6FA6FA1E3A011125D57C4377E18FC2B4CF` |
| S04 | `195ECC1A70F721AB5668CAA2A61387BF0573C86E68CAC486074EAE0B3C365C44` | `EBB53A50D3503B86978B818ABB113B8C3B00AFE70FAEC46C536B46CC0221A10E` | `576AE0CBD8664645FFC9C933F35AA56F4A6DCD9B15E14A79D7AE519FBA5D4315` |
| S05 | `E1DD6653AF55A988A1793AA62CCE8459919347CFEDFB4EFB0B71E98ACF5FB24B` | `5FB5007D8F916926413C86D015FF2FD3262A68B298A3196594AFE18C356C52B6` | `2B6098E0AE277E6ECD37BE017B2931D21AC396A0AEC88CA77839A40128692FAB` |

The generated `.plan` files were saved, closed, and reloaded during the recorded procedure. Each retains the
generated path and selected angle without replanning. P1-13 is complete for Engineering Freeze at the validated
software commit. Real-USV validation remains a separate, deferred gate.
