# V05-10 技术设计：验收、集成、SITL、人工验证与冻结审计

状态：FROZEN DESIGN — 对应已批准的 `V05-10-DC-v1`；不构成 V05-10 execution authorization、软件冻结或 field acceptance。

## 1. 目的

本文件把 P2 v0.5 冻结语义、冻结的 V05-08/V05-09 合同、当前提交中的 API 与测试证据，整理为 V05-10 可执行的验证方案。V05-10 的范围是 M00–M09、回归、持久化、Mission 集成、SITL、人工验收和冻结审计。

本轮仅完成只读审计和设计文档。没有运行构建、测试、SITL 或人工验收；没有修改 C++、QML、测试、Task v3、Artifact v3 或冻结合同。实际 V05-10 执行仍须经过独立设计审查及单独的实施授权。

## 2. 权威与基线

### 仓库启动状态

| 项目 | 记录 |
| --- | --- |
| 工作区 | qgroundcontrol，F:\Projects\qgroundcontrol |
| 分支 | feature/marine-p2-complex-coverage |
| HEAD | 1a426c04004f7b4e2a2dbe961a824db67da06995 |
| 工作树 | 任务开始前基线干净；当前仅新增本文件与 V05_10_DESIGN_CONTRACT.md 两份未跟踪设计文档，暂存为 0；无生产代码/测试改动 |
| 相对上游 | ahead 2，behind 0；本轮未同步或重写分支 |
| V05-08 提交 | 4a948c8842a915fe3125322ec9eec9e66a007fb4 |
| V05-09 提交 / 当前 HEAD | 1a426c04004f7b4e2a2dbe961a824db67da06995 |
| 本轮授权 | 仅 V05-10 机械审计与两份设计文档；不启动实现或 V05-11/P3 |

### 权威顺序

1. P2 v0.5 冻结规范，尤其 §§5–34、36–37。
2. 冻结的 V05-08-DC-v1（所有者提供的合同附件）及冻结的 V05-09-DC-v1。
3. 当前提交中的生产代码和测试。
4. V05-08/V05-09 实施报告、独立审核、机器证据；这些是证据，不覆盖冻结语义。

V05-08 合同附件的源文件为所有者提供的 V05-08 Design Contract — Integrated Planning Result, Diagnostics, Persistence and Upload Gate，合同身份 V05-08-DC-v1。它没有作为独立文件跟踪在 docs/marine；仓库内跟踪的 V05-08 文档是 V05_08_IMPLEMENTATION_REPORT.md。其最终审核证据位于 build/v05-08-r2-independent-review/REVIEW.md 与 review-evidence.json。R2 审核结论为 APPROVED FOR COMMIT，并关闭 F5；闭包提交为 4a948c8。V05-08 实施报告保留了先前“等待独立复核”的历史段落，阅读时以最终审核和闭包提交为后续证据。

V05-09 冻结设计/合同为 V05_09_TECHNICAL_DESIGN.md 和 V05_09_DESIGN_CONTRACT.md。实现报告记录 R2 实施交接、F1/F3 证据及待独立实现复核；当前 HEAD 提交信息是 V05-09 闭包提交。V05-10 接受此处明示的闭包状态，不把报告中的历史交接句改写成新的复核结论。

### 已读治理和设计文件

CODING_STYLE.md、.github/CONTRIBUTING.md、test/README.md、.github/ci-overview.md、tools/README.md、P2_COMPLEX_COVERAGE_SPEC_v0.5_ADAPTIVE_PLANNING_AMENDMENT.md、所有者提供的 V05-08-DC-v1 附件、V05_08_IMPLEMENTATION_REPORT.md、V05-09 合同/技术设计/实施报告，以及 V05-08/V05-09 机器证据和当前 API/测试代码。

## 3. 当前架构清单

本表记录当前声明和职责，不是正确性判定。

| 组件 | 当前源码 | API / 关键成员 | 当前职责 | 冻结关联 | 已有测试 |
| --- | --- | --- | --- | --- | --- |
| MarineTask | custom/src/Marine/MarineTask.h | MarineTask::region/coverage/safety/planner/sensors；schemaValid()、isValid() | Task v3 规划输入、schema 与域有效性 | §§6–9、28 | MarineTaskModelTest、MarineTaskJsonTest |
| PlanningInputIdentity | custom/src/Marine/PlanningInputIdentity.h/.cc | fromTask()、matches()、matchesSupported()、toJson()/fromJson() | 规范化规划输入、语义身份和 SHA-256 指纹 | §§29–30 | PlanningInputIdentityTest |
| PlanningResult | custom/src/Marine/Planning/PlanningResult.h | status、error、outcome、path、legRoles、metrics、plannerSource | 地理坐标集成结果，规范几何与元数据 | §§11–15、29 | IntegratedPlanningResultTest、CoverageTaskAdapterTest |
| MissionReadiness / SafetySolutionTier | custom/src/Marine/Planning/PlanningOutcome.h | MissionReadiness、SafetySolutionTier；PlanningOutcome::readiness/tier | 保存 readiness 与安全/诊断层级 | §§12–14 | IntegratedPlanningResultTest、CoveragePresentationTest |
| CoverageQuality 模型 | custom/src/Marine/Planning/CoverageQualityEvaluator.h | CoverageQualityStatus、CoverageEvaluation、CoverageQualityAvailability、residual | Standard/Strict 评估、指标/残余可用性 | §§18–21 | CoverageQualityEvaluatorTest、IntegratedPlanningResultTest |
| Repair provenance | custom/src/Marine/Planning/PlanningOutcome.h | SelectedRepairProvenance、AppliedRepairComponent | 保存所选结果实际修补尝试/分量及前后评估 | §§22–24、29 | CoverageRepairTest、IntegratedPlanningResultTest |
| Issues / suggestions | custom/src/Marine/Planning/PlanningOutcome.h | PlanningIssue、PlanningSuggestion、typed PlanningReference | 稳定码、级别、说明和有类型引用 | §§25–26 | IntegratedPlanningResultTest、CoveragePresentationTest |
| DiagnosticCandidate / Overlay | custom/src/Marine/Planning/PlanningOutcome.h | DiagnosticCandidate、DiagnosticOverlay、SafetyLegClass | 分离原始空间候选和说明图层 | §§13–14、29 | IntegratedPlanningResultTest |
| PlanningArtifact / Codec | custom/src/Marine/PlanningArtifact.h；PlanningArtifactCodec.h/.cc | Artifact v3；save/load/validateResult/matchesCurrentInput/uploadAllowed | 结构校验、精确恢复、过期判断、唯一后端门禁 | §§27–30 | IntegratedPlanningResultTest、CoverageComplexItemTest |
| CoverageTaskAdapter | custom/src/Marine/Planning/CoverageTaskAdapter.h/.cc | buildProblem()、toPlanningResult() | Task 地理输入转本地问题以及结果坐标转换 | §§7–10、20–21 | CoverageTaskAdapterTest |
| IntegratedPlanningResult | custom/src/Marine/Planning/IntegratedPlanningResult.h/.cc | publishCanonicalOutcome()、publishDiagnosticOutcome()、publishUnresolvedOutcome()、populatePlanningAdvice() | 发布已选结果、诊断与建议；不重新排序或评估 | V05-08 合同 §§18–21、27 | IntegratedPlanningResultTest |
| SimpleMonotone planner | custom/src/Marine/Planning/SimpleMonotoneCoveragePlanner.h/.cc | SimpleMonotoneCoveragePlanner::plan() | 单调目标的简单往复式规划 | §§15–16 | SimpleMonotoneCoveragePlannerTest、AutoCoveragePlannerTest |
| BCD planner | custom/src/Marine/Planning/BoustrophedonCoveragePlanner.h/.cc | BoustrophedonCoveragePlanner::plan() | 从 CoverageTarget 分解并在活动区域内组装/路由 | §§15、17 | BoustrophedonCoveragePlannerTest、BoustrophedonDecompositionTest |
| Auto planner | custom/src/Marine/Planning/AutoCoveragePlanner.h/.cc | AutoCoveragePlanner::plan() | 能力/拓扑解析 Simple 或 BCD，并记录来源 | §15 | AutoCoveragePlannerTest |
| CoverageInspectionComplexItem | custom/src/MissionManager/CoverageInspectionComplexItem.h/.cc | C/N/O editor properties、plan()/invalidatePlan()、planningPresentation、resultStale、uploadAllowed、appendMissionItemsForUpload() | QGC Task/Artifact 集成和展示适配 | §§27–31、V05-09 DC | CoverageComplexItemTest、CoveragePresentationTest |
| CoveragePlanningPresentation | custom/src/MissionManager/CoveragePlanningPresentation.h/.cc | planningPresentation(result,current,stale) | 把已发布事实转成 QVariantMap；不评估/认证 | §31、V05-09 DC R1–R13 | CoveragePresentationTest |
| ArduPilotMissionAdapter | custom/src/Marine/Mission/ArduPilotMissionAdapter.h/.cc | appendWaypoints(artifact,currentTask,...)；裸结果重载 fail-closed | 将通过门禁的规范路径转换为航点 | §27 | ArduPilotMissionAdapterTest、IntegratedPlanningResultTest |
| VisualMissionItem | src/MissionManager/VisualMissionItem.h | readyForSaveState()、appendMissionItems() 及 V05-09 checked-upload 虚函数 | 通用视觉任务项上传/保存扩展点 | 上游 QGC + V05-09 U1–U7 | MissionControllerTest、PlanMasterControllerTest |
| MissionController | src/MissionManager/MissionController.h/.cc | aggregate upload admission、checked append/send conversion | 整体 Mission 上传预检和原子转换 | V05-09 DC U1–U7 | MissionControllerTest、MarineUploadGateTest |
| PlanMasterController | src/MissionManager/PlanMasterController.h/.cc | plan 文件读取结果、发送序列与 Mission/围栏/集合点流程 | checked-load 和整计划 send 编排 | V05-09 DC U3–U7 | PlanMasterControllerTest、MarineUploadGateTest |
| PlanView / toolbar | src/PlanView/PlanView.qml、PlanToolBarIndicators.qml | PlanView Upload 回调与工具栏可用状态 | 入口展示；调用时仍由 C++ 后端复检 | §§27、31；V05-09 U5 | CoveragePlanViewUITest、PlanViewUITest |
| TerrainStatus | src/PlanView/TerrainStatus.qml | terrainStatusChart；axisX/axisY 与可见 TerrainProfile | 共享 Mission 高度/地形图 | V05-09 R2 地形回归 | CoveragePlanViewUITest、PlanViewUITest |
| TerrainProfile | src/QmlControls/TerrainProfile.h/.cc | updateSeries()；_addTerrainPoints/_addFlightPoints/_addMissingPoints/_addCollisionPoints | 从 MissionController/FlightPathSegment 生成曲线 | V05-09 R2 F1 根因修复 | CoveragePlanViewUITest、PlanViewUITest |
| MarineSITLValidationTest | custom/test/Marine/MarineSITLValidationTest.cc | _analyzeExecutionSafety、_validateP2Scenarios、_diagnoseS04Execution | 旧 P2-13/P2-13E 的可选 ArduRover SITL/分析夹具 | §§33、36；历史协议边界 | MarineSITLValidationTest |

