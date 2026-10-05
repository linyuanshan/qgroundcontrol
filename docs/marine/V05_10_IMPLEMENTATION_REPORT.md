# V05-10 / V05-10-R1 实施与证据报告

本轮状态：**V05-10-R1 — READY FOR INDEPENDENT REVIEW**。

两个 R1 SITL blocker 已由本轮固定 fixture 的正式验收关闭。此结论只表示准备交给独立实施审查，不是作者自我批准；整体 V05-10 的 GUI 手工验收尚未执行，不宣称软件冻结、SITL 全域认证或实船就绪。未启动 V05-11。

## 1. 授权、预检与继承改动

branch 为 `feature/marine-p2-complex-coverage`，HEAD 为 `cc70699c25f7f396907cf937c95f41daa9ff64b7`，与所有者指定值完全一致。编辑前已阅读 AGENTS、执行 wrapper、冻结设计合同/技术设计、v0.5 amendment、原实施报告、CODING_STYLE、CONTRIBUTING、test README、CI overview 和 tools README。

开始时六个 test 文件及原报告已有 V05-10 改动，已逐项归因并在 [preflight 快照](F:/Projects/qgroundcontrol/build/v05-10-r1/preflight) 保留原文件、哈希及 inherited.patch。未覆盖、丢弃或归并无关工作。R1 未修改的四个继承文件（CoveragePlanViewUITest.cc、CoverageRepairTest.cc、IntegratedPlanningResultTest.h、MarineSITLValidationTest.h）与开始时哈希相同。

材料 API 差异：原 resetMissionExecutionState 仅设置 HOLD/disarm，不能重置位置、朝向及运动历史；且原 post-reset 位于失败 QVERIFY 后。GeoReference::create(region) 采用区域中心，fixture 的车辆原点投影不能直接保证真实 Task 管线的 1mm 入口锚定。

## 2. R1 变更文件与架构边界

- [MarineSITLValidationTest.cc](F:/Projects/qgroundcontrol/custom/test/Marine/MarineSITLValidationTest.cc)：执行前状态、GLOBAL_POSITION_INT 全量采样、固定投影锚定、完整身份输出及正式验收前身份比对；轨迹观察持续到 Complete 后静止。
- [IntegratedPlanningResultTest.cc](F:/Projects/qgroundcontrol/custom/test/Marine/IntegratedPlanningResultTest.cc)：使用共享最终 M00/M04 fixture，并加载真实 SITL Task/Artifact 验证身份、几何、重算结果、持久化和 MissionAdapter NAV_WAYPOINT。
- [V05ExecutionFixtures.h](F:/Projects/qgroundcontrol/custom/test/Marine/V05ExecutionFixtures.h)：一次固定的共享最终 fixture、canonical string/hash。
- [v05_10_r1.py](F:/Projects/qgroundcontrol/tools/simulation/v05_10_r1.py)：fresh 容器、固定测量集、身份准备、正式验收和独立于 QTest 返回的 finally 停止。
- [V05_10_R1_EXECUTION_CALIBRATION.md](F:/Projects/qgroundcontrol/docs/marine/V05_10_R1_EXECUTION_CALIBRATION.md)：测量前声明的协议及工具修正记录。
- 本报告：用中文更新并保留历史失败证据路径。

生产 C++/QML、planner/safety/H/E、frozen spec、冻结设计合同/技术设计及 Rover 参数修改：**无**。未尝试更广泛的重新设计。所有者允许的增量仅是 fixture、隔离、测量与证据。Task != Plan != Mission、计划域纯逻辑、backend upload gate、Marine v3 Task/Artifact 和普通 QGC .plan 兼容均保留。

## 3. 校准依据与唯一选择

协议最终 SHA-256：`66dc6f93d33dd49ae81b5a00174eb282ae72990cc3f1892065b77e4066631596`；v1 原稿保留在 preflight。第一次工具修正不改 E 公式、测量尺度或选择规则，CAL120 的执行前锚定错误原样保留；CAL20/CAL60 的前轮数据不参与最终聚合。工具修复后重新从零执行 CAL20/CAL60/CAL120，每次均为独立 fresh 实例。

最终测量 binary：`27da2e8fcb3485a4a5b4748125c99c64d20e713da3af788da2fed44e8432e48c`。最终有效测量集位于 [calibration-4](F:/Projects/qgroundcontrol/build/v05-10-r1/calibration-4)，三个场景均完整执行，初始 D0/Ready/质量 PASS/no repair，ACK accepted、ACTIVE、顺序推进、Complete；E=0 越界如实记录为测量数据，没有作为 M00/M04 正式验收 PASS。

