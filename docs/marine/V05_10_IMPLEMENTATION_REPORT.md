# V05-10 最终实施报告 — 软件冻结已批准并关闭

记录时间：2026-10-07T05:08:55.872090+00:00。Windows 是权威验收平台。本报告汇总已完成、已由所有者批准的 FC3A/FC3B，并做证据层冻结审计。本轮未运行构建、测试、SITL 或 GUI 验收，未修改产品、测试及冻结规范，未 stage/commit/push。

```text
FC3A = APPROVED
FC3B = APPROVED
E7 = APPROVED
E10-01 = OWNER-APPROVED DOCUMENTARY EXCEPTION
FZ10-01 = PASS — SATISFIED WITH APPROVED EXCEPTION
FZ10-02 = PASS
FZ10-03 = PASS — FINAL INDEPENDENT FREEZE REVIEW APPROVED
V05-10 CLOSED = YES
P2 v0.5 SOFTWARE FREEZE = APPROVED
V05-11 = NOT AUTHORIZED
FIELD VALIDATION DEFERRED BY FROZEN SPEC
```

审批来源是项目所有者在本会话的明确指示。FC3A/FC3B 已通过独立复核，最终独立冻结复核结论为 `APPROVE`；项目负责人随后明确批准 `V05-10 = CLOSED` 与 `P2 v0.5 SOFTWARE FREEZE = APPROVED`。原始 FC3A/FC3B、E7 v1/v2 与例外审批工件继续保留，不以本关闭记录覆盖历史证据。

## 1. 唯一源码、二进制与报告工作树身份

| 项目 | 最终绑定 |
| --- | --- |
| Branch | `feature/marine-p2-complex-coverage` |
| acceptance source HEAD | `f25ec3990cdeb5355680a8d9669a0f3e15373a26` |
| EXE SHA-256 | `751a8b8c6dea17520ee11cf1207b68c2189ab82203ea6b9840660b78fecb597b` |
| 原始 EXE | [QGroundControl.exe](F:/Projects/qgroundcontrol/build/P0-01-marine-debug/Debug/QGroundControl.exe) |
| 冻结 EXE | [QGroundControl.exe](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/final-candidate-f25ec3990-751a8b8c/QGroundControl.exe) |
| EXE size | 122940416 bytes |
| 原始/冻结 PDB SHA-256 | `d88e42f5b83bb4ea61b0dec52a4142ec39d8beb53a7e739f924492a5c835d7a5` |
| 冻结 manifest | [final-binary-manifest.json](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/final-candidate-f25ec3990-751a8b8c/final-binary-manifest.json) |
| FC3 independent fixture lock | [fc3-fixture-lock.json](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-fixture-lock.json)；SHA `d754867b16aaec4f53f3b75ccd8fdae232dc00fcc003c3be4402a7998e36f045` |

冻结和 FC3A/FC3B 验收时 source worktree/index 为 clean。E7 预检亦 clean。最终关闭阶段仅修改治理/证据文档：`AGENTS.md`、本报告和 `docs/marine/V05_10_CLOSURE_RECORD.md`；新增 machine closure evidence 位于忽略的 build 目录。该 docs-only closure delta 不属于已编译产品输入，接受的 runtime/source HEAD 仍为 `f25ec3990cdeb5355680a8d9669a0f3e15373a26`。

提交链：V05-08 `4a948c8842a915fe3125322ec9eec9e66a007fb4` → V05-09 `1a426c04004f7b4e2a2dbe961a824db67da06995` → V05-10 design freeze `4c1414622293bbb23da881ba1c8f4a76b9a1b9eb` → execution `cc70699c25f7f396907cf937c95f41daa9ff64b7` → R1 `945e2227a447b61a0f00d03b82675c2c51bf4b1f` → FC3R harness `f25ec3990cdeb5355680a8d9669a0f3e15373a26`。

旧 R1 acceptance binary `529a7ba38ca66df1cd667c3fa9fb9d1d4d9c89aa08d2e95b5a65bda91383978d`、FC2 candidate `0ed55fea4c5c1c28499ff5fa279af9c3a22e65304b781595a27a83589e53dfb8` 均仅作为历史证据。最终 binary 的测试、SITL 与 GUI 采用独立 FC3 证据，不拼接旧 binary PASS。[原 R1 实施报告完整快照](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/e7-support/V05_10_IMPLEMENTATION_REPORT.before-e7.md) 保留修改前的字节及历史上下文。

## 2. 文件变化与架构边界

V05-10 design freeze 至当前 HEAD 的已提交文件清单：