API 对照要点：结果生命周期 Planned 与 readiness 分离；Artifact 持有 IntegratedV05/InfrastructureOnly 合同身份；上传唯一真值是 PlanningArtifactCodec 接收 Artifact 和当前 Task 的 uploadAllowed()；QML presentation map 不拥有结果语义。V05-08 明确保留 InfrastructureOnly 过渡 Artifact：它不能升级成当前集成结果、不能上传，必须显式重规划。

## 4. 已有验证清单

分类含义严格使用 DIRECT、PARTIAL、INDIRECT、NOT COVERED。DIRECT 表示断言冻结语义；仅有构建、注册、源代码搜索或序列化成功不算 DIRECT。表内是当前测试代码/报告所显示的覆盖，不是本轮执行结果。

| 测试套件 / 用例 | 实际经过的产品行为 | 冻结条款 | 覆盖强度 | 缺少的判定条件 |
| --- | --- | --- | --- | --- |
| AutoCoveragePlannerTest：rectangle/concave capability/escalation/Auto sweep | 实际 Auto 策略解析、升级和扫描模式 | §§15–17、M00–M02 | DIRECT（策略分支）；PARTIAL（M 端到端） | Artifact 精确恢复和完整 QGC 路径由其他套件承担 |
| SimpleMonotoneCoveragePlannerTest：navigation outside target、preferred/hard ranking、AssessmentError | 真实规划候选、导航 Transit 与候选排序 | §§8、12、16、24 | DIRECT（各断言）；PARTIAL（M03/M04 集成） | M03 的完整任务、保存/加载和 Mission 路径未由单个场景覆盖 |
| BoustrophedonCoveragePlannerTest / DecompositionTest | 目标/导航分离、分解、No-Go、确定性和失败传播 | §§7–10、17 | PARTIAL | 覆盖目标各分量的最终质量与上传需组合测试 |
| CoverageQualityEvaluatorTest / NominalCoverageValidatorTest | round-cap Coverage 足迹、角色、残余、数值容差、策略 | §§18–21 | DIRECT（评估器语义） | 完整产品边界要由 M06 和集成结果复核 |
| CoverageRepairTest：AssessmentError/initial pass/incremental/exhausted/order | 真实修补候选生成、评估、安全和终止条件 | §§22–24、M00/M07/M08 | DIRECT（修补语义） | 从实际 Task/Artifact 到 UI/Mission 的组合证据 |
| IntegratedPlanningResultTest：T01–T20 | Ready/D1/Review/Error/D2/D3、issues、建议、provenance、stale、精确恢复、门禁 | §§11–15、25–30 | DIRECT（V05-08 合同 T1–T20） | M00–M09 并非由 T1–T20 全部替代；M09 某些案例为 publication 单元场景 |
| PlanningInputIdentityTest：planning fields/canonical geometry/non-planning/semantics | 每个规划字段和语义身份的指纹变化 | §30 | DIRECT | Artifact/ComplexItem 当前状态需与 T14 联合验证 |
| MarineTaskJsonTest / MarineTaskModelTest | Task v3 字段、schema、字段校验、角度规范化 | §§6、28 | DIRECT（Task codec/model） | 任务与 Artifact 一起保存/加载由集成套件验证 |
| CoverageTaskAdapterTest | Task 输入地理转换、独立 C/N/O、路径映射/结构 | §§7–10、14、20–21 | DIRECT（adapter） | 实际 UI/Plan 生命周期端到端 |
| CoverageComplexItemTest / MarinePlanIntegrationTest | QGC complex item、Task/Artifact roundtrip、旧 schema 拒绝、plan 文件 | §§28–30 | DIRECT（已断言生命周期）；PARTIAL（全部生产链） | 与上传、SITL、人工结果展示仍需独立验证 |
| ArduPilotMissionAdapterTest / MarineUploadGateTest | 后端允许/拒绝、转换、失败原子性、文件加载拒绝 | §27、V05-09 U1–U7 | DIRECT（adapter/受控 Mission 场景） | 结果矩阵须与新 M00–M09 fixture 同二进制绑定 |
| CoveragePresentationTest：result matrix/repair/stale/editor/holes/palettes | 实际 QML 实例化、结果展示、保存事实映射、QGeoPolygon holes、编辑过期 | §31、V05-09 T09 | DIRECT（被断言的 map/property 语义） | 最终二进制的人工可读性/渲染截图不是单元断言 |
| CoveragePlanViewUITest：actual upload/confirmation | 实际 PlanView、工具栏、回调、拒绝状态、地形剖面 | §§27、31；V05-09 T09/Terrain | DIRECT（交互/上传与轴/图可见）；PARTIAL（未知海拔样本的单独语义） | 缺失地形 marker 的独立数值 oracle |
| MissionControllerTest / PlanMasterControllerTest / MissionManagerTest | 上游 Mission、计划文件、发送序列、mission manager | P0/P1/QGC regression、V05-09 U | INDIRECT（Marine gate 之外的回归） | 必须用当前 build 重跑；不能按旧结果判定当前 |
| PlanViewUITest：_testSaveAsMenu 等 | 实际上游窗口、Save As 与 terrain graph 路径 | P0/P1/V05-09 terrain | DIRECT（Save As 产品回归） | 对剩余 localization FAIL 单独记录，不弱化严格日志 |
| MarineSITLValidationTest | 旧 P2-13 场景/分析/S04 执行诊断 | §33 历史边界 | PARTIAL（旧协议回归） | 不证明 v0.5 的 M 矩阵、Artifact readiness 或当前提交的 v0.5 SITL |

## 5. M00–M09 矩阵

场景谓词和输出按规范 §32 原义。没有规范冻结的绝对坐标不作为判定条件。表中“保存”要求遵从 Artifact v3 身份匹配/过期规则。

证据组合规则：每个 M00–M09 必须有命名且固定的 canonical scenario/fixture identity 与 hash。允许 planner、evaluator、persistence、UI、Mission 多层共享证据，但所有用于同一 Mxx 结论的证据必须绑定同一 scenario/Task identity，需要时绑定同一 Artifact/result identity，并绑定同一最终接受 source HEAD / executable SHA-256。不得把不同输入的零散测试拼成某个 M 场景 PASS。