- Dmax = `1.91198268019 m`：所有轨迹点到完整 canonical polyline 的最大最短距离。
- Vmax = `5.39045452629 m/s`；Tmax = `0.341 s`，满足预先声明的有限性、V≤6、T≤0.5 有效性门。
- 预先声明：`E = ceil((1.25×Dmax + Vmax×Tmax + 0.02m)/0.5m)×0.5m`。取整前为 `4.2481233437m`，故 **E=4.5m**。25% 偏移预留、一个未采样间隔移动距离及 0.02m 坐标量化/舍入依据均先于正式测量声明。
- `L = ceil10(max(40m,20E,4E²/1m)) = 90m`，`swath=0.75L=67.5m`；均在测量尺度及 E≤6m 的预先声明范围内。

没有搜索 E 到 PASS，也没有依据正式验收 PASS/FAIL 调整 E、尺寸、swath 或 N。E 用于 execution reserve，H 未替代 E。正式验收前的最终锁定记录为 [fixture-lock-final.json](F:/Projects/qgroundcontrol/build/v05-10-r1/fixture-lock-final.json)，时间 `2026-10-05T04:10:18.413978+00:00`；它晚于工具绑定校验修正、早于任何正式 acceptance。此前二进制锁定记录仍保留，未拼入最终验收。

## 4. 最终 canonical fixtures 与身份

### M00

`M00|C=rect(0,0,90,90)|N=C|O=[]|swath=67.5|H=0|P=0|E=4.5|req=Standard|sweep=Manual90|planner=Auto`

- fixture SHA-256：`b2c40fb313785b91467859e1342b409e4f94679e7dcd47f78451ba06e3701526`
- Task ID：`V05-10-R1-M00`；PlanningInputIdentity fingerprint：`0af6244c21d9dcc35130ef6a49b9535caf27ea61dff40ac3dbbfc0914948ecf7`
- Task JSON SHA-256：`3f90955a2d001826a1e785a8b6fa7fb4221cbc03b0ebe68d2fb5c2fceeb97036`
- 完整 IntegratedV05 Artifact/result JSON SHA-256：`f1c40f575e401fcf8678be286df05a6e215fb2f3af6d008acd9afdcea803327c`

### M04

`M04|C=rect(0,0,90,90)|N=rect(-5.5,-5.5,95.5,95.5)|O=[]|swath=67.5|H=0|P=8|E=4.5|req=Strict|sweep=Manual90|planner=Auto`

- fixture SHA-256：`e3acd31f0445498dfb722f1ed84c630d8b09c18c1e4f5b6776c213b42dd4d141`
- Task ID：`V05-10-R1-M04`；PlanningInputIdentity fingerprint：`cebdbf2bd82cd1dadcd0f909ff767ef1cd295c206bc5bf2e75370a37808828b4`
- Task JSON SHA-256：`9c3a2889d23037344e55363510d1cce2fad136bd57c705377b2bb8375493f0f3`
- 完整 IntegratedV05 Artifact/result JSON SHA-256：`edabc24b56524237fd45c93798fc5f2dd5aa1eca2ad11f77e4f8302ef2106efd`


M00 保持矩形、N=C、无 No-Go、Manual90、Auto→SimpleMonotone、Standard、初始候选 PASS、D0/Ready、InitialPolicyPass、repair attempted/applied=false、upload allowed。M04 的 N 边界为 −5.5..95.5m，C⊂N；preferred 满足质量要求的解不可行，hard 候选 Strict Complete/PASS，D1/ReadyWithWarning、PreferredSafetyViolated、upload allowed，未退化成 ReviewRequired。实际 Artifact 中两者均无修复。

Task、完整 Artifact/result 文件字节哈希以及 PlanningInputIdentity 在准备、正式 SITL 和随后自动重绑定阶段一致。只有同一最终二进制下的新 fixture 证据被纳入本轮结论。旧 20m/E=0 fixture PASS 仅保留为历史，未拼接。

## 5. Windows build 与最终 binary

使用项目标准 x64 VsDevCmd：`cmake --build build/P0-01-marine-debug --target QGroundControl --parallel 8`。Windows/MSVC x64 Debug、Qt 6.11.1，最终 build exit=0。

最终 QGroundControl.exe SHA-256：`529a7ba38ca66df1cd667c3fa9fb9d1d4d9c89aa08d2e95b5a65bda91383978d`。

[构建原始日志](F:/Projects/qgroundcontrol/build/v05-09-r2-repair/v05-10-r1-final-2.log)；[构建机器记录](F:/Projects/qgroundcontrol/build/v05-09-r2-repair/v05-10-r1-final-2-results.json)。本节二进制对应下面所有最终自动测试、UI 回归和正式 SITL；calibration 的测量二进制单独记录，不冒充 acceptance 二进制。

## 6. Focused、持久化与 Mission 回归