- `M	AGENTS.md`
- `M	custom/test/Marine/CoveragePlanViewUITest.cc`
- `M	custom/test/Marine/CoverageRepairTest.cc`
- `M	custom/test/Marine/IntegratedPlanningResultTest.cc`
- `M	custom/test/Marine/IntegratedPlanningResultTest.h`
- `M	custom/test/Marine/MarineSITLValidationTest.cc`
- `M	custom/test/Marine/MarineSITLValidationTest.h`
- `A	custom/test/Marine/V05ExecutionFixtures.h`
- `A	docs/marine/V05_10_EXECUTION_WRAPPER.md`
- `A	docs/marine/V05_10_IMPLEMENTATION_REPORT.md`
- `A	docs/marine/V05_10_R1_EXECUTION_CALIBRATION.md`
- `A	tools/simulation/v05_10_fc3.py`
- `A	tools/simulation/v05_10_r1.py`

上述范围没有生产 C++/QML 变更，也没有 active spec、冻结合同和技术设计变更。R1 仅修正 fixture/execution reserve 与 fresh 容器隔离；FC3R 仅修正显式运行时 source HEAD binding、独立 lock 及容器 namespace。Task != Plan != Mission、纯 domain planner、QML 展示层、MissionAdapter 转换职责和 plan-scoped context 均保持原架构。本次 E7 不做设计或语义变更。

E7 本轮 tracked 修改：`docs/marine/V05_10_IMPLEMENTATION_REPORT.md`；新增主要工件：`build/v05-10-r1/final-closure/e7-final-closure-evidence.json`、`e7-freeze-audit.json`、`E7_FINAL_CLOSURE_SUMMARY.md`。支持核验、报告快照与文档 diff 位于 `e7-support/`。历史 fixture lock、机器证据、GUI v1/BLOCKED 报告和失败尝试均不覆盖。

## 3. Windows build 与候选不变性

构建执行者为项目所有者，命令：

```text
cmd.exe /d /c build\v05-09-validation\build-current.cmd
等价：cmake --build build/P0-01-marine-debug --target QGroundControl --parallel 8
```

Windows x64 / Debug / Ninja / MSVC 14.51.36231（Visual Studio 18 BuildTools）/ Qt 6.11.1 msvc2022_64；link flags `/debug /INCREMENTAL`。所有者控制台的 `$LASTEXITCODE` 检查未抛出失败，exitCode=0；[控制台摘录](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/final-candidate-f25ec3990-751a8b8c/user-build-observation.txt) 和冻结 manifest 保留来源。**全量原始构建日志、实际构建开始/结束时间未归档**；只记录 EXE LastWriteTimeUtc `2026-10-06T02:26:16.5230862Z`，不将其冒充构建起止。该候选 byte reproducibility 未测试；最终证据通过固定字节副本绑定。本轮禁止重建并已遵守。

FC3A 35 份阶段身份记录、FC3B preflight/after-five/after-ten/closeout 身份门及 E7 当前只读哈希核验，原始与冻结 EXE 始终为 `751a8b8c6dea17520ee11cf1207b68c2189ab82203ea6b9840660b78fecb597b`。当前原始/冻结 PDB 亦相同。[E7 只读核验](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/e7-support/e7-readonly-evidence-audit.json) 记录逐项结果。

## 3.1 E10-01-EXC-01 正式文档例外审批

项目负责人授权原文为：“我授权正式例外审批通过，不重新启动整个验收流程。”

依据该有效授权，项目负责人批准 **E10-01-EXC-01**：最终 Windows 构建原始日志及准确起止时间缺失。处理方式为“保留缺口记录，不补造证据”；FC3A 机器验收和 FC3B GUI 验收继续有效，**不重新构建或测试**。审批原文已逐字归档于 [授权原文](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/e10-01-exc-01/authorization-verbatim.md)，正式记录见 [例外审批记录](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/e10-01-exc-01/exception-approval.json)。授权来源归属修正归档时间为 2026-10-07T06:34:35.839427+00:00，不冒充原授权签署时间或缺失的构建起止时间。此前误记的内容已归为 [助手解释性记录（非用户授权）](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/e10-01-exc-01/assistant-explanatory-record.md)；修正前快照明确标为错误归属历史记录，见 [来源修正历史索引](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/e10-01-exc-01/archival-correction-r1/historical-snapshots.json)。本次只纠正来源记录，无需重新审批，不改变验收结论。

```text
E10-01 = OWNER-APPROVED DOCUMENTARY EXCEPTION
FZ10-01 = SATISFIED WITH APPROVED EXCEPTION
FZ10-02 = PASS
FZ10-03 = PENDING FINAL INDEPENDENT REVIEW
```

该审批仅豁免指定文档缺口；**构建成功、二进制身份、安全性和功能验收要求不豁免**。原 build manifest 的 fullRawBuildLog、buildStartTimestamp 和 buildEndTimestamp 仍为 null，缺口 EL-01 保留；未补写原始日志或推测时间。EL-02–EL-05 及 KL-01/02 不在例外范围内。本审批不代表最终独立冻结批准，也不关闭 V05-10。