| 场景 | 冻结输入条件 | PlanningStatus | MissionReadiness | Tier | Canonical path | Diagnostic candidate | 覆盖期望 | Repair | Upload | Issues / suggestions | Persistence | 现有 fixture / 缺口 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| M00 简单矩形 | C 矩形，N=C，无 O，固定 Manual，H/P/E 有效，Standard，初始候选过优选安全和 Standard | Success | Ready | D0 | 保留，整段硬安全/优选安全 | 无 | 初始候选通过 | repairApplied=false；不得造周界航线 | 允许 | 不得因场景本身发覆盖失败码；包含 IngressNotAssessed | 匹配 Artifact v3 精确恢复；无重算 | T12、Auto rectangle、T09-01 可组合；仍需冻结 fixture 身份和 M00 语义单测 |
| M01 凹但单调 | 无 O；凹形 T 对固定扫描角单调且 SimpleMonotone 适用 | Success | 按评估/优选层级决定 | D0/D1（若硬安全 canonical 存在） | 保留硬安全候选 | 仅在无硬安全候选时才可有 | 全 T 评估 | 仅可靠 Insufficient 才能修补 | 仅 Ready/Warning 可上传 | 不得仅因凹形触发 PlannerEscalated | 匹配恢复；规划输入/策略变更过期 | Auto _testConcaveMonotoneResolvesSimpleMonotone；缺 M01 完整 Artifact/QGC 组合 |
| M02 非单调目标 | 有效受支持拓扑；固定角下需要分解 | Success 或实际求解 Failed | 依规范矩阵 | D0/D1，失败可 D2/D3 | 有则保留硬安全路径 | 只有无硬安全路径才允许 | 使用完整 T、同一评估器 | 仅可靠 Insufficient | Ready/Warning 可上传 | Auto→BCD，原因/来源可见，必要时 PlannerEscalated | source/resolved strategy/identity 一致保存 | Auto _testCapabilityEscalationDelegatesToBcd；缺 M02 端到端 v0.5 fixture |
| M03 N 大于 C | C⊂N；夹具确实用到 C 外导航空间 | 有硬安全候选时 Success | 依覆盖与 tier | D0/D1 | 连接/转弯可离 C，但在活动区域内 | 不替代 canonical | 只评估 T=C−O；不得系统覆盖 N−C | 条件触发 | Ready/Warning 按 gate | 不因 N−C 形成覆盖义务 | 输入变化使 artifact stale | Simple planner 有 NavigationOutsideTarget/Transit 用例；缺全链路 M03 |
| M04 优选不可行/硬安全可行 | 无满足优选/场景要求的解；存在硬安全且覆盖通过候选；还须覆盖“优选候选覆盖失败不能压过硬安全通过候选” | Success | ReadyWithWarning | D1 | 保留 | 无 | 可靠评估策略通过 | 不修改 H/P/E；按具体 Insufficient 资格规则 | 允许，warning 保留 | PreferredSafetyViolated；建议仅按既有码产生 | 精确保留 D1 来源/identity | T02 + SimpleMonotone hard-pass priority；缺单 fixture 同时锁定 M04 优先级与 Artifact |
| M05 硬安全不可行/有原始诊断 | 无 D0/D1；RawNavigationFreeSpace 有真实连续 route | Failed | DiagnosticOnly | D2 | 必须为空 | 独立保存，真实连续，位于 N−O 且不得过 O | 不作为 canonical 覆盖认证 | 不修补诊断路径 | 禁止；后端拒绝且不编码 | UnsafeDiagnosticCandidate 与具体安全说明 | diagnostic 可保存/精确恢复，不变为 canonical | T06/T17 覆盖 D2 和 gate；缺 M05 命名 fixture 的完整 Plan/Mission 组合 |
| M06 Standard 过、Strict 不过 | 对相同 T/path/roles/Swath；有限边界残余满足 Standard 而不满足 Strict | Success | 按选中要求，Strict 为 ReviewRequired | D0/D1 | 几何相同并保留 | 无 | CAL-01 0.99、CAL-02 0.50m；数值容差不改变 | 按选中策略；过策略即不修补 | 仅所选策略 PASS 的 Ready/Warning 可上传 | Strict 的 CoverageBelowRequirement 只在 Insufficient 时出现 | policy identity coverage-quality.v1；策略变化使旧结果 stale | CoverageQuality/Repair 有分支测试；缺 M06 几何、policy 双评估和 Artifact 绑定场景 |
| M07 修补成功 | 初始硬安全候选可靠 Insufficient；存在安全且能改善缺口的目标相对修补分量 | Success | 由最终偏好层决定 Ready 或 ReadyWithWarning | D0/D1 | 最终修补候选保留 | 无 | 每次只加一个完整安全分量、重评；通过即停 | attempted=true、applied=true，保留分量/前后评估/路径/转弯事实 | PASS 后按最终 D0/D1 gate | CoverageRepairApplied；InspectRepairCost 可按确定规则出现 | repair provenance 精确往返 | RepairTest、T11 与 T09-08；缺一条 M07 Task→Artifact→UI→Mission 全链路 |
| M08 修补仍不足 | 硬安全候选存在；受支持优选/硬层级候选与修补均未通过 | Success | ReviewRequired | D0/D1 | 保留最佳硬安全 canonical 与残余 | 无 | 最终可靠 Insufficient | 仅在初始该候选可靠 Insufficient 时尝试；穷尽即保留 | 禁止 | CoverageBelowRequirement；尝试后可有 CoverageRepairInsufficient | ReviewRequired/残余精确恢复且不重算 | T03/T09-03 覆盖保留/拒绝；需证实策略通过候选搜索已穷尽 |
| M09 不支持/未解决原始拓扑 | 原始拓扑不受支持或无有意义受支持的 raw connection | 有效输入求解：Failed；无效输入：InvalidInput | DiagnosticOnly 或 None | D3（有效求解诊断）；invalid 的 tier 只按当前结果结构保存，不改变 None gate | 空 | 可以为空；不能伪造连接路线 | 无策略通过结论 | 不尝试修补非法/无路输入 | 禁止，后端 gate 必须拒绝 | 结构化问题/overlay解释原因 | 诊断事实可恢复；invalid/malformed schema 不得执行 | T07 为 D3 publication，报告说明部分场景是 publication 单元；缺生产 planner 拓扑端到端负例 |

## 6. 持久化与过期验证

| 验证条件 | 预期恢复状态 | Planner 调用 | Evaluator 调用 | Router 调用 | Repair 调用 | Upload |
| --- | --- | --- | --- | --- | --- | --- |
| Task v3 roundtrip，所有必需字段 | Task v3 值精确往返；拒绝未知必需枚举/缺失字段 | 0 | 0 | 0 | 0 | 按当前 Artifact 决定 |
| 匹配的 IntegratedV05 Artifact v3 | 保持保存的路径顺序、就绪、tier、质量可用性、diagnostic/issues/suggestions/provenance | 0 | 0 | 0 | 0 | 仅合法 current Success+Ready/Warning |
| Artifact v3 identity mismatch | stale；current result 清空为 Unplanned/None；不自动重规划 | 0 | 0 | 0 | 0 | 拒绝 |
| malformed Artifact v3 | 明确校验错误；无 current executable result | 0 | 0 | 0 | 0 | 拒绝 |
| Task/Artifact v1/v2、旧无 role/identity/readiness | 明确不支持开发期 schema；不猜测/迁移 | 0 | 0 | 0 | 0 | 拒绝 |
| 普通上游 .plan，无 Marine extension | 由上游 QGC 正常加载/保存 | 0（Marine） | 0 | 0 | 0 | 保留既有普通 Mission 行为 |
| AssessmentError 已知/未知可用数据 | 状态和每项 availability 原样恢复；不可用值仍 missing/null | 0 | 0 | 0 | 0 | 拒绝 |
| canonical/diagnostic/overlay/残余/leg roles | 各类型与几何命名空间分离，引用有效 | 0 | 0 | 0 | 0 | 诊断和残余不编码 |
| issues/suggestions/repair provenance/source | 稳定码、顺序、关联和实际 before/after 事实保持 | 0 | 0 | 0 | 0 | 由最终 readiness 决定 |
| InfrastructureOnly transition Artifact | 原样可保留的过渡语义；Unplanned/None；不升级 IntegratedV05 | 0 | 0 | 0 | 0 | 拒绝；显式 replan 才能生成新当前结果 |
| policy/strategy/planning semantic 不支持 | stale 并清空 current result；没有隐式重评 | 0 | 0 | 0 | 0 | 拒绝 |

### 规划身份字段矩阵

| 字段 | 指纹包含 | 当前位置 | 已有 stale/identity 测试 | 预期 |
| --- | --- | --- | --- | --- |
| CoverageArea C | 是 | PlanningInputIdentity::fromTask / Task.region.coverageBoundary | PlanningInputIdentityTest::_testPlanningFields、_testCanonicalGeometry | 改变即 stale |
| NavigationArea N | 是 | 同上 / navigationBoundary | 同上 | 改变即 stale |
| 每个 No-Go O | 是，规范顺序编码 | 同上 / noGoRegions | _testPlanningFields、CoverageTaskAdapterTest | 内容变化即 stale；表达轮换/方向遵循身份 canonicalization |
| Swath | 是 | 同上 / coverage.swathWidthM | _testPlanningFields | 改变即 stale |
| H | 是 | 同上 / safety.hardSafetyMarginM | _testPlanningFields | 改变即 stale |
| P | 是 | 同上 / safety.preferredSafetyMarginM | _testPlanningFields | 改变即 stale |
| E | 是 | 同上 / planner.executionSafety.executionMarginM | _testPlanningFields | 改变即 stale |
| coverage requirement | 是 | 同上 / coverage.coverageRequirement | _testPlanningFields | Standard/Strict 改变即 stale |
| CAL / coverage policy semantic identity | 是 | PlanningSemantics.policyVersion；当前 coverage-quality.v1 | _testSemanticIdentity、_testSupportedSemanticMatching | 不支持的策略语义 stale |
| sweep mode | 是 | coverage.sweepAngleMode | _testPlanningFields、_testNonPlanningFields | Manual/Auto 切换即 stale |
| Manual sweep angle | 生效时是，按 180° 归一 | fromTask() 只在 Manual 写入归一角 | _testPlanningFields、_testCanonicalGeometry | Manual 方向语义变化 stale；等价 180° 表示保持身份 |
| Auto sweep 角值 | 不生效时不进入角度字段；mode 仍进入 | fromTask() 的 manual 分支 | _testNonPlanningFields | 语义遵从 Auto；不以未使用角度值造成失效 |
| requested planner ID | 是 | Task.planner.plannerId | _testPlanningFields、IntegratedPlanningResultTest::_testPlannerSourceBinding | 改变即 stale |
| resolved strategy ID/version | 是 | PlanningSemantics / PlannerStrategyIdentity | _testPlanningAndResolvedSemanticFingerprints、_testSupportedSemanticMatching | 不支持版本 stale；Auto 也保存实际解析身份 |
| planning semantic version | 是；当前 p2.v0.5.planning.2 | CoverageStrategySemantics.h | _testSemanticIdentity | 版本变化 stale |
| Task name、sensor display/record 配置、纯 UI editor selection | 否 | Task.name/sensors；ComplexItem editingRegion | _testNonPlanningFields、CoveragePresentationTest::_testIndependentEditingAndStale | 改变保留匹配规划产物 |

## 7. D0–D3 结果矩阵

| Tier | 硬安全 canonical | Preferred-safe | Raw diagnostic route | canonical leg class | diagnostic leg class | Status / readiness | Upload | Issues / persistence / UI |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| D0 | 必须存在；每段在 HardExecutionTrackRegion | 全段也在 PreferredExecutionTrackRegion | 不用于替代 canonical | 仅 PreferredSafe | 无 | 策略 PASS→Success/Ready；可靠 Insufficient 或 AssessmentError→Success/ReviewRequired | 仅 Ready 可上传 | 保留覆盖/来源；UI 显示规范路径 |
| D1 | 必须存在；全段在 HardExecutionTrackRegion | 至少一段未满足优选区 | 不用于替代 canonical | PreferredSafe 与 HardSafeWarning | 无 | PASS→Success/ReadyWithWarning；Insufficient/Error→Success/ReviewRequired | 仅 ReadyWithWarning 可上传 | PreferredSafetyViolated 警告；UI保留 Coverage/Transit 区分 |
| D2 | 不存在 | 不适用 | 存在 RawNavigationFreeSpace 内真实连续路径 | 空 | 可含 PreferredSafe、HardSafeWarning、ExecutionUnsafe、HardUnsafe；不得跨 O | Failed/DiagnosticOnly | 禁止 | UnsafeDiagnosticCandidate；独立红色 NOT EXECUTABLE 路径；Artifact 不将其作为 Mission |
| D3 | 不存在 | 不适用 | 无有意义的受支持连续 route，或为不支持类别 | 空 | 无 route；overlay 仅说明 | 有效输入 Failed/DiagnosticOnly；非法输入 InvalidInput/None | 禁止 | 有结构化理由/可用 overlay；不绘制伪连线，load 状态按保存结果精确还原 |