最终 20 个套件，共 **380 tests / 0 failures / 0 errors / 0 skips**；IntegratedPlanningResult 的 M00–M09 均通过，准备阶段真实 Task/Artifact 绑定也已通过。

| Suite | Tests | 结果 |
| --- | ---: | --- |
| IntegratedPlanningResultTest | 44 | PASS |
| AutoCoveragePlannerTest | 10 | PASS |
| SimpleMonotoneCoveragePlannerTest | 10 | PASS |
| SimpleMonotoneCapabilityTest | 6 | PASS |
| CoverageRepairTest | 12 | PASS |
| CoverageSafetyTest | 36 | PASS |
| CoverageQualityEvaluatorTest | 21 | PASS |
| ArduPilotMissionAdapterTest | 6 | PASS |
| MarineUploadGateTest | 24 | PASS |
| MarinePlanIntegrationTest | 3 | PASS |
| CoveragePlannerTest | 9 | PASS |
| PlanningInputIdentityTest | 9 | PASS |
| MarineTaskJsonTest | 60 | PASS |
| MarineTaskModelTest | 8 | PASS |
| MissionControllerTest | 20 | PASS |
| PlanMasterControllerTest | 71 | PASS |
| MissionItemTest | 13 | PASS |
| MissionSettingsTest | 4 | PASS |
| SimpleMissionItemTest | 12 | PASS |
| MissionControllerManagerTest | 2 | PASS |

正式 SITL 完成后，用同一 binary 再执行 IntegratedPlanningResultTest：**44/44，0 failure/skip**，绑定 [acceptance](F:/Projects/qgroundcontrol/build/v05-10-r1/acceptance) 保存的真实 Task/Artifact，验证重算 path 和同一输入身份、Codec round-trip、实际 Artifact 经 MissionAdapter 转换后的全部 NAV_WAYPOINT。未用旧 fixture 自动 PASS 替代。

Windows 自动 UI 回归：CoveragePlanViewUITest **6/6 PASS**，真实 PlanView/高度图路径保留；PlanViewUITest **6 tests / 2 failures**。MissionManagerTest **7 tests / 1 failure**。这三项剩余失败各有 V05-08 `4a948c8842a915fe3125322ec9eec9e66a007fb4` 原始 baseline XML 重现，失败属性逐项匹配：

- MissionManagerTest::_testErrorAckFailureStrings：同一默认中文消息产生 QString::arg missing argument 严格日志失败。
- PlanViewUITest::_testPlanViewStates：工具按钮预期 Return 与相同本地化文本不符。
- PlanViewUITest::_testRoverWaypointOnEmptyPlan：同一 Return 本地化断言失败。

[逐项 baseline 绑定](F:/Projects/qgroundcontrol/build/v05-10-r1/remaining-baseline-failures.json) 包含两端 XML、SHA、case、属性匹配及条件；baseline 原始 XML/环境在 [f3-baseline](F:/Projects/qgroundcontrol/build/v05-10/f3-baseline)，source commit 导出证明在 [baseline-source-manifest.json](F:/Projects/qgroundcontrol/build/v05-09-r2-repair/baseline-source-manifest.json)。不将未重现失败标为 pre-existing。初轮受限环境失败未记 PASS，原始日志/XML保留；纠正执行权限后重新完整执行，未增加忽略日志或降低严格检查。

自动测试原始结果：[v05-10-r1-focused-final-results.json](F:/Projects/qgroundcontrol/build/v05-09-r2-repair/v05-10-r1-focused-final-results.json)、[v05-10-r1-actual-binding-results.json](F:/Projects/qgroundcontrol/build/v05-09-r2-repair/v05-10-r1-actual-binding-results.json)、[v05-10-r1-ui-regression-results.json](F:/Projects/qgroundcontrol/build/v05-09-r2-repair/v05-10-r1-ui-regression-results.json)。

## 7. Fresh SITL hard acceptance

冻结 image 为 `qgc-ardurover-sitl:rover-4.7.0` / `sha256:001f20d07215138f2cb4aebc8eb691c8f4c3a23825a01f343b3f5397d262d3f0`；frame rover、home 47.397742,8.545594,488,0、speedup=1。WP_SPEED=5、WP_RADIUS=3、TURN_RADIUS=0.9、ATC_TURN_MAX_G=0.6、WP_ACCEL=0、WP_JERK=0，均只读核验，未改动。

每场景独立新建容器，无共享运行实例，不依赖 QTest 中末尾 cleanup。原有失败状态无法传给下一场景；finally 收集 inspect/log 后停止容器，失败尝试亦保留。

- M00：`ff0f7797d80fe093b372e420f4a6176ec081d94e8ce412c2194cc26381a8c4b1`，finally 停止已确认。
- M04：`b5761d7eaa177a1e4dc92e25d998e1d64b520447906aa897bbe81e04d0c7749d`，finally 停止已确认。