本次为 E7 证据 revision 2：[新版机器证据](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/e7-final-closure-evidence-v2.json)、[新版冻结审计](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/e7-freeze-audit-v2.json) 和 [新版收口汇总](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/E7_FINAL_CLOSURE_SUMMARY-v2.md)。原始 [E7 machine v1](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/e7-final-closure-evidence.json)、[E7 freeze audit v1](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/e7-freeze-audit.json)、[E7 summary v1](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/E7_FINAL_CLOSURE_SUMMARY.md) 及全部 e7-support 文件保持原字节。原 E7 report SHA `74260dad8b5918eded355c440e4c43585727f4ec8b4a80ed429ec6d12d3c83a3` 对应的完整内容归档于 [E7 v1 报告快照](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/e10-01-exc-01/V05_10_IMPLEMENTATION_REPORT.e7-v1.md)；v1 的报告路径记录属于当时状态，审阅旧版本时应核对该快照，而不是已更新的当前报告。

## 4. 固定 fixture 与 E calibration

独立 FC3 lock 绑定 `f25ec3990cdeb5355680a8d9669a0f3e15373a26` / `751a8b8c6dea17520ee11cf1207b68c2189ab82203ea6b9840660b78fecb597b`，E=4.5m、side=90m、swath=67.5m；R1 canonical 输入语义和两场景 SHA 未改变。M00 维持 rectangle、N=C、无 No-Go、Manual sweep、Auto→SimpleMonotone、Standard、初始 PASS、D0/Ready、repairApplied=false 和 upload allowed。M04 保持 C⊂N、preferred infeasible、hard-safe/Strict quality PASS、D1/ReadyWithWarning、PreferredSafetyViolated 和 upload allowed。

**M00**

```text
M00|C=rect(0,0,90,90)|N=C|O=[]|swath=67.5|H=0|P=0|E=4.5|req=Standard|sweep=Manual90|planner=Auto
canonical SHA-256 = b2c40fb313785b91467859e1342b409e4f94679e7dcd47f78451ba06e3701526
Task SHA-256 = 3f90955a2d001826a1e785a8b6fa7fb4221cbc03b0ebe68d2fb5c2fceeb97036
Artifact SHA-256 = f1c40f575e401fcf8678be286df05a6e215fb2f3af6d008acd9afdcea803327c
input fingerprint = 0af6244c21d9dcc35130ef6a49b9535caf27ea61dff40ac3dbbfc0914948ecf7
```

**M04**

```text
M04|C=rect(0,0,90,90)|N=rect(-5.5,-5.5,95.5,95.5)|O=[]|swath=67.5|H=0|P=8|E=4.5|req=Strict|sweep=Manual90|planner=Auto
canonical SHA-256 = e3acd31f0445498dfb722f1ed84c630d8b09c18c1e4f5b6776c213b42dd4d141
Task SHA-256 = 9c3a2889d23037344e55363510d1cce2fad136bd57c705377b2bb8375493f0f3
Artifact SHA-256 = edabc24b56524237fd45c93798fc5f2dd5aa1eca2ad11f77e4f8302ef2106efd
input fingerprint = cebdbf2bd82cd1dadcd0f909ff767ef1cd295c206bc5bf2e75370a37808828b4
```

E 是 waypoint tolerance/tracking/turn dynamics 的 execution reserve，没有以 H 代替。[已批准 R1 calibration 规则](F:/Projects/qgroundcontrol/docs/marine/V05_10_R1_EXECUTION_CALIBRATION.md) 的预先协议 SHA 为 `66dc6f93d33dd49ae81b5a00174eb282ae72990cc3f1892065b77e4066631596`。历史 deterministic measurement 得到 Dmax=1.91198268019m、Vmax=5.39045452629m/s、Tmax=0.341s；按预声明规则：

```text
E = ceil((1.25 × Dmax + Vmax × Tmax + 0.02m) / 0.5m) × 0.5m = 4.5m
L = ceil_to_10m(max(40m, 20E, 4E²/1m)) = 90m
swath = 0.75L = 67.5m
```

该推导是 calibration 历史依据，不冒充 final binary acceptance。E 在正式 acceptance 前固定，FC3 独立 preparation 重新生成 Task/Artifact 后建新 lock；没有按正式 PASS/FAIL 调 E。历史 R1 [fixture lock](F:/Projects/qgroundcontrol/build/v05-10-r1/fixture-lock-final.json) SHA 仍为 `b45de1f0d00de2e9c1f84ad71ec5b8b43b43917215ce06f94c78df0d85df836d`。

## 5. FC3A focused、绑定与 M00–M09

FC3A = APPROVED。[FC3A closeout machine evidence](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/closeout/fc3-machine-evidence.json) 与 [focused matrix 原记录](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/focused-matrix-results.json) 绑定 final binary，20 suites / 380 tests / 0 failures / 0 errors / 0 skips：