MissionReadiness 与 tier 独立；D2/D3 不能进入 MissionAdapter。规范路径每段必须通过整个线段安全判定；仅端点检查不够。

## 8. CoverageQuality 验证矩阵

| Quality 状态 | 可靠 | 所选策略通过 | CoverageBelowRequirement | CoverageAssessmentFailed | Repair | Canonical / readiness / upload | 指标与残余 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Complete | 是 | 是 | 否 | 否 | 不允许；若初始候选已 PASS 不修补 | 硬安全路径保留；D0 Ready 或 D1 ReadyWithWarning；允许上传 | 成功评估数据及可用残余按 codec 保存 |
| Acceptable | 是 | 仅 Standard 接受；Strict 不接受该结果 | 否 | 否 | Standard 通过时不允许 | Standard 下按安全 tier；Strict 对同一结果必须成为 Insufficient | 不得改变相同 T/path/roles/Swath 的评估几何或数值容差 |
| Insufficient | 是 | 否 | 必须 | 否 | 可触发；只能使用该候选可靠得到的 uncovered target | 有硬安全路径则 Success/ReviewRequired，保留路径/残余，禁止上传 | 完整 metrics/residual 可用于策略与条件修补 |
| AssessmentError | 否 | 不可判定 | 禁止 | 必须 | 禁止；指标/残余不得作为修补基线 | 有独立 hard-safe 路径则 Success/ReviewRequired 并保留路径；否则按失败/无效输入语义；禁止上传 | 可用事实逐项标 available；未知值不得填零、空几何或通过状态 |

统一公式按原始 T 及 Coverage-role legs 的 round-cap swath/2 footprint；Transit、T 外面积不得计入。数值容差固定为 max(0.01 m², 1e-6 × A)。v0.5 §§18–21、37 冻结 policy structure 与 calibration gate；其后 owner approval 已由 AGENTS.md §7 与 V05-04 closure 将 CAL-01=0.99、CAL-02=0.50m 和策略身份 coverage-quality.v1 冻结。关键核心退化须执行规范 §21 的逐分量 Strict fallback；无效 inset 属 AssessmentError。

## 9. 候选排序与 Coverage Repair

### 候选排序

| 顺序 | 判定键 | 规则 | 审计证据 |
| --- | --- | --- | --- |
| 1 | 硬安全 | 违反 H 或 E 的候选不得成为 canonical；安全优先级不可由覆盖率、距离或转弯数抵消 | Planner 安全分类、IntegratedPlanningResultTest、Mission gate |
| 2 | 覆盖评估类别 | 策略通过优于可靠 Insufficient，可靠 Insufficient 优于 AssessmentError | 同一评估输入的 evaluator 状态及选中结果 |
| 3 | 同一可靠失败类别内的质量 | 依规范 q(critical uncovered)、q(uncovered) 词典序；AssessmentError 没有质量排序键 | CoverageQualityEvaluatorTest 与候选对照夹具 |
| 4 | Preferred tier | 仅在前述安全/质量比较相同后优选 preferred-safe | D0/D1 分类与候选来源 |
| 5 | 路由成本 | 规范路径长度较短者优先，其后 turns 较少者优先 | 规范化几何上的长度和 turns |
| 6 | 稳定决胜 | 稳定候选键保证相同输入重复规划得到相同胜者 | 固定输入的确定性重复运行 |

当所有候选均为 AssessmentError 时，不读取不可用质量数值；依次比较 Preferred tier、路径长度、turns 和稳定键。任何实现若让有可靠策略通过结果被 AssessmentError 或可靠失败候选压过，即为阻断失败。

| 顺序 | 当前实现位置 | 已有测试 | 本轮设计的缺失 oracle |
| --- | --- | --- | --- |
| 硬安全候选资格 | custom/src/Marine/Planning/CoverageSafety.cc；SimpleMonotoneCoveragePlanner.cc；BoustrophedonCoveragePlanner.cc | CoverageSafetyTest；SimpleMonotoneCoveragePlannerTest | 确认跨策略最终 winner 均不含不安全 canonical segment |
| PASS > Insufficient > AssessmentError | custom/src/Marine/Planning/SimpleMonotoneCoveragePlanner.cc 的 qualityCategory/candidateIsBetter；BoustrophedonCoveragePlanner.cc 的 qualityCategory/candidateBetter | SimpleMonotoneCoveragePlannerTest 的 hard-pass priority；CoverageRepairTest 的 Error 隔离 | 显式 AssessmentError 候选对照及全部候选 Error 时的分支证据 |
| Insufficient 内 q(critical)、q(uncovered) | CoverageQualityEvaluator.cc 的 compareCoverageQuality；候选 comparator 调用处 | CoverageQualityEvaluatorTest；SimpleMonotoneCoveragePlannerTest | planner 级候选冲突同时涉及关键与普通缺口时的 winner oracle |
| Preferred tier | SimpleMonotoneCoveragePlanner.cc；BoustrophedonCoveragePlanner.cc | SimpleMonotoneCoveragePlannerTest：hard pass outranks preferred insufficient | 关键质量相同但 D0/D1 对比的候选组合；确认不提前偏好 D0 |
| path length、turns | PlanningPathMetrics.cc；两种 planner 的 candidate comparator | SimpleMonotoneCoveragePlannerTest；BoustrophedonCoveragePlannerTest | 分别构造仅 length 冲突及仅 turns 冲突的候选 |
| deterministic final order | SimpleMonotoneCoveragePlanner.cc orientationOrder；BoustrophedonCoveragePlanner.cc generationOrder | BoustrophedonCoveragePlannerTest deterministic；M00 同输入重复规划 | 对 Auto/各 resolved strategy 同输入完整结果重复运行，包含完全相等候选 |

### Coverage Repair 排序代码与现有证据

| 顺序 | 当前实现位置 | 已有测试 | 缺失 oracle |
| --- | --- | --- | --- |
| 只从 Error-free Insufficient 进入 | custom/src/Marine/Planning/CoverageRepair.cc 的 repairCoverageCandidate 初始准入 | CoverageRepairTest::_testAssessmentErrorAndInitialPass | 与实际 planner 一起证明初始 PASS、Error、D2/D3、invalid 均为零次 repair |
| hard-safe、active-region segment、完整评估 | CoverageRepair.cc 的 buildRepairTrial；CoverageSafety.cc；CoverageQualityEvaluator.cc | CoverageRepairTest successful/incremental、hard safety 分支 | 逐 trial 的 evaluator 调用、排序前过滤条件和未选 trial 排除理由 |
| 先验证相对当前候选 strict improvement，再在 improved trials 中比较完整重评后的 quality，最后比较 legal transition cost | CoverageRepair.cc 的 `evaluateCoverageRepairTrial` + `coverageRepairTrialBetter` | CoverageRepairTest::_testFiniteInsufficientAndNoUsefulRepair、排序用例 | 明确区分 §23 第 2 层“相对 current 必须改善”和第 3 层“improved trials 之间的质量排名”；仅当 trial quality 等价时才比较 `transitionCostM` |
| 稳定 component/entry/direction | CoverageRepair.cc 的 coverageRepairTrialBetter（componentId、entryIndex、reverse） | CoverageRepairTest::_testStableTrialKeys、_testRepairDeterminism | transitionCost 相同后依次 componentId、entryIndex、direction；完全相同时重复运行结果稳定 |
| 每步之后重新循环，PASS 即结束 | CoverageRepair.cc 的 repairCoverageCandidate | CoverageRepairTest::_testSuccessfulIncrementalRepair | 多个改善分量先后达到 PASS 时证明后续分量不再尝试 |
| provenance before/after 与最终结果一致 | IntegratedPlanningResult.cc；CoverageRepair.cc；PlanningArtifactCodec | CoverageRepairTest、IntegratedPlanningResultTest、CoveragePresentationTest | 最终 repair evaluation 与 canonical 最终 evaluation 对应并经 Artifact/UI 往返 |

### 修补排序与准入

冻结 §23 的完整决策顺序不得压缩或重排：1) safety eligibility；2) 相对当前候选必须按同一 CoverageQuality comparator strict improvement；3) 在所有 eligible improved trials 中按完整重评后的 quality 排名；4) 更低 legal transition cost；5) stable component ID；6) entry index；7) direction。第 2 层是准入过滤，第 3 层是候选间排序，两者不得合并。

修补只可由可靠 Insufficient 触发；AssessmentError、初始 PASS、无硬安全 canonical 的 D2/D3、非法输入均不得修补。每次增量只纳入一个完整且硬安全的目标相对分量，重新运行完整质量评估；第一次达到所选策略 PASS 即停止。修补分量来自原始 CoverageTarget 的 uncovered residual，不把 NavigationArea 或 No-Go 当作补充目标。

| 修补决策键 | 规则 | 必须保留的事实 |
| --- | --- | --- |
| 安全适配 | 硬安全和执行安全先行；不能降低 H/P/E | 每个 trial 的 safety fit 与淘汰原因 |
| 有效改进 | 按规范 q(critical)、q(uncovered) 严格改进；仅变化噪声或改善面积但恶化更高优先级指标不算改进 | 修补前后完整可用评估 |
| 完整重评 | 每次采用同一 CoverageQualityEvaluator 与 policy identity；PASS 后立即终止 | 所选策略、状态、metrics 和 residual |
| 路由代价 | 合格 trial 中比较路由代价 | 路径长度、turns |
| 稳定选择 | 最后按分量稳定 ID、entry index、direction 决胜 | 选中 trial 的稳定键 |
| Provenance | 只报告最终所选候选的实际尝试和应用状态 | attempted/applied、component、入口/方向、before/after 与尝试结果 |

若尝试后仍 Insufficient，保留最优硬安全 canonical、residual 和 ReviewRequired，上传拒绝；不能用成功规划状态掩盖覆盖缺口。

## 10. Auto planner 与升级决策