执行前完整状态记录：

| 场景 | Position lat/lon | Heading | Velocity | Armed / Mode | Mission |
| --- | --- | --- | --- | --- | --- |
| M00 | 47.397742, 8.5455939 | 356.92° | 0 / stationary | false / Manual | state=1, seq=0, count=0 |
| M04 | 47.397742, 8.5455939 | 356.94° | 0 / stationary | false / Manual | state=1, seq=0, count=0 |

| 场景 | 首 canonical waypoint anchor | 原始 GPI 轨迹点 | raw N 外点 / 最大越界 | hard acceptance |
| --- | --- | ---: | --- | --- |
| M00 | 3.95270380321e-09 m | 247 | 0 / 0 m | PASS |
| M04 | 3.95948195902e-09 m | 258 | 0 / 0 m | PASS |

M00/M04 均 planning/readiness 正确、upload allowed、Mission Start ACK accepted、MISSION_ACTIVE、全部 canonical waypoint 顺序推进、Mission Complete。轨迹持续观察到 Complete 后静止，未进入 No-Go，未观察到 RTL；实际绑定 Artifact 的 MissionAdapter 输出全部为 NAV_WAYPOINT。raw NavigationArea oracle 没有增加 tolerance、没有删除 outsideNavigation 检查，也没有将 FAIL 改成 warning。

M05/M08/M09-valid-D3/STALE **4/4 PASS**：真实 connected backend rejection；seeded remote Mission 完整保留，sendComplete、mission activity/progress 均 0，sync 不悬挂。为最大化隔离，reject 本轮也各用 fresh 容器；原 zero-mutation oracle 未变。

每次 MarineSITLValidationTest XML 中三个既有 legacy opt-in 方法保持原有 SKIP；本轮目标 `_validateV05Scenarios` 每次均实际执行且无 SKIP。没有把失败验证改为 SKIP。

## 8. 失败尝试、静态检查与 Git 审计

[failed-attempts.json](F:/Projects/qgroundcontrol/build/v05-10-r1/failed-attempts.json) 索引所有本轮编译、启动、锚定、绑定及权限失败；所有原始失败文件保留。前轮 V05-10 的 E=4/H=4 失败、20m/E=0 正式轨迹越界，以及独立审查 fresh M04 的 ~0.506m 越界仅作为历史问题证据，原报告完整快照保留在 preflight。没有覆盖或删除失败以制造通过。

- changed-range clang-format 22.1.8：PASS；新 header 全文件 PASS；[format-check.json](F:/Projects/qgroundcontrol/build/v05-10-r1/format-check.json)。
- Ruff check、Ruff format、Python compile、git diff --check：PASS，最终检查另存机器证据。
- 普通缓存 pre-commit：ENVIRONMENT BLOCKED，readonly database / PermissionError；hook 未执行，不记 PASS，未修复/换缓存/豁免 hook。[precommit.log](F:/Projects/qgroundcontrol/build/v05-10-r1/precommit.log)。
- Clazy：SUPPLEMENTAL STATIC-ANALYSIS SKIP（不可用），按 AGENTS。R1 无生产翻译单元变更，未宣称新增 production clang-tidy 检查。
- frozen spec、合同及技术设计与预检 SHA 相同；生产文件无变更；HEAD/branch 不变，index 为空。没有 stage/commit/push。

## 9. 停止门及未执行项

R1 指定两项 blocker 的 DoD 已满足，**OWNER_DESIGN_DECISION_REQUIRED=false**；正式固定 fixture 在冻结 Rover 环境中同时满足全部 M00/M04 要求。此为实施作者报告，等待独立审查，不是自我批准。

GUI manual acceptance：**NOT EXECUTED（所有者本任务明确排除）**。Windows 自动 UI 回归不能替代手工验收；整体 V05-10 DoD、software freeze 和 field acceptance 未宣称完成。P1 实船验证仍 DEFERRED / NOT EXECUTED。

下一步是 fresh independent implementation review；通过并获所有者后续授权后，继续 V05-10 GUI 手工验收。未启动 V05-11，未自动推进其他包。

总机器证据：[v05-10-r1-machine-evidence.json](F:/Projects/qgroundcontrol/build/v05-10-r1/v05-10-r1-machine-evidence.json)；最终不可覆盖的 fixture/source/binary 锁定：[fixture-lock-final.json](F:/Projects/qgroundcontrol/build/v05-10-r1/fixture-lock-final.json)；全部 raw SITL/tlog/CSV/GPI/container/XML 在 [acceptance](F:/Projects/qgroundcontrol/build/v05-10-r1/acceptance)。