| Suite | Tests | Failures / Errors / Skips | 原始证据 |
| --- | ---: | --- | --- |
| IntegratedPlanningResultTest | 44 | 0 / 0 / 0 | [XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/focused-IntegratedPlanningResultTest-IntegratedPlanningResultTest.xml) |
| AutoCoveragePlannerTest | 10 | 0 / 0 / 0 | [XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/focused-AutoCoveragePlannerTest-AutoCoveragePlannerTest.xml) |
| SimpleMonotoneCoveragePlannerTest | 10 | 0 / 0 / 0 | [XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/focused-SimpleMonotoneCoveragePlannerTest-SimpleMonotoneCoveragePlannerTest.xml) |
| SimpleMonotoneCapabilityTest | 6 | 0 / 0 / 0 | [XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/focused-SimpleMonotoneCapabilityTest-SimpleMonotoneCapabilityTest.xml) |
| CoverageRepairTest | 12 | 0 / 0 / 0 | [XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/focused-CoverageRepairTest-CoverageRepairTest.xml) |
| CoverageSafetyTest | 36 | 0 / 0 / 0 | [XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/focused-CoverageSafetyTest-CoverageSafetyTest.xml) |
| CoverageQualityEvaluatorTest | 21 | 0 / 0 / 0 | [XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/focused-CoverageQualityEvaluatorTest-CoverageQualityEvaluatorTest.xml) |
| ArduPilotMissionAdapterTest | 6 | 0 / 0 / 0 | [XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/focused-ArduPilotMissionAdapterTest-ArduPilotMissionAdapterTest.xml) |
| MarineUploadGateTest | 24 | 0 / 0 / 0 | [XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/focused-MarineUploadGateTest-MarineUploadGateTest.xml) |
| MarinePlanIntegrationTest | 3 | 0 / 0 / 0 | [XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/focused-MarinePlanIntegrationTest-MarinePlanIntegrationTest.xml) |
| CoveragePlannerTest | 9 | 0 / 0 / 0 | [XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/focused-CoveragePlannerTest-CoveragePlannerTest.xml) |
| PlanningInputIdentityTest | 9 | 0 / 0 / 0 | [XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/focused-PlanningInputIdentityTest-PlanningInputIdentityTest.xml) |
| MarineTaskJsonTest | 60 | 0 / 0 / 0 | [XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/focused-MarineTaskJsonTest-MarineTaskJsonTest.xml) |
| MarineTaskModelTest | 8 | 0 / 0 / 0 | [XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/focused-MarineTaskModelTest-MarineTaskModelTest.xml) |
| MissionControllerTest | 20 | 0 / 0 / 0 | [XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/focused-MissionControllerTest-MissionControllerTest.xml) |
| PlanMasterControllerTest | 71 | 0 / 0 / 0 | [XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/focused-PlanMasterControllerTest-PlanMasterControllerTest.xml) |
| MissionItemTest | 13 | 0 / 0 / 0 | [XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/focused-MissionItemTest-MissionItemTest.xml) |
| MissionSettingsTest | 4 | 0 / 0 / 0 | [XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/focused-MissionSettingsTest-MissionSettingsTest.xml) |
| SimpleMissionItemTest | 12 | 0 / 0 / 0 | [XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/focused-SimpleMissionItemTest-SimpleMissionItemTest.xml) |
| MissionControllerManagerTest | 2 | 0 / 0 / 0 | [XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/focused-MissionControllerManagerTest-MissionControllerManagerTest.xml) |

正式 SITL Task/Artifact 已生成后，单独 `IntegratedPlanningResultTest` **44/44 PASS、零 failure/error/skip**。`QGC_V05_10_SOURCE_HEAD=f25ec3990cdeb5355680a8d9669a0f3e15373a26`；binding dir 指向本次 FC3 acceptance。[actual binding XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/actual-FC3-binding-IntegratedPlanningResultTest-IntegratedPlanningResultTest.xml) 保留 canonical identity、Task/Artifact codec round-trip、fingerprint/result identity、NAV_WAYPOINT 转换及 upload gate 断言。