| 输入/能力 | Auto 解析 | 输出来源和行为 | v0.5 审计 oracle |
| --- | --- | --- | --- |
| 简单矩形或简单凹形单调目标，且能力满足 | SimpleMonotone | 仅覆盖 C−O；安全层级/质量由统一后续流程决定 | M00、M01；AutoCoveragePlannerTest 与产品集成 |
| 固定手动方向下为非单调/需分解拓扑 | BCD | 从 CoverageTarget 分解；依活动区域进行连接和排序，记升级原因 | M02；解析身份和 PlannerEscalated 说明 |
| 空/无效任务几何或安全输入 | 不应以升级替代输入校验 | InvalidInput/None 或规范定义的具体无效结果，不构造伪 route | M09 非法分支 |
| N 大于 C | 不因较大的 NavigationArea 擅自产生覆盖带 | 目标评估仍为 C−O；连接/转弯只能在许可运动域中 | M03 |
| 无可靠 Simple 解且能力要求升级 | 只按规定能力/拓扑条件升级 BCD | 保留 requested planner、resolved strategy/version 和原因 | M02 与 stale identity |

Auto、Simple、BCD 统一使用 C/N/O、H/P/E 和所选质量要求；不得通过结果层再做隐式重规划、重排序或覆盖评估。解析结果应在 Artifact 和 planning identity 中留下可审计的 resolved strategy/version。

| 核对点 | 当前源码/API | 已有证据 | V05-10 oracle / 缺口 |
| --- | --- | --- | --- |
| requested planner 与 resolved strategy/source | AutoCoveragePlanner.cc::sourceInfo/plan；PlannerSourceInfo::requestedPlannerId/resolvedStrategy/resolutionStatus/resolutionReason/escalated | AutoCoveragePlannerTest 的 Simple/BCD/source 行为；PlanningInputIdentityTest strategy identity | M00–M02 保存并精确恢复来源、版本和原因 |
| Auto planner 与 Auto sweep 相互独立 | CoverageProblem.sweepAngleMode；AutoCoveragePlanner.cc；SimpleMonotoneCoveragePlanner.cc::directSource | AutoCoveragePlannerTest 的 Manual/Auto sweep 与 selected angle 用例 | 分别改变两个 Auto 输入，证明一个不会隐式改变另一个 |
| 修补失败本身不升级策略 | AutoCoveragePlanner.cc 先做 capability/topology resolution，再向 resolved planner 委派；repair 在 planner 内部 | 当前 AutoCoveragePlannerTest 中未发现明确“repair exhausted 后不升级”断言 | 冻结 §15 已明确：repair exhaustion / coverage insufficiency alone 不授权 Auto 升级。V05-10 必须补 oracle：若 capability/topology 解析为 SimpleMonotone，则 repair exhausted 后仍保持该 resolved strategy，可进入 ReviewRequired，但不得改写为 BCD 或伪造 PlannerEscalated |
| Auto sweep 选角与稳定性 | AutoCoveragePlanner.cc 的 selectedSweepAngleDeg；CoverageStrategySemantics | AutoCoveragePlannerTest repeated Auto result 与 Manual exact angle | 同输入选角重复一致并符合固定 180° 归一约定；M00–M02 固定 Manual |

| 请求场景 | requested planner | resolved strategy | resolution status/reason | escalated | requested sweep / selected sweep | 核验来源 |
| --- | --- | --- | --- | --- | --- | --- |
| M00 矩形 | Auto | SimpleMonotone | Resolved / None | false | Manual / 固定 fixture 角度 | AutoCoveragePlannerTest rectangle + M00 integration |
| M01 凹但单调 | Auto | SimpleMonotone | Resolved / None | false | Manual / 固定 fixture 角度 | AutoCoveragePlannerTest concave-monotone |
| M02 需分解 | Auto | BCD | Resolved / topology/capability reason，例如 TargetNonMonotoneForSelectedSweep | true | Manual / 同请求角度 | AutoCoveragePlannerTest escalation/source |
| Auto sweep 专项 | Auto 或显式 planner / 依 fixture 请求 | 由能力/拓扑解析 | Resolved / 独立记录 planner reason | 仅能力/拓扑升级时为 true | Auto / 由全局 sweep 选择器确定，并按实际 source 记录 angle 与 sweep semantic version | AutoCoveragePlannerTest Auto sweep determinism |
| repair 耗尽专项 | Auto | 仍为原解析 planner，除非能力/拓扑本身要求升级 | status/reason 由 planner resolution 事实决定；repair exhaustion 不能伪作拓扑原因 | repair failure alone 不触发 | 请求的 Manual/Auto mode 与 selected sweep 单独记录 | 当前测试未发现明确断言；列为 Mxx/Sol review gap，不臆造 scenario 判定 |

## 11. Mission readiness、上传门禁与上游兼容

后端唯一上传授权由 PlanningArtifactCodec::uploadAllowed(artifact, currentTask) 决定。授权须同时证明 Artifact 当前、非 stale、schema/identity/语义匹配，结果为 PlanningStatus::Success，readiness 为 Ready 或 ReadyWithWarning，canonical hard-safe path 和安全证书完整有效。QML 可提前禁用入口以改善体验，但不能成为安全边界。

| 结果类别 | 保存/展示 | Mission conversion | 后端结果 |
| --- | --- | --- | --- |
| 当前 D0 + Ready | 保留 canonical、质量和来源 | 可转换 | 放行 |
| 当前 D1 + ReadyWithWarning | 保留 hard-safe canonical 与优选偏离说明 | 可转换并呈现 warning | 放行 |
| ReviewRequired（含可靠 Insufficient 或 AssessmentError） | 保留可用 hard-safe 路径/残余与诊断 | 不得转换为执行航点 | 拒绝 |
| D2 DiagnosticOnly | 原始诊断独立持久化并标记不可执行 | canonical 输入为空 | 拒绝，禁止序列化任何任务航点 |
| D3 DiagnosticOnly/None/InvalidInput/Failed | 保存结构化失败信息；无 canonical path | 不转换 | 拒绝 |
| stale、缺失、损坏、身份/语义不匹配 | 标 stale/error；不得自动重规划 | 不转换 | 拒绝 |
| InfrastructureOnly transition Artifact | 按其过渡身份保存，不升级为 IntegratedV05 | 不转换 | 拒绝；用户显式 replan 后再生成新集成结果 |

门禁拒绝的 mission 编码应为零项追加、零序列变异；整计划预检失败不得部分转换/发送。覆盖航点后续的 fence/rally 等序列不得因拒绝路径而被意外改变。普通无 Marine extension 的上游 .plan、空计划及 home-only 计划遵守现有 QGC 保存/加载/发送语义。门禁失败不得使原有 Mission dirty 或改写用户计划。

| 输入结果 | PlanningStatus | MissionReadiness | Current / stale | certification | upload | MissionItems | 序列变化 | fence/rally 后续流程 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Ready D0 | Success | Ready | current | 有效 hard-safe canonical | allow | 编码 canonical | 正常完整计划序列 | 正常既有流程 |
| ReadyWithWarning D1 | Success | ReadyWithWarning | current | 有效 hard-safe canonical，保留优选偏离 warning | allow | 编码 canonical | 正常完整计划序列 | 正常既有流程 |
| ReviewRequired | Success | ReviewRequired | current | 不满足 readiness gate | reject | 0 项 | 不变 | 不启动 |
| DiagnosticOnly | Failed | DiagnosticOnly | current diagnostic | 无 canonical 认证；raw path 非执行 | reject | 0 项 | 不变 | 不启动 |
| None | Unplanned/无结果 | None | 无 current result | 无 | reject | 0 项 | 不变 | 不启动 |
| InvalidInput | InvalidInput | None | 无有效 current result | 无 | reject | 0 项 | 不变 | 不启动 |
| Failed | Failed | None 或 DiagnosticOnly 依结果分类 | current failure only | 无 canonical 上传证书 | reject | 0 项 | 不变 | 不启动 |
| stale result | 历史 outcome；current result 清为 Unplanned | None | stale | 无 current 证书 | reject | 0 项 | 不变 | 不启动 |
| missing Artifact | 无当前集成结果 | None | missing | 无 | reject | 0 项 | 不变 | 不启动 |
| identity mismatch | load 后 Unplanned/stale | None | mismatch | 无 current 证书 | reject | 0 项 | 不变 | 不启动 |
| malformed Artifact | validation error/无 current 结果 | None | malformed | 无 | reject | 0 项 | 不变 | 不启动 |
| AssessmentError | 有 hard-safe path 时 Success，否则按失败/输入语义 | ReviewRequired 或 None | current 但不可认证 | 质量不可用 | reject | 0 项 | 不变 | 不启动 |
| D2 | Failed | DiagnosticOnly | current diagnostic | canonical 为空 | reject | 0 项 | 不变 | 不启动 |
| D3 | Failed/InvalidInput | DiagnosticOnly/None | current failure 或 invalid | canonical 为空 | reject | 0 项 | 不变 | 不启动 |

保留回归的普通 Mission 行为包括普通 mission、允许的 ordinary + D0/D1 混合 mission、MissionSettings end action、home-only clear、有效空计划 clear 和普通 .plan 文件。只对校验成功的空/home-only 文件保留有意清除行为；文件载入失败必须拒绝，不能误判为空计划。

| 普通 QGC 场景 | 必须保留的既有行为 | 拒绝/成功判定 |
| --- | --- | --- |
| ordinary mission | 原来完整 Mission item 顺序、上传和序列化语义不变 | 无 Marine extension 时不经过 Marine gate |
| mixed allowed mission | 普通 item 与当前 D0/D1 Marine item 整体预检，通过后一起转换/发送 | 任一 Marine item 拒绝则整个 mission 零写入、无部分转换 |
| MissionSettings end action | 保留原有 end-action 展示、序列位置和上传行为 | Marine admission 不替代 end-action 逻辑 |
| home-only clear | 有效 home-only 内容可执行既有意图中的 Mission clear | 必须与载入失败产生的空列表区分 |
| valid empty plan clear | 成功 parse/load 的普通空 .plan 可按现有行为清除 Vehicle mission | 只有 loader 确认有效空计划后才允许 clear |
| ordinary .plan file | 正常打开、保存、发送和用户消息/文件关联保持 QGC 兼容 | 无 Marine extension 不因新门禁误拒绝 |