| M 场景 | 实际语义结果（PASS） | Canonical SHA-256 |
| --- | --- | --- |
| M00 | Auto→SimpleMonotone；Standard PASS；D0 / Ready；无 repair；upload allowed | `b2c40fb313785b91467859e1342b409e4f94679e7dcd47f78451ba06e3701526` |
| M01 | 凹形单调目标仍 SimpleMonotone；round-trip PASS | `3f0f9aceb82cf3dd9f0ebc488cf100c96e3d862610d4a050564d86b98a59fb06` |
| M02 | 非单调拓扑升级 BCD；TargetNonMonotoneForSelectedSweep；来源 round-trip PASS | `7372f8486e1e3ec12447ba05ce553666a9487c540b8b7d406546c47892939d44` |
| M03 | N−C 只作合法 Transit；Coverage-role 在 T 内；不扩展覆盖义务 | `289f4330ce183cc6811f7450cf5e59df0ba939d63ea168724d8ccc2bdda2139d` |
| M04 | Strict PASS；硬安全候选 D1 / ReadyWithWarning；PreferredSafetyViolated；upload allowed | `e3acd31f0445498dfb722f1ed84c630d8b09c18c1e4f5b6776c213b42dd4d141` |
| M05 | Failed / DiagnosticOnly / D2；仅 raw diagnostic route；canonical 为空；拒绝上传 | `98fda72f67f30797e74244c2f5ccebc4ada0b9845a3ed89897a7f58931a9a6ae` |
| M06 | 相同几何和指标；Standard Acceptable/PASS，Strict Insufficient；仅策略判定不同 | `24a97a4708f316f457f1acfde827f26541f2643412bac2f16a5f45ab4c55986e` |
| M07 | repair attempted/applied；AppliedPolicyPass；Success / ReadyWithWarning；允许上传 | `47de0de79db47251ac34af0a0bd41d48f26ea701603bb380bd10884e83dada9d` |
| M08 | 修补耗尽仍 Insufficient；Success / ReviewRequired；保留 hard-safe canonical/residual；无 Auto 升级；拒绝上传 | `d59601e14d74fb4f1559f215229bb0e0d045e28dd7ecf7ddcc14efbaeb96b8b3` |
| M09 | 有效不支持：Failed / DiagnosticOnly / D3，无 raw route；非法：InvalidInput / None，无 canonical | `e116c1ed81aa5837d202254d757d5accf1786eb85efd31a1957f4c4c466bdbe7` |

M06 是相同几何的双策略 evaluator 组合，没有伪造独立 Task/Artifact。其冻结 canonical 文本中的 `simple-monotone.v1` 保留为历史场景标签；真实策略语义使用 source 常量 `simple-monotone.v2`，两者在机器审计中区分。

## 6. Windows regression 与 baseline attribution

| Suite | Tests | Raw Failures / Errors / Skips | 原始证据 |
| --- | ---: | --- | --- |
| CoveragePlanViewUITest | 6 | 0 / 0 / 0 | [当前 XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/regression-CoveragePlanViewUITest-CoveragePlanViewUITest.xml) |
| MissionManagerTest | 7 | 1 / 0 / 0 | [当前 XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/regression-MissionManagerTest-MissionManagerTest.xml) |
| PlanViewUITest | 6 | 2 / 0 / 0 | [当前 XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/regression-PlanViewUITest-PlanViewUITest.xml) |

三个 raw FAIL 如实保留：

- `MissionManagerTest::_testErrorAckFailureStrings`：当前与 baseline 的 failure attributes、正文精确一致；[当前 XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/regression-MissionManagerTest-MissionManagerTest.xml) / [基线 XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/baseline-MissionManagerTest-MissionManagerTest.xml)。
- `PlanViewUITest::_testPlanViewStates`：当前与 baseline 的 failure attributes、正文精确一致；[当前 XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/regression-PlanViewUITest-PlanViewUITest.xml) / [基线 XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/baseline-PlanViewUITest-PlanViewUITest.xml)。
- `PlanViewUITest::_testRoverWaypointOnEmptyPlan`：当前与 baseline 的 failure attributes、正文精确一致；[当前 XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/regression-PlanViewUITest-PlanViewUITest.xml) / [基线 XML](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/baseline-PlanViewUITest-PlanViewUITest.xml)。

归因为 **PRE-EXISTING / UNRELATED** 的依据是本次 FC3A 在 V05-08 baseline `4a948c8842a915fe3125322ec9eec9e66a007fb4` 的具体重现，而非“以前失败过”。baseline EXE SHA `2b387f3d44563dcc086b95e1fa36146808aca1e2560dfda764b6b4c9a13373ff`，source export manifest、双侧 raw XML/log、执行命令/环境及 11 项 Debug/compiler/Qt/testing/linker 条件相同的对照均在 FC3A 证据中。E7 再次只读比较 failure 属性和正文，精确一致；未发现新增未归因失败。不能将以上 regression 表写成零失败。

## 7. Fresh SITL 完整 hard acceptance

冻结 image `qgc-ardurover-sitl:rover-4.7.0`，ID `sha256:001f20d07215138f2cb4aebc8eb691c8f4c3a23825a01f343b3f5397d262d3f0`；frame rover；home `47.397742,8.545594,488,0`；TCP `127.0.0.1:5760`；speedup=1；seed=N/A。WP_RADIUS=3、WP_SPEED=5、TURN_RADIUS=0.9、ATC_TURN_MAX_G=0.6、WP_ACCEL=0、WP_JERK=0。未改变 Rover 环境。参数快照中的浮点编码值仍原样保留。

两次 preparation 与六次正式 acceptance 各有 fresh container，正式 acceptance 的六个 ID 不同：

| 场景 | 结果 | Fresh container ID | 原始记录 |
| --- | --- | --- | --- |
| M00 | PASS | `32477608c44b81ae4134b2ce6a15f923b117aaf182a98949c8d4dc128cfe4b16` | [run](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/fc3-acceptance-e095c71edfbee1a24370/M00/run.json) |
| M04 | PASS | `a50ea4adc3bb8efceda1efd03eddae0b9e5d2a4695a345e3249bde43e978be50` | [run](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/fc3-acceptance-e095c71edfbee1a24370/M04/run.json) |
| M05 | PASS | `ffeeb63b9eb494dd9a457f2e61d9632c7349df305e9b1dcf6e90d4ad84ae2f7d` | [run](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/fc3-acceptance-e095c71edfbee1a24370/M05/run.json) |
| M08 | PASS | `4062bbf81c83be0e0630f42b48eceb2ba086e47161f919462b33eb681d85673e` | [run](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/fc3-acceptance-e095c71edfbee1a24370/M08/run.json) |
| M09 | PASS | `74d44ea9b9c32ce358350c8e6dc9a99ff1ad8b1bc328107df6638a7640f528b8` | [run](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/fc3-acceptance-e095c71edfbee1a24370/M09/run.json) |
| STALE | PASS | `2ee15036b26cec5443f731eceef68116e1f16a6fb7e6f584da7543539e8a44a7` | [run](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-machine-751a8b8c-r2/fc3-acceptance-e095c71edfbee1a24370/STALE/run.json) |

所有初始状态都有 position、heading、velocity/stationary、armed、mode、mission 字段：位置 `47.397742,8.5455939`、speed=0、stationary=true、armed=false、Manual、mission state=1/seq=0/items=0。M00 heading=357.42°，M04=356.96°；逐场景完整值见 run/initial state 原文件。

| Allow 场景 | Readiness / Tier | Position samples | raw N 外点 / 最大越界 | 首点 anchor / tolerance |
| --- | --- | ---: | --- | --- |
| M00 | Ready / D0 | 247 | 0 / 0m | 3.95270380320582e-09m / 0.001m |
| M04 | ReadyWithWarning / D1 | 258 | 0 / 0m | 3.9594819590180776e-09m / 0.001m |

M00/M04 均 planning/readiness 正确、uploadAllowed=true、Mission Start ACK accepted、MISSION_ACTIVE observed、全 canonical waypoint 顺序到达、Mission Complete、enteredNoGo=false、marineGeneratedRtl=false。实际轨迹在 raw NavigationArea 内；没有增加 NavigationArea tolerance，也没有降低 WP_SPEED、改变 WP_RADIUS/TURN_RADIUS 或删检查。Ingress 只在冻结的首 canonical waypoint 共址容差内验证，不把未认证 ingress 混入安全声明。

M05/M08/M09-valid-D3/STALE 各走真实 connected backend 拒绝链：uploadAllowed=false，backendRejected=true，sendCompleteSignals=0、missionActivitySignals=0、missionProgressSignals=0，remoteMissionPreserved=true、syncInProgress=false。拒绝未清空或部分发送 seeded remote Mission。

每份 SITL raw XML 的 `_validateP2Scenarios`、`_diagnoseS04Execution`、`_analyzeExecutionSafety` 三个历史辅助方法仍 SKIP；真正 `_validateV05Scenarios` 每次实跑 PASS。它们没有替代本轮验收，也没有把失败转换成 SKIP。旧环境阻断、失败尝试、容器日志/inspect、tlog、CSV 与 raw XML 保留原目录。

## 8. FC3B Windows GUI 与 provenance

FC3B = APPROVED，10/10 PASS、0 FAIL、0 INCOMPLETE。原始数据在 [fc3b-machine-evidence-v2.json](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-gui-751a8b8c/fc3b-machine-evidence-v2.json) 和 [GUI 结果 v2](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-gui-751a8b8c/manual-gui-results-v2.md)。M00 由 Codex 在前轮实际 GUI 操作，M04 前段亦由 Codex 操作；林远山完成接续及其余场景，并明确确认全部符合预期、无未完成及异常。E7 没有重复执行 GUI。