## 12. QML 展示与真实 PlanView 路径

CoveragePlanningPresentation 只把 C++ 已发布的 status/readiness/tier/quality/issues/suggestions/residual/repair facts 转为展示 map。Presentation 与 editor selection 分离：选择其他编辑对象可显示所选对象，但不能改变规划结果或其 identity；更改 planning input 后立即 stale，旧路径不能冒充当前结果。QML 不得自行推断覆盖 PASS、重新评估残余、选候选、生成修补或变更 MissionReadiness。

M00–M09 的 UI 断言须结合真实 Task/ComplexItem/Artifact 生命周期，至少检查 D0/D1/D2/D3 的差异、Ready/Warning/Review/Failed/InvalidInput 的文字与图形、Coverage/Transit leg class、issues 和稳定引用、suggestion、residual、修补 provenance、stale/重新规划提示。D2 红色路线必须有 NOT EXECUTABLE 标识且不得进入 Mission。D3 不得绘制伪连接线。真实 PlanView 上传按钮触发 callback 后仍由 MissionController 和 codec 重新核验；同时覆盖允许、拒绝、全计划原子转换与普通 Mission 回归。

现有 CoveragePresentationTest、CoveragePlanViewUITest 和 Mission/PlanMaster 测试已覆盖若干显示与 callback 单元行为，但并不能单独证明每个 M 场景完成端到端。contract 将完整 v0.5 场景断言和既有 DIRECT 单元语义分别记录，避免把 UI snapshot 当成后端安全证明。

### 现有 UI 表面逐状态盘点

分类基于冻结 V05-09 R1–R13 和 CoveragePresentationTest / CoveragePlanViewUITest 当前断言：IMPLEMENTED + TESTED、IMPLEMENTED + PARTIALLY TESTED、IMPLEMENTED + NOT DIRECTLY TESTED、NOT IMPLEMENTED。此处按同一展示组归类；组内未由该 fixture 直接断言的子字段使用 PARTIALLY TESTED 或 NOT DIRECTLY TESTED，不推导为产品缺陷。

| 状态/fixture | readiness/status/tier | Coverage/Transit 与 PreferredSafe/HardSafeWarning | canonical/diagnostic | boundary/critical residual | issues/suggestions | planner source / repair provenance | IngressNotAssessed | upload admission |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Ready/D0 — T09-01 | IMPLEMENTED + TESTED | IMPLEMENTED + TESTED | IMPLEMENTED + PARTIALLY TESTED（D0 canonical roles；diagnostic absence 由矩阵共同断言） | IMPLEMENTED + NOT DIRECTLY TESTED | IMPLEMENTED + PARTIALLY TESTED（Ingress 和状态问题；全 issue/suggestion 在 T09-12） | IMPLEMENTED + PARTIALLY TESTED（source 与 repair=false；完整 provenance 在 T09-08） | IMPLEMENTED + TESTED | IMPLEMENTED + TESTED |
| ReadyWithWarning/D1 — T09-02 | IMPLEMENTED + TESTED | IMPLEMENTED + TESTED | IMPLEMENTED + PARTIALLY TESTED | IMPLEMENTED + NOT DIRECTLY TESTED | IMPLEMENTED + PARTIALLY TESTED（warning；全 advice 在 T09-12） | IMPLEMENTED + PARTIALLY TESTED | IMPLEMENTED + PARTIALLY TESTED（R12 排除特定诊断态须另核） | IMPLEMENTED + TESTED |
| ReviewRequired — T09-03 | IMPLEMENTED + TESTED | IMPLEMENTED + PARTIALLY TESTED | IMPLEMENTED + TESTED | IMPLEMENTED + TESTED | IMPLEMENTED + PARTIALLY TESTED | IMPLEMENTED + PARTIALLY TESTED | IMPLEMENTED + PARTIALLY TESTED | IMPLEMENTED + TESTED（拒绝/零写入） |
| AssessmentError — T09-06 | IMPLEMENTED + TESTED | IMPLEMENTED + PARTIALLY TESTED | IMPLEMENTED + PARTIALLY TESTED | IMPLEMENTED + PARTIALLY TESTED（不可用 vs 可用字段） | IMPLEMENTED + TESTED（AssessmentFailed；禁止 CoverageBelowRequirement/repair） | IMPLEMENTED + PARTIALLY TESTED | IMPLEMENTED + PARTIALLY TESTED | IMPLEMENTED + TESTED（拒绝） |
| D2 — T09-04 | IMPLEMENTED + PARTIALLY TESTED | IMPLEMENTED + PARTIALLY TESTED（诊断腿角色独立） | IMPLEMENTED + TESTED（canonical 空、raw 诊断分离） | IMPLEMENTED + NOT DIRECTLY TESTED | IMPLEMENTED + PARTIALLY TESTED | IMPLEMENTED + PARTIALLY TESTED | IMPLEMENTED + TESTED（按 R12 正确不注入） | IMPLEMENTED + TESTED（后端拒绝） |
| D3/Invalid — T09-05 | IMPLEMENTED + TESTED（有效 Failed 与 InvalidInput/None 区分） | IMPLEMENTED + PARTIALLY TESTED | IMPLEMENTED + TESTED（不得画伪 route） | IMPLEMENTED + PARTIALLY TESTED（仅可用 overlay/残余） | IMPLEMENTED + PARTIALLY TESTED | IMPLEMENTED + PARTIALLY TESTED | IMPLEMENTED + TESTED（按 R12 正确不注入） | IMPLEMENTED + TESTED |
| Repair applied / attempted-not-applied — T09-08 | IMPLEMENTED + PARTIALLY TESTED（沿用当前 result readiness） | IMPLEMENTED + PARTIALLY TESTED | IMPLEMENTED + TESTED | IMPLEMENTED + PARTIALLY TESTED | IMPLEMENTED + PARTIALLY TESTED | IMPLEMENTED + TESTED（组件、入口、方向、代价、before/after） | IMPLEMENTED + PARTIALLY TESTED | IMPLEMENTED + PARTIALLY TESTED（依 final readiness） |
| stale — T09-09 | IMPLEMENTED + TESTED（Unplanned/None） | IMPLEMENTED + PARTIALLY TESTED（当前路由清除） | IMPLEMENTED + TESTED（无 current path） | IMPLEMENTED + PARTIALLY TESTED | IMPLEMENTED + PARTIALLY TESTED（out-of-date） | IMPLEMENTED + PARTIALLY TESTED | IMPLEMENTED + TESTED（无 current result，故不注入） | IMPLEMENTED + TESTED（false） |

T09-10–T09-16 另外覆盖编辑 draft/stale、规划字段通知、所有 issue/suggestion enum 与 reference namespace、真实 toolbar/callback 后端拒绝及普通/混合/空 Mission preservation。上述细节仍须在 V05-10 最终 build 上重跑并保留原始结果；表内是当前 HEAD 的源码/测试盘点，不是本轮 PASS。

## 13. 共享高度/地形图回归

高度图是实际 PlanView Mission/terrain 可视路径，focused suite 必须继续显示并经过真实 callback。地形绘制采用有效有限点；未知 AMSL marker 仅表示未知，不应转换成巨大数值。零航程/重复距离时，绘图轴使用既有语义的非退化展示区间；恒定/相等高度也使用有效非退化轴范围。此展示轴处理不得改写 Mission 的原始距离、高度或安全评估事实。

Covered regression oracle: CoveragePlanViewUITest 实际展示 terrainStatusChart 与 TerrainProfile，图表宽高非零、轴 finite 且有序；包括普通零 mission distance、恒定 altitude 50m、Marine 平坦 altitude 0m 以及保存计划回归。曲线值不得出现 Inf；NaN 只可用于既有未知采样段语义。PlanView 的 Save As 必须走真实 Mission 路径，不出现极端 QPainter 输入或被吞掉的 warning。V05-09 R2 的两个 producer 修复是本轮既有基线，V05-10 不更改/隐藏该组件。

F1 的历史根因为 TerrainStatus.qml 的零距离/恒定高度轴退化与 TerrainProfile.cc 未知 AMSL 首 marker 坐标两处；已有实现使用非退化显示轴及有限 display lower bound，focused test 实际走过高度图。该证据用于回归边界，不替代本轮任何新运行结论。

| Terrain 输入/路径 | 期望证据 | 原始数据不变量 | 当前 focused oracle |
| --- | --- | --- | --- |
| 零 mission distance / 重复距离 | finite ordered 非退化 x-axis；QPainter 无 invalid/extreme coordinate warning | mission distance 与 waypoint 不改写 | CoveragePlanViewUITest zero-distance case、PlanViewTest Save As |
| 恒定 AMSL（50m） | finite ordered y-axis；高度图仍可见 | altitude sample 不改写 | CoveragePlanViewUITest ordinary flat-altitude case |
| Marine 恒定 AMSL（0m） | 轴非退化且有序；图保持可见 | TerrainProfile/Task 高度事实不改写 | CoveragePlanViewUITest Marine flat-altitude case |
| 未知 AMSL first marker | marker 坐标有限；unknown 仍为 unknown，不转成真实零高程 | 只为画图选择有限 display lower bound，不改输入值/availability | CoveragePlanViewUITest 曲线有限检查；缺独立 marker 数值断言 |
| missing terrain samples | 显示 missing/unknown 语义；不以 Inf/极端数值接入 QPainter | terrain query 结果和 missing 状态不变 | CoveragePlanViewUITest series 缺失样本覆盖；实施时核实各曲线 oracle |
| Save As / 实际 PlanView profile | 真实产品菜单和高度图保持可见，保存回调完整通过，无 QPainter warning | 保存前后航点/高度不变 | PlanViewTest::_testSaveAsMenu 与 CoveragePlanViewUITest 实际 PlanView callback |

## 14. 回归套件与等价基线归因

### Windows build 与 binary evidence gate