| 场景 | 结果 | 已记录观察 | 本轮工件 |
| --- | --- | --- | --- |
| M00 | PASS | Success / Ready / D0；无修补；4 canonical 航点 | [manifest v2](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-gui-751a8b8c/M00/scenario-manifest-v2.json) |
| M04 | PASS | Success / ReadyWithWarning / D1；4 canonical 航点 | [manifest v2](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-gui-751a8b8c/M04/scenario-manifest-v2.json) |
| STALE | PASS | Unplanned / None / stale；未 Generate | [manifest v2](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-gui-751a8b8c/STALE/scenario-manifest-v2.json) |
| NORMAL | PASS | ordinary Mission 8 航点；无 Marine task | [manifest v2](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-gui-751a8b8c/NORMAL/scenario-manifest-v2.json) |
| TERRAIN | PASS | ordinary Mission 4 航点；Profile 正常 | [manifest v2](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-gui-751a8b8c/TERRAIN/scenario-manifest-v2.json) |
| M05 | PASS | Failed / DiagnosticOnly / D2；canonical 为空 | [manifest v2](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-gui-751a8b8c/M05/scenario-manifest-v2.json) |
| M07 | PASS | Success / ReadyWithWarning / D1；修补应用；13 航点 | [manifest v2](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-gui-751a8b8c/M07/scenario-manifest-v2.json) |
| M08 | PASS | Success / ReviewRequired / D0；13 航点；拒绝上传 | [manifest v2](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-gui-751a8b8c/M08/scenario-manifest-v2.json) |
| M09-invalid | PASS | InvalidInput / None / None；canonical 为空 | [manifest v2](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-gui-751a8b8c/M09-invalid/scenario-manifest-v2.json) |
| M09-valid | PASS | Failed / DiagnosticOnly / D3；canonical 为空 | [manifest v2](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-gui-751a8b8c/M09-valid/scenario-manifest-v2.json) |

各场景 Save As / 关闭重启同一候选 / Open 已保存计划 / 不再次 Generate 的证据已归档。STALE 当前 runtime 是 Unplanned/None/stale；文件保留的旧 Ready Artifact 不等于当前可执行 Ready。GUI 上传按钮观察不替代 FC3A backend gate oracle。

TERRAIN 收尾已完成：[terrain-archive-audit-v2.json](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-gui-751a8b8c/TERRAIN/terrain-archive-audit-v2.json) 确认真实 ordinary Mission、4 个普通航点、无 Marine task、Save As/reload 和可见正常 Profile；[FC3B-TERRAIN.plan](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-gui-751a8b8c/TERRAIN/FC3B-TERRAIN.plan) SHA `775eba336e9e99445c77cf8a5ca3fa44e4b3d63a012f2183c08d3a8830fba35a` 与 M00 不同。历史疑似 M00 副本未篡改为本轮证据。

M09-valid [provenance-manifest-v2.json](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3-gui-751a8b8c/M09-valid/provenance-manifest-v2.json) 明确区分：

| 来源层 | 实际 SHA-256 |
| --- | --- |
| FC3A raw acceptance Task | `3ace710a39347506dce56848e9242a842770121d037e1e12d9210a4b5908e8ef` |
| FC3A raw acceptance Artifact | `7f2dcfd721b40f767ee4cfcd939870fea557a25bbbcd3208ebd923780bad5a97` |
| archived FC3 Task / Artifact sidecars | 分别与上述 raw 字节 SHA 相同 |
| pre-GUI plan | `fc86959e5b51517517b5456b3ba8b3fe10a2f530fc2169e92037c6f98ab6aa6d` |
| 本轮 final GUI plan | `d9ae1f394ac3740f07e6ec0a56cdc3f9d07f375b5eaf062e4f6ee58aca0ed644` |
| historical post-GUI plan | 同为 `d9ae1f394ac3740f07e6ec0a56cdc3f9d07f375b5eaf062e4f6ee58aca0ed644`；仅历史身份 |

本轮 M09-valid plan 的实际字节仍与历史相同，未制造新 SHA。新来源链使用当前截图、保存时间、操作者确认及 final-candidate raw Task/Artifact 的全部字段精确比较。M07/M09-invalid 也有保存计划与历史字节相同的备注；M07 input-only.plan 当前字节改变但 Task 语义与冻结输入及最终计划相同，初始 provenance 不改写。

人工时间按所有者授权使用截图/.plan 的 LastWriteTime 范围；不称为独立记录的实际点击时间。具体人工后端连接类型未单独记录。11 张早期截图为 JPEG 字节但扩展名 .png，其余 34 张为 PNG；保留原字节，不重编码。45 张图包括历史中断黑屏，黑屏不是成功截图。旧 BLOCKED 报告与早期不完整 manifest 不覆盖；后续 APPROVED 状态由 E7 独立记录。

## 9. Frozen semantics、静态检查与冻结审计

Marine extension v2、Task v3、Artifact v3；planning=`p2.v0.5.planning.2`；quality=`coverage-quality.v1`；CAL-01=0.99、CAL-02=0.50m；area tolerance=`max(0.01m², 1e-6×A)`；Auto=`auto.v1`、SimpleMonotone=`simple-monotone.v2`、BCD=`bcd.v0.5.v2`、global sweep=`global-sweep.v1` 均保留。M00/M04 的 requested planner=Auto、resolved SimpleMonotone、Manual90、fingerprint 与来源数据见独立 lock/Artifact；其他解析/版本及 persistence/exact restore/invalidations 见实际绑定测试。

[e7-freeze-audit-v2.json](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/e7-freeze-audit-v2.json) 包含冻结合同逐条 implementation/code/tests/raw-evidence trace，以及 M00–M09 canonical identities。检查 C/N/O、H/P/E、整段 hard certification、D0–D3、质量/availability、候选字典序、条件 repair/provenance、Task/Artifact exact restore、fingerprint stale、diagnostic 非可执行、真实后端 gate、普通 .plan 与 height-profile 路径。活动规范、冻结合同/设计相对 design-freeze HEAD 无变化。既有 InfrastructureOnly/旧 schema 测试只验证拒绝行为，当前冻结要求不依赖过渡 Artifact 获取可上传状态；transitional dependency=NO。

静态检查是已记录结果的继承，不是本轮重跑：R1 changed-range clang-format、direct Ruff check/format、Python compile PASS；[FC3R-R1 harness 检查](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/fc3r-r1-review/fc3r-r1-review-evidence.json) Python syntax、Ruff check/format、changed-range format、17/17 offline probes、git diff/JSON PASS，当前源文件 SHA 与检查绑定一致。普通缓存 pre-commit 为 ENVIRONMENT BLOCKED（readonly database/PermissionError），不记 PASS；Clazy 为 SUPPLEMENTAL STATIC-ANALYSIS SKIP（不可用）。本轮无新的 production clang-tidy 检查。

| 冻结项 | E7 证据审计结论 |
| --- | --- |
| FZ10-01 | SATISFIED WITH APPROVED EXCEPTION（仅 EL-01 指定构建文档缺口由 E10-01-EXC-01 批准；其余要求仍保持） |
| FZ10-02 | PASS（冻结语义与测试证据逐条绑定；无未批准语义更改、无未归因回归） |
| FZ10-03 | PASS — FINAL INDEPENDENT FREEZE REVIEW APPROVED |

FZ10-01/02 的 trace audit 已完成；FZ10-03 已由最终独立冻结复核批准。项目负责人随后明确批准 `V05-10 = CLOSED` 与 `P2 v0.5 SOFTWARE FREEZE = APPROVED`。

## 10. 两项保留问题与停止状态

[既有已知问题记录](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/V05_10_KNOWN_LIMITATIONS.json) 保留原样：

- **KL-01 OPEN / NOT FIXED：重复 Clear 导致右侧面板消失。** 属 GUI 状态/模型生命周期缺陷；既有机制分析仍为候选根因。本轮不复现、不修复，不将问题标已关闭。
- **KL-02 OPEN / NOT FIXED：M07 整边界修补造成路径冗余。** 质量与硬安全满足冻结场景，13 canonical 航点及 AppliedPolicyPass 已留证；完整边界分量增加路径与转弯。E7 不改变 repair 架构，也不自行授权优化。

两项沿用既有记录的 FC3B 非阻断分类并继续保持 OPEN / NOT FIXED；它们不阻断本次冻结批准，但后续修复或优化必须另行授权。当前没有新增 product corrective。

P1 real-USV 为 DEFERRED AND NOT EXECUTED；**FIELD VALIDATION DEFERRED BY FROZEN SPEC**。设计、Windows、SITL、GUI 通过均不提升为 field acceptance。V05-10 软件冻结已获批准；停止，不执行 V05-11。

最终证据：[e7-final-closure-evidence-v2.json](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/e7-final-closure-evidence-v2.json)；收口汇总：[E7_FINAL_CLOSURE_SUMMARY-v2.md](F:/Projects/qgroundcontrol/build/v05-10-r1/final-closure/E7_FINAL_CLOSURE_SUMMARY-v2.md)。E7 只读核验共验证 FC3A index 292 文件、FC3B index 148 文件、历史 GUI 42 文件；当前 binary/PDB 匹配；JSON/XML 可解析；`git diff --check` PASS；index empty。文档差异及最终机械记录见 `e7-support/`。

本次例外审批更新的只读核验见 `build/v05-10-r1/final-closure/e10-01-exc-01/`；不重建、不重验收，原验收证据保持不变。当前唯一 tracked 修改仍为本报告，index 为空。

## 11. Final Owner Closure

最终独立冻结复核结论：`APPROVE`。FZ10-03 = `PASS — FINAL INDEPENDENT FREEZE REVIEW APPROVED`。

项目负责人最终授权原文：

> 我明确批准：V05-10 = CLOSED
> P2 v0.5 SOFTWARE FREEZE = APPROVED

最终关闭状态：`V05-10 = CLOSED`；`P2 v0.5 SOFTWARE FREEZE = APPROVED`。该关闭不改变已接受的 runtime/source HEAD `f25ec3990cdeb5355680a8d9669a0f3e15373a26` 与 binary SHA-256 `751a8b8c6dea17520ee11cf1207b68c2189ab82203ea6b9840660b78fecb597b`。后续仅文档性质的 closure commit 不属于接受的运行时源码身份。

E10-01-EXC-01 继续作为仅限构建日志/准确起止时间缺失的项目负责人批准文档例外；KL-01 与 KL-02 继续 OPEN / NOT FIXED；`FIELD VALIDATION DEFERRED BY FROZEN SPEC`；`V05-11 = NOT AUTHORIZED`。