V05-10 freeze 前必须在 Windows 主平台对当前接受 source HEAD 执行 `cmake --build build/P0-01-marine-debug --target QGroundControl --parallel 8`，并要求 exit code=0。正式运行需进入项目既定 VsDevCmd x64 / Windows build kit 环境，记录 OS、compiler、Qt、CMake/configuration、完整命令、source HEAD、日志与生成的 `QGroundControl.exe` SHA-256。所有最终 focused/regression/SITL/manual evidence 必须标识这一接受 binary；若因后续获批修改重新 build，则记录新 hash 与原因，并重新判定哪些旧 evidence 仍可绑定，不能把不同二进制的结果静默混为一次 freeze acceptance。任何未归因 build FAIL 或 binary/evidence identity 不一致均为 BLOCKING。


| 回归组 | 必需验证 | 记录要求 |
| --- | --- | --- |
| Marine 规划域 | geometry backend、Task adapter、Simple/BCD/Auto、quality、repair、identity、codec | 每个 M 语义与既有单元断言分开；无隐式重规划 |
| 集成与任务项 | CoverageComplexItem、MarinePlanIntegration、CoverageTaskAdapter、MarineTaskModel/Json | exact restore、stale、InfrastructureOnly、ordinary plan |
| Mission gate | ArduPilotMissionAdapter、MarineUploadGate、MissionController、PlanMasterController、MissionManager | 放行/拒绝、序列原子性、普通任务项与特殊 item 顺序 |
| Plan/QML | CoveragePresentation、CoveragePlanViewUI、PlanView | 实际 callback、terrain profile、toolbar、保存/加载 |
| 上游 Mission | MissionManagerTest、MissionControllerTest、PlanMasterControllerTest、CustomPluginIntegrationTest | 保留普通 Mission 行为；记录 platform/localization 差异 |
| SITL fixture | MarineSITLValidationTest（适用 opt-in 场景） | 区分环境未提供导致 SKIP 与实际 FAIL；SITL 不替代单测语义 oracle |

逐 suite、逐 test case 记录 PASS、FAIL、SKIP、ENVIRONMENT BLOCKED 或 NOT EXECUTED，并附构建配置、Qt/平台、fixture、日志/报告路径。基线为 V05-08 commit 4a948c8842a915fe3125322ec9eec9e66a007fb4；归因比较需同一 Windows build kit、Qt/runtime、配置、测试命令、环境变量、输入 fixture 和可比日志。失败只有在该精确用例在基线提交上复现，且行为与错误分类一致，才能标 PRE-EXISTING/UNRELATED。未复现、执行条件不等价、无原始日志或仅凭历史报告推断，都保持 BLOCKING/UNRESOLVED；若失败由 V05-09/V05-10 变更导致，在当前包内修复并重跑。

V05-09 R2 报告记录三条 localization raw FAIL 在 V05-08 基线具体重现：MissionManager::_testErrorAckFailureStrings、PlanView::_testPlanViewStates、_testRoverWaypointOnEmptyPlan。V05-10 应重新执行并对当前结果逐案归因；旧报告仅为检索基线复现材料的线索，必须核对同环境命令、原始输出和提交身份后方可复用结论。历史 3 个 opt-in SITL skip 同样不得充当本轮执行证据。

## 15. SITL 验证范围与冻结验收合同

当前 MarineSITLValidationTest 提供旧 P2-13 S01–S04 流程，不等同于 v0.5 M00–M09 执行安全与 Artifact gate。现有入口及环境条件如下：

| fixture/入口 | 条件/证据 | 可证明范围 | 不证明/缺口 |
| --- | --- | --- | --- |
| _analyzeExecutionSafety | 设置 QGC_P2_EXECUTION_SAFETY_ANALYSIS；保存离线分析结果 | 该分析输入上的历史执行安全计算结果 | 不启动 SITL、不经当前 QGC Mission send，也不证明 v0.5 readiness/Artifact gate |
| _validateP2Scenarios | QGC_P2_SITL_EVIDENCE_DIR；可选 QGC_P2_SITL_SCENARIO 与 QGC_P2_SITL_SEED_STALE_EXECUTION_STATE；ArduRover TCP 127.0.0.1:5760 | legacy S01–S04 的构建、保存/重载、append/upload/execute 行为（仅环境已提供且未触发 skip 时） | 不断言 v0.5 Quality、M00–M09、Task/Artifact identity 或 backend readiness gate；opt-in 未设时只会 skip |
| _diagnoseS04Execution | QGC_P2_SITL_DIAGNOSTIC_ROOT、QGC_P2_SITL_DIAGNOSTIC_PLAN、QGC_P2_SITL_DIAGNOSTIC_RUN 与固定历史 plan hash | 对历史 S04 输入/运行执行离线诊断 | 不构成 v0.5 场景，不认证当前 HEAD 或 upload gate |

V05-10 SITL 的 acceptance set 已由独立设计审查解决 RQ-01 并冻结为两类。ALLOW+EXECUTE：M00 D0/Ready 与 M04 D1/ReadyWithWarning，使用当前 `Task → Integrated Artifact → backend upload gate → MissionAdapter → MissionController/PlanMasterController → ArduRover SITL` 后端产品链；SITL 不强制经过 QML/PlanView 自动化，因为真实 GUI callback 由 UI10/H10 独立验证。CONNECTED BACKEND REJECTION：M08 ReviewRequired、M05 D2/DiagnosticOnly、M09 有效 D3，以及从此前有效 M00 Artifact 通过 planning-critical input mutation 得到的 stale 场景。Reject 场景不进入 AUTO；必须证明 backend refusal、0 `writeMissionItems`、0 new executable mission、0 partial conversion/send、mission/sequence 不变、不误清已有 mission、send/sync state 不 stuck。

ALLOW+EXECUTE 场景必须显式遵守 `IngressNotAssessed` 边界：运行前将 SITL vehicle 初始/current position 锚定在首个 canonical Mission waypoint 的明确记录容差内，沿用现有 `anchorFirstPathPoint()` 的工程原则；若超出该容差，场景不得开始或计为 v0.5 execution PASS。这样后续实际轨迹可以作为 Mission execution evidence，但不能把未认证的 vehicle-to-first-waypoint ingress 混入规划安全认证。

默认 SITL 环境复用已验证的 P2-13K 环境：ArduRover `4.7.0`；Docker `qgc-ardurover-sitl:rover-4.7.0`；image ID `sha256:001f20d07215138f2cb4aebc8eb691c8f4c3a23825a01f343b3f5397d262d3f0`；frame `rover`；home `47.397742,8.545594,488,0`；TCP `127.0.0.1:5760`。证据必须记录 WP_RADIUS、WP_SPEED、TURN_RADIUS、ATC_TURN_MAX_G、WP_ACCEL、WP_JERK、fixture/hash、command/env、plan hash 与 seed；没有随机 seed 时明确记 N/A。Allow 场景还要检查 Mission Start ACK accepted、MISSION_STATE_ACTIVE、ordered waypoint progression、Mission Complete、无 Marine 自动 RTL。涉及 O/N 的 allow fixture 若实际轨迹进入原始 No-Go 或离开 NavigationArea 则 FAIL；仅进入规划 H/E safety band 但未违反原始 N/O 时记录为 execution-side limitation，不把车辆动态跟踪表现反向改写为规划器静态安全语义。

RQ-01 已解决；上述场景集是 V05-10 SITL 合同，不代表现有 legacy fixture 已具备这些能力。若环境/fixture 未实现或未执行，则按 ENVIRONMENT BLOCKED/NOT EXECUTED/SKIP 如实记录，绝不能用历史 P2-13K PASS 或旧 opt-in skip 代替本轮 v0.5 SITL evidence。

## 16. 人工验收流程

人工验收须以冻结场景输入和对应 Artifact/机器 evidence 为前提，使用实际 Windows GUI 与真实 PlanView/Marine complex item；保存版本、构建 ID、系统/Qt、firmware plugin、vehicle connectivity、截图/录屏、plan/mission 文件、日志和操作者判断。不得手动修饰文件后声称通过。

| 人工检查 | 操作与 pass oracle |
| --- | --- |
| 结果图形 | 查看 C/N/O 编辑区域和 Coverage/Transit；D0/D1 路径可执行状态清楚，D2 显示不可执行诊断线，D3 无伪 route |
| 状态与建议 | 逐一检查 Ready、ReadyWithWarning、ReviewRequired、Failed、InvalidInput；issue/suggestion 引用有稳定定位且不改变算法事实 |
| 过期与持久化 | 保存匹配结果再加载，无重规划且状态一致；修改规划字段后旧 Artifact stale 且上传拒绝；UI-only 字段变化不错误过期 |
| Mission 上传 | 当前 D0/D1 通过真实 PlanView callback；Review/D2/D3/stale/malformed 拒绝；拒绝后序列和普通 Mission 不变 |
| 高度/地形图 | 高度图仍显示、尺寸有效、轴有限有序；真实 Save As 完成，无 QPainter 坐标告警 |
| 普通 QGC 计划 | 无 Marine extension 的既有打开/保存/上传操作仍正常 |

### 按冻结语义逐场景的人工清单

| 场景 | 准备 | 操作者动作 | 地图/路径期望 | readiness | 诊断/残余 | issues/suggestions | 上传 | 保存/重载观察 | 留证 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| M00 | 固定 Manual、矩形 C、N=C、无 O、H/P/E 有效、Standard 且初始候选 PASS 的冻结 fixture | 选择 Marine 任务、规划、重复同输入规划、保存并重载 | SimpleMonotone 的 hard/preferred-safe 规范路径；无合成周界航线 | Ready / D0 | 无诊断候选；IngressNotAssessed 按结果合同呈现 | 无覆盖失败码；issue/source 按已存结果呈现 | 启用且后端允许 | outcome 完全恢复，无重规划 | fixture ID/hash、路径图、来源、重复结果、Artifact 与日志 |
| M04 | 不存在满足场景要求的优选解，但有 H/E hard-safe 且质量 PASS 候选 | 规划，查看 margin 标示和 issue，上传至测试车辆或 gate mock | D1 hard-safe canonical，warning 段保持不同样式 | ReadyWithWarning | 不以诊断候选替代 canonical | PreferredSafetyViolated；H/P/E 与建议内容可见 | 启用并允许 | 保存/重载后 warning、来源和身份一致 | fixture/hash、warning 图、readiness、upload 日志、mission 序列 |
| M05 | 无 D0/D1，但存在有意义连续 RawNavigationFreeSpace route | 规划、查看诊断线、触发上传 callback，再保存/重载 | canonical 为空；诊断单独显示并标 NOT EXECUTABLE | DiagnosticOnly | D2 诊断候选保留为诊断，不作覆盖认证 | UnsafeDiagnosticCandidate 与原因 | 禁用且后端拒绝 | 诊断精确恢复，不成为 canonical；Mission 无航点 | fixture/hash、诊断 Artifact、拒绝原因、零写入/零变异日志 |
| M07 | hard-safe 初始 Insufficient 和可安全改善的目标相对分量 | 规划并逐项检查修补结果，再保存/重载 | 最终 canonical 含已应用修补且安全的 Coverage/Transit legs | 按优选层级为 Ready 或 ReadyWithWarning | 保存修补前后 residual 与 metrics | CoverageRepairApplied；component/entry/direction 与 before/after 事实 | 仅最终 Ready/Warning 可上传 | provenance 精确往返且不重算 | fixture/hash、前后图、provenance、evaluator 与 upload 日志 |
| M08 | 存在 hard-safe 候选，且受支持候选/修补尝试均无法通过所选策略 | 规划、检查路径和残余，尝试上传，再重载 | hard-safe canonical 保留，不呈现为通过 | ReviewRequired | boundary/critical residual 和 availability 如实显示 | CoverageBelowRequirement；可有 CoverageRepairInsufficient | 禁止且拒绝不改 Mission 序列 | ReviewRequired/path/residual/provenance 精确恢复 | 候选/repair 穷尽记录、截图、Artifact、零写入日志 |
| M09 | 分别准备有效但不支持拓扑与无效输入两个冻结 fixture | 规划、查看问题/overlay、触发上传，再保存/重载 | D3 无 canonical/伪连线；两类状态明显区分 | 有效求解为 DiagnosticOnly；非法输入为 None | 有效失败可有 overlay；不伪造 route/残余 | 两类错误分别显示稳定问题解释 | 两种均禁止，后端拒绝 | 保存状态/类型一致；非法/损坏输入不执行 | 两个 fixture/hash、状态截图、problem/overlay、拒绝和零变异日志 |

人工检查当前为未执行的设计清单。每项结果必须记录测试版本与 fixture identity；截图不能替代机器 oracle、Artifact 校验或真实 Mission callback。

现场/真实 USV 操作不属于 V05-10 软件人工验收；禁止把 UI 测试、SITL 或桌面演示写成 field acceptance。

## 17. 现场验证边界

最终状态应原样记录为：FIELD VALIDATION DEFERRED BY FROZEN SPEC。依据 P2 v0.5 §33，P1 real-USV field validation 仍明确延期且未执行；须按已提交的 P1 field protocol 完成前置工作，之后才可评估 P2 real-USV field acceptance。§33 同时要求 v0.5 自有 SITL 与人工验收证据，但没有将实船测试设成 V05-10 软件验收项；§36 禁止在缺乏对应证据时声称 field 前置工作已完成。P1_REAL_USV_FIELD_VALIDATION_PROTOCOL.md 的状态为 DEFERRED AND NOT EXECUTED。设计审计、实现完工、Windows 测试、SITL、软件冻结审计与实船认可彼此分离。V05-10 不修改该协议、不申请或暗示实船执行，也不把历史 P2-13K 证据提升为 v0.5/field 认证。

## 18. 机器证据及报告格式

建议将本轮实现证据集中写入 build/v05-10/，包含 manifest、工具链、提交 SHA、逐用例结果、原始 stdout/stderr、XML/日志、基线对照、artifact/plan fixture hashes、SITL 原始证据和人工验收记录。生成 JSON 只保留可重复机器读取的结构；报告链接原始工件，不以摘要替代原始结果。该路径是提案，当前未创建或写入任何机器证据。

每条执行记录字段：package、source revision、baseline revision、dirty state、build kit/configuration、OS、compiler、Qt、接受 binary SHA-256、test suite/case、status、exit code、start/end、scenario/fixture/hash、Task identity、必要时 Artifact/result identity、环境开关、log/evidence paths、baseline equivalent condition、attribution、reviewer。SKIP 必须有明确不适用/ opt-in 原因；因环境缺失不能运行时记 ENVIRONMENT BLOCKED；未执行记 NOT EXECUTED；执行失败必须保留 FAIL，不可改写为 SKIP。不同 source/binary 的证据不得静默合并为同一 acceptance run。

## 19. 冻结审计清单

最终机器报告需有如下固定字段：

| 字段 | 内容 |
| --- | --- |
| V05 commit chain / final HEAD / working tree | V05-08/V05-09/V05-10 SHA、完整 HEAD、branch、dirty/staged/unstaged/untracked |
| binary SHA / toolchain | 可执行文件 SHA-256、OS、编译器、Qt、build kit、configuration、build 命令 |
| Task v3 / Artifact v3 identity | schema、fixture hashes、规划输入 fingerprint、Artifact identity 和 exact restore 结果 |
| planning semantic identity | p2.v0.5.planning.2、coverage-quality.v1、CAL-01=.99、CAL-02=.50m、兼容性结果 |
| planner semantic identities | requested/resolved planner、策略 ID/version、sweep semantic/version、resolution reason |
| M00–M09 / regressions | 每场景 fixture/hash、结果；每个回归 suite/case、状态、证据路径 |
| baseline-attributed failures | 当前/基线 SHA、构建/运行等价条件、逐 case 结果、归因、双侧原始日志 |
| SITL / manual acceptance | scenario contract、vehicle/firmware/build、fixture/seed、判定 oracle、原始日志/录屏与操作者 |
| field status | FIELD VALIDATION DEFERRED BY FROZEN SPEC 或经单独批准取得的后续实证 |
| known defects / environment blockers | 稳定 ID、根因、影响、阻断条件及其证据 |
| scope deviation / transitional behavior audit | 文件/架构变化、批准来源、InfrastructureOnly/legacy/schema/load/save 过渡行为及逐项证据 |
| Any frozen v0.5 requirement still dependent on transitional behavior? | YES / NO；若 YES，列具体条款、过渡行为与批准来源 |

完成全部要求的机器和人工验证后，实施报告逐项给出证据位置和审计者：

1. 当前源码与文档符合 v0.5 active authority，所有冻结条款映射到实现、测试和证据；偏差具明确批准来源，否则阻断。
2. Task v3/Artifact v3/Marine extension v2、strategy/policy/planning semantic identity 和 CAL-01/CAL-02 与冻结值一致；普通 QGC .plan 兼容。
3. C/N/O、H/P/E、candidate tier、quality/residual、repair、diagnostic namespace、MissionReadiness 与 upload gate 分别有证据。
4. M00–M09 及所需 planner/geometry/safety/quality/persistence/Mission/P0/P1/QGC regressions 均有逐条结果；未分类失败均阻断。
5. SITL 仅在 RQ-01 已解决、场景 oracle/环境/原始证据完整后可标通过；否则如实列为 deferred/blocked，并按冻结验收门评估 DoD。
6. GUI 人工验收与现场验证状态分开；真实 USV 必须保持 deferred，直到 P1 protocol 要求完成。
7. 克隆/干净工作树的正式实现与冻结流程、变更清单、review status 可重现；当前设计草案不进行 release/freeze 操作。
8. Implementation report 和 machine evidence 与 HEAD/工作树一一对应，逐项给出 tests/build/core static-analysis 结果及未执行项。
9. 作者不得自我批准；独立 reviewer 核对冻结规范、这两份设计文件、实现差异、原始机器证据与剩余风险后给出决定。

本轮仅有文档审计，不满足任何软件 freeze 或 field acceptance 声明。

## 20. 独立设计审查裁决和已识别差异

| ID | 问题/差异 | 当前证据与建议 | 状态/阻断影响 |
| --- | --- | --- | --- |
| RQ-01 | v0.5 SITL 对应哪些强制场景、固件/仿真环境、成功/停止 oracle、日志 schema 与 seed 条件？ | 独立设计审查已裁决：M00/M04 ALLOW+EXECUTE；M08/M05/M09-valid-D3/stale CONNECTED BACKEND REJECTION；backend product chain 不强绑 GUI；allow 场景首点锚定；默认复用 P2-13K ArduRover 4.7.0 环境 | RESOLVED BY INDEPENDENT DESIGN REVIEW；实现阶段必须据此建立新 v0.5 fixture/evidence，不能复用历史 PASS 冒充 |
| GAP-01 | 现有 MarineSITLValidationTest 不覆盖 v0.5 Artifact/readiness gate | 源码显示 legacy S01–S04、offline analysis 与诊断流程，不能作为新 SITL 场景集的直接证据 | OPEN IMPLEMENTATION GAP；按已冻结 S10 合同扩展/新增验证能力，未完成则 SITL DoD BLOCKING |
| GAP-02 | 现有 M 场景由多个真实单测与集成断言组合，单一端到端 fixture 不齐全 | 独立设计审查允许组合证据，但每个 Mxx 必须有 canonical fixture/scenario identity；所有组合层绑定同一 Task identity、必要时 Artifact/result identity 和最终接受 binary | RESOLVED AS DESIGN RULE；实现阶段若任何 Mxx 无法满足 identity-bound evidence，则 BLOCKING |
| GAP-03 | P1 field protocol 明确未执行 | repository AGENTS 与冻结协议规定 defer；V05-10 只保存该边界 | DEFERRED BY FROZEN SPEC；不是 V05-10 软件修复项 |

当前已不存在需要继续上升为产品语义裁决的开放设计问题；剩余 GAP 是实现/验证证据缺口。不得用历史报告补足缺少的本轮运行结果。

## 21. 非目标和停止门

本任务不更改生产代码、QML、测试、冻结 V05-08/V05-09 contract、active v0.5 规范、CAL 值或历史 field protocol；不运行包级/全量测试、build、SITL、人工/实船验收；不实现修复、写机器证据、stage、commit、push、merge、release 或 V05-11。两份文档完成后仅做文档差异/格式核对，停在“V05-10 DESIGN READY FOR INDEPENDENT REVIEW”门前。独立审查和新的明确授权之后，才可开始实现与执行验证。
