# V05-10 设计合同：验收、集成、SITL、人工验证与冻结审计

合同身份：V05-10-DC-v1
状态：FROZEN — independent design/contract review APPROVED。本文不是 V05-10 execution authorization、软件冻结或 field acceptance；V05-10 execution 仍须单独授权。
语言：中文。冻结语义名词和代码 API 保留英文，便于与规范及源码逐字核对。

## 1. 合同边界

本合同定义 V05-10 的验证、回归归因、持久化、Mission 集成、SITL、人工验收和冻结审计判定。设计来源为 P2 v0.5 冻结规范、冻结 V05-08/V05-09 合同以及当前实现/API 盘点。当前没有运行本合同中的 build、test、SITL 或 GUI 验收。

本合同不授权更改生产代码、测试代码、V05-08/V05-09 冻结合同、P2 v0.5 语义、CAL-01/CAL-02、field protocol 或 V05-11。实施前必须先通过独立设计审查并取得单独实施授权。作者不得进行自我批准。

## 2. 判定词汇

| 记录值 | 含义 |
| --- | --- |
| PASS | 条款全部 pass oracle 有可复核的实际证据 |
| FAIL | 实际执行违反 pass oracle；保留原始失败证据 |
| SKIP | 有明确、预先可说明的不适用或 opt-in 原因；不得借此隐藏失败 |
| ENVIRONMENT BLOCKED | 条件必需但环境未提供，因而不能执行；属于未完成/阻断，不是通过 |
| NOT EXECUTED | 尚未运行；不得推导为 PASS |
| PRE-EXISTING/UNRELATED | 仅在相同用例、等价构建/运行条件下于基线提交具体重现后可用 |
| BLOCKING / OPEN | 影响 V05-10 DoD、冻结语义或证据充分性，必须解决或由有权方明确裁决 |
| NON-BLOCKING / DEFERRED | 不影响本包软件结论，且冻结权威明确要求延期；必须保留准确状态 |

SKIP、ENVIRONMENT BLOCKED、NOT EXECUTED、无法证明条件等价的回归失败、没有基线复现证据的“预存”判断，均不得按 PASS 或 PRE-EXISTING 结案。V05-10 失败若由本包变更导致，须在本包修复并重跑。

## 3. 实施条款

每项条款记录稳定 ID、要求、权威来源、验证方式、通过判据和阻断分类。源文件名为 repository-relative path；唯一的例外是所有者提供、未跟踪于仓库的 V05-08-DC-v1 合同附件。

### E10 — 证据与工作区

| ID | 要求 | 来源 | 验证方式 | PASS 判据 | 未满足分类 |
| --- | --- | --- | --- | --- | --- |
| E10-01 | 每次执行记录提交 SHA、基线 SHA、工作树状态、Windows OS、编译器、Qt、build kit/configuration、命令、环境变量、fixture/hash、开始结束时间、退出码和原始日志位置 | AGENTS.md §9；P2 v0.5 §§32–33、36；V05-08/V05-09 contract | 检查机器 manifest 与原始工件 | 记录可定位且与运行本身一致；关键上下文完整 | BLOCKING |
| E10-02 | 每套件/用例只使用 PASS、FAIL、SKIP、ENVIRONMENT BLOCKED、NOT EXECUTED 等本合同状态；FAIL 保留原始信息 | AGENTS.md §9；本合同 §2 | 对照原始 test report、stdout/stderr、XML 与摘要 | 摘要没有覆盖、改写或遗漏 raw FAIL | BLOCKING |
| E10-03 | 仅在等价环境的 V05-08 基线精确复现同一失败用例后，才能标 PRE-EXISTING/UNRELATED | owner decision V05-09 F3；V05-08 baseline commit 4a948c8842a915fe3125322ec9eec9e66a007fb4 | 当前与基线逐用例重跑；比对 build/runtime/命令/fixture/log | 相同用例在基线与当前以同类错误稳定复现，且条件等价；附双方原始日志 | BLOCKING；不能归因时保持 unresolved |
| E10-04 | 准确披露没有运行的 build、focused tests、QGC regressions、SITL、GUI 人工验收与 field validation | AGENTS.md §9；本合同 §2 | 对照命令记录、报告与原始机器工件 | 每一项均有实跑结果或标记为 SKIP/ENVIRONMENT BLOCKED/NOT EXECUTED | BLOCKING（若宣称完成/通过）；否则按准确状态分类 |
| E10-05 | 当前 V05-10 接受版本必须在 Windows 主平台完成 QGroundControl build，且最终验证证据必须绑定该 source HEAD 与生成的 executable SHA-256；后续若重建二进制，必须记录新 hash、原因及哪些旧证据需要重跑 | AGENTS.md §§8–9；P2 v0.5 §§33、36；V05 freeze package 要求 | 在 VsDevCmd x64 / 当前项目正式 Windows build kit 中执行 `cmake --build build/P0-01-marine-debug --target QGroundControl --parallel 8`，记录 HEAD、toolchain、exit code、日志与 `QGroundControl.exe` SHA-256 | build exit code=0；最终 focused/regression/SITL/manual evidence 均标识同一接受 binary，或明确标识经批准的后续 rebuild 及重新绑定证据 | BLOCKING；任何未归因 build FAIL 或 binary/evidence 失配均阻止 freeze |

### M10 — 冻结场景 M00–M09

下列场景语义取自 P2 v0.5 §§7–26、32–33、36；具体示例不能削弱规范的一般要求。完整矩阵见 V05_10_TECHNICAL_DESIGN.md §5。每个 M00–M09 必须具有命名且固定的 canonical scenario/fixture identity，并记录 fixture hash；允许 planner/evaluator/persistence/UI/Mission 多层组合证据，但组合证据必须绑定同一 scenario/Task identity，需要时绑定同一 Artifact/result identity，并绑定同一接受 source/binary。不得用不同输入的零散 PASS 拼接成某个 M 场景 PASS。

| ID | 要求 | 来源 | 验证方式 | PASS 判据 | 未满足分类 |
| --- | --- | --- | --- | --- | --- |
| M10-00 | 简单可解矩形自动选择 SimpleMonotone；覆盖、硬/优选安全、readiness、Artifact、上传一致 | v0.5 §15、M00；V05-08-DC-v1 | Auto/planner、quality、complex item、codec、Mission 集成断言 | 输出 canonical hard-safe D0；质量 PASS 时 Ready、可上传；不合成导航区边界覆盖 | BLOCKING |
| M10-01 | 简单凹但单调目标仍选择 SimpleMonotone | v0.5 §15、M01 | 对实际凹形输入执行 Auto 解析并核对结果来源 | SimpleMonotone 成功且来源/identity 与输入一致 | BLOCKING |
| M10-02 | 固定角度下非单调拓扑升级 BCD，并保留可解释的解析原因/来源 | v0.5 §15、M02 | Auto fixture、PlanningIssue/presentation/Artifact | resolved strategy 为 BCD，升级原因稳定，目标与导航职责正确 | BLOCKING |
| M10-03 | N 大于 C 时覆盖义务仍为 T=C−O；导航区增量不能成为额外覆盖目标 | v0.5 §§5–10、M03 | 使用 C 外连接/转弯但仍在 N 的真实输入；检查 coverage-role 评估和路径 | C 外仅允许 route/transit；评估不计 N−C 覆盖 | BLOCKING |
| M10-04 | 优选安全不可行而硬安全可行时，不削弱 H/P/E；覆盖 PASS 的硬安全候选胜过优选层级但覆盖失败候选 | v0.5 §§8–14、M04 | 使两个候选同时出现的确定输入；检查 ranking、readiness、warning 和 Artifact | D1、Success/ReadyWithWarning；优选偏离可说明且硬安全不变；PASS 候选胜出 | BLOCKING |
| M10-05 | 硬安全不可行时保存真实 RawNavigationFreeSpace 诊断 route 为 D2，不作为 canonical/Mission | v0.5 §§11–14、M05 | 构造原始连续 route 且无 D0/D1 的输入；检查 codec、QML 与 append/send | Failed/DiagnosticOnly；诊断独立保存；canonical 为空、上传拒绝、零航点序列变异 | BLOCKING |
| M10-06 | 相同输入在 Standard 通过而 Strict 不通过时，几何/度量不变，仅按已选要求改变判定 | v0.5 §§18–21、37、M06 定义策略结构与校准门；后续 owner approval 由 AGENTS.md §7 与 V05-04 closure 冻结 CAL-01=0.99、CAL-02=0.50m、coverage-quality.v1 | 对同一 T/path/roles/swath 双评估并绑定不同策略身份 | Standard 通过；Strict Insufficient/ReviewRequired；0.99、0.50m 与 coverage-quality.v1 不变 | BLOCKING |
| M10-07 | 可靠 Insufficient 时逐个安全修补分量、全量重评，并在首次 PASS 后停止 | v0.5 §§22–24、M07 | CoverageRepair + provenance + Artifact/UI/Mission 组合验证 | 安全增量、严格改善、PASS 即停；attempted/applied 与所选分量事实精确 | BLOCKING |
| M10-08 | 修补耗尽仍不足时保留最佳硬安全路径/residual 和 ReviewRequired，拒绝上传 | v0.5 §§12、18–24、M08 | 规划、修补穷尽、持久化、PlanView callback 与后端 gate | Success/ReviewRequired（若存在硬安全路径）；残余存在，Mission 序列不变 | BLOCKING |
| M10-09 | 有效但不支持/未解决拓扑不伪造连接 route；非法输入与有效求解失败须区分 | v0.5 §§7–14、M09 | 分别输入有效不支持拓扑与非法 schema/geometry，检查结果/overlay | 有效失败依语义为 Failed/DiagnosticOnly；输入错误为 InvalidInput/None；二者均无 canonical route 并拒绝上传 | BLOCKING |

### P10 — Task/Artifact 持久化与身份

| ID | 要求 | 来源 | 验证方式 | PASS 判据 | 未满足分类 |
| --- | --- | --- | --- | --- | --- |
| P10-01 | Marine extension v2、Task v3、Artifact v3 和规划/质量语义身份保持冻结版本；普通 QGC .plan 兼容 | v0.5 §§28–30；AGENTS.md §6 | codec roundtrip 和普通 QGC plan 回归 | 当前 Marine schema 匹配规范；普通无 Marine extension 文件保持行为 | BLOCKING |
| P10-02 | 匹配、受支持 Artifact 精确恢复，不调用 planner/evaluator/router/repair，不自动重规划 | v0.5 §§28–30；V05-08-DC-v1 | spy/counter 或可验证调用证据；比较序列化前后语义及路径顺序 | 保存的状态/顺序/可用性/provenance 原样恢复，四类调用均为 0 | BLOCKING |
| P10-03 | 规划输入或语义 fingerprint 不匹配即 stale，清除 current executable state，不自动重规划 | v0.5 §§29–30 | 逐一改变 identity 字段并 load | Unplanned/stale、无当前 canonical 可执行结果、上传拒绝；planner 调用为 0 | BLOCKING |
| P10-04 | 身份包含 C/N/所有 O、swath、H/P/E、requirement、policy semantic、sweep mode/适用 Manual angle、requested planner、resolved strategy/version 与 planning semantic | v0.5 §§29–30；PlanningInputIdentityTest | 逐字段 mutation 对比 fingerprint 和现存 identity tests | 规划字段变化使 stale；Auto 未使用角度、名称/传感显示/纯 UI 选择遵从冻结排除规则 | BLOCKING |
| P10-05 | 不支持的旧 Marine-private schema 不迁移；InfrastructureOnly transition Artifact 保持不可升级/不可上传，须显式 replan | v0.5 §§28–30；V05-08-DC-v1 | legacy/malformed/transition fixture load + gate | 状态和错误明确；无自动转换、无重新评估、无任务航点 | BLOCKING |
| P10-06 | AssessmentError 的逐项 availability、已知事实和 unknown/missing/null 精确保存；未知不置零 | v0.5 §§18–21、27–30；V05-08-DC-v1 | 含部分可用指标与残余的错误 Artifact 往返 | 错误状态可解释且字段 availability 原样往返，上传拒绝 | BLOCKING |

### D10 — 安全结果层级 D0–D3

| ID | 要求 | 来源 | 验证方式 | PASS 判据 | 未满足分类 |
| --- | --- | --- | --- | --- | --- |
| D10-00 | D0 是 hard-safe 且 preferred-safe canonical；PASS→Success/Ready，AssessmentError/可靠 Insufficient 保留安全路径并 ReviewRequired | v0.5 §§11–14、M00 | Planner + result publication + Artifact + presentation | 仅完整安全 PASS 可 Ready/上传；错误/不足均不得授权上传 | BLOCKING |
| D10-01 | D1 是 hard-safe、违反 preferred margin 的 canonical；PASS→ReadyWithWarning，H/P/E 不改变 | v0.5 §§8–14、M04 | margin tier 与 issue/readiness 集成验证 | PreferredSafetyViolated 被保留；仅 ReadyWithWarning 可上传 | BLOCKING |
| D10-02 | D2 仅有 raw 诊断 route，canonical 为空，诊断不得转换或进入 Mission | v0.5 §§13–14、M05 | 真正连续的 raw route 端到端至拒绝 callback | DiagnosticOnly；图形清楚标不可执行；Mission append/send 无变异 | BLOCKING |
| D10-03 | D3 不存在有意义的受支持 raw route；诊断 overlay 不连造路径，InvalidInput 与有效 Failed 分开 | v0.5 §§13–14、M09 | 两类输入及 QML/Artifact 检查 | 无伪 route，状态区分并可解释，拒绝上传 | BLOCKING |
| D10-04 | 任一 canonical segment 均按整段几何检查 HardExecutionTrackRegion；端点安全不能代替 segment 安全 | v0.5 §§8–14 | 线段穿越禁区/安全边界的几何 fixture | segment 检查判定与 H/E 公式一致，unsafe leg 不进入 canonical | BLOCKING |

### Q10 — Quality、候选排序与修补

| ID | 要求 | 来源 | 验证方式 | PASS 判据 | 未满足分类 |
| --- | --- | --- | --- | --- | --- |
| Q10-01 | Coverage 只评估 T=C−O 上 Coverage-role legs 的 swath/2 round-cap 足迹；Transit 和 T 外区不计入 | v0.5 §§5–7、18–21 | evaluator 几何 oracle、M03 和同输入 role 对照 | 面积、ratio、边界残余与 oracle 相同；transit 不增加 coverage | BLOCKING |
| Q10-02 | coverage-quality.v1、Standard=.99、boundaryTolerance=.50m、area tolerance=max(0.01m², 1e-6×A) 固定 | v0.5 §§18–21、37 冻结 policy structure/calibration gate；AGENTS.md §7 与 V05-04 closure 记录后续 owner-approved CAL-01/CAL-02 与 policy identity | 常量/semantic identity 检查及边界 case | 数值、版本和数值容差逐一匹配冻结值及其权威链 | BLOCKING |
| Q10-03 | Quality 必须区分 Complete、Acceptable、Insufficient、AssessmentError；Error 不得成为 Insufficient 或触发 Repair | v0.5 §§18–24 | 完整状态矩阵及候选生命周期调用计数 | Error 发 CoverageAssessmentFailed、不发 CoverageBelowRequirement、不修补；已知 hard-safe path 保留但 ReviewRequired | BLOCKING |
| Q10-04 | 关键核心退化按规范逐分量使用 Strict fallback；invalid inset 是 AssessmentError | v0.5 §21 | 构造相同核心组件轻微退化、严重失效与 invalid inset 输入 | fallback 的使用范围/数值与规范吻合；invalid inset 不转成空区域或零质量 | BLOCKING |
| Q10-05 | 候选先满足硬安全，再按质量类别 PASS > 可靠失败 > Error；同类可靠失败依 q(critical)、q(uncovered) 比较 | v0.5 §§12、18–21 | 构造候选对并逐项观察实际 winner | 任何质量差异不能掩盖硬安全；PASS 候选不会输给失败/Error | BLOCKING |
| Q10-06 | 候选质量相同后比较 preferred tier、route length、turns、稳定 tie-break；全 Error 候选不访问缺失质量值 | v0.5 §§12、16、21 | 同类/全 Error 确定性候选排序测试 | 选择顺序和重复运行结果符合规范；没有 unknown→0 排序 | BLOCKING |
| Q10-07 | Repair 仅从可靠 Insufficient 硬安全候选触发；每次一个完整目标相对分量、完整重评、首次策略 PASS 即停止 | v0.5 §§22–24 | Repair fixture、evaluator 调用顺序/计数、stop condition | 初始 PASS/Error/D2/D3/非法输入零 repair；有效 repair 次序和停止点准确 | BLOCKING |
| Q10-08 | Repair 严格遵循 §23 七层词典序：1) safety eligibility；2) 相对当前候选必须 strict quality improvement；3) 在 eligible improved trials 中按完整重评后的 CoverageQuality comparator 排序；4) 更低 legal transition cost；5) stable component ID；6) entry index；7) direction | v0.5 §§22–24，尤其 §23 | 构造分别在第 2/3/4/5/6/7 层发生冲突的多 trial fixture，并 deterministic replay | 不合格/未改善 trial 先淘汰；剩余 trial 严格按 3→7 层决胜，结果可复现且不引入加权分数 | BLOCKING |
| Q10-09 | 最终选中结果保留真实 provenance；耗尽时保留最佳 hard-safe canonical/residual 并 ReviewRequired | v0.5 §§22–24、29 | success/exhausted Artifact 与 UI roundtrip | attempted/applied 和 before/after 真实；耗尽后 gate 拒绝上传 | BLOCKING |

### A10 — Auto planner

| ID | 要求 | 来源 | 验证方式 | PASS 判据 | 未满足分类 |
| --- | --- | --- | --- | --- | --- |
| A10-01 | Auto 用冻结能力/拓扑选择最简单适用策略：单调采用 SimpleMonotone，需要分解才升级 BCD | v0.5 §15、M00–M02 | Auto fixture 与 resolved source/strategy/version 检查 | M00/M01 simple，M02 按能力升级并保留原因 | BLOCKING |
| A10-02 | Planner 使用 coverage target C−O 生成覆盖；NavigationArea 只约束静态运动许可，不自动变成覆盖目标 | v0.5 §§5–10、15–17 | 目标/导航差异 fixture、path roles 与 evaluator 交叉检查 | 附加 N−C 区域只有许可 route/transit；不存在 synthetic coverage | BLOCKING |
| A10-03 | Auto 不在规划结果发布阶段隐式重新规划/重排/重评估；requested/resolved identity 可持久化 | v0.5 §§15、27–30；V05-08-DC-v1 | 调用计数、Artifact roundtrip、identity mutation | 结果发布不调用 planner；版本不匹配 stale | BLOCKING |
| A10-04 | Repair exhaustion / coverage insufficiency alone 不得触发 Auto strategy escalation；若 capability/topology 已解析为 SimpleMonotone，则 repair exhausted 可产生 ReviewRequired，但 resolved strategy 必须保持 SimpleMonotone，不得改写为 BCD 或伪造 PlannerEscalated | v0.5 §15 | 构造 capability/topology 明确适用 SimpleMonotone、但 coverage repair 最终仍不足的确定输入；检查 source、resolution reason、issue、readiness 与 Artifact | requested planner=Auto；resolved strategy=SimpleMonotone；repair failure alone 不改变 strategy/escalated；最终状态按 coverage/readiness 规则处理 | BLOCKING |

### U10 — 上传与普通 Mission 兼容

| ID | 要求 | 来源 | 验证方式 | PASS 判据 | 未满足分类 |
| --- | --- | --- | --- | --- | --- |
| U10-01 | 唯一 backend authorization 是 PlanningArtifactCodec 接收 Artifact 与 current Task 的 uploadAllowed；仅 current/non-stale Success + Ready/ReadyWithWarning + 合法 canonical certificate 放行 | v0.5 §27；V05-08-DC-v1 | codec、adapter、MissionController、PlanMasterController 真调用链 | 所有 gate 条件必须同时满足；改变任一条件则拒绝 | BLOCKING |
| U10-02 | ReviewRequired、DiagnosticOnly、None、InvalidInput、Failed、stale、缺失/不匹配/损坏 Artifact、AssessmentError、D2/D3 均 fail closed | v0.5 §§13–14、27–30 | 各拒绝类型逐项经后端实际 callback | Mission items/序列零变异、无部分转换/发送 | BLOCKING |
| U10-03 | QML/toolbar 是入口提示，实际 PlanView callback 后 C++ 必须再次预检；不得 bypass 真实 callback | v0.5 §§27、31；V05-09 contract | CoveragePlanViewUITest/PlanViewTest 实际 GUI callback 路径 | UI 可见性与后端 gate 双重验证；调用路径完整可追溯 | BLOCKING |
| U10-04 | 整计划转换拒绝时保持 fence、rally、非 Marine items 与 mission sequence 不变；无部分上传 | V05-08-DC-v1；V05-09-DC-v1 U1–U7 | MissionController/PlanMasterController 的失败原子性与顺序回归 | 拒绝之前/之后 mission 序列和 side data 完全一致 | BLOCKING |
| U10-05 | 普通 QGC .plan、空计划、home-only 及非 Marine 任务项保持现有上游 load/save/send 行为 | AGENTS.md §6；P2 v0.5 §§28–30；QGC regressions | MissionManager/MissionController/PlanMasterController/CustomPluginIntegration 回归 | 已有正常输入与结果一致，无 Marine extension 时不受门禁误伤 | BLOCKING |
| U10-06 | 分别回归普通 mission、允许的 ordinary + D0/D1 mixed mission、MissionSettings end action、home-only clear、有效空计划 clear 和 ordinary .plan file | V05-09-DC-v1 U7；MissionManager/PlanMasterController actual API | technical design §11 对应 fixture；覆盖真实 load success/failure 与 send callback | 普通序列/end-action 保持；合法 empty/home-only 按既有行为 clear；load refusal 零写入且不得误判为空；允许混合整计划转换 | BLOCKING |

### UI10 — 结果展示与共享高度图

| ID | 要求 | 来源 | 验证方式 | PASS 判据 | 未满足分类 |
| --- | --- | --- | --- | --- | --- |
| UI10-01 | CoveragePlanningPresentation 只呈现 C++ 已发布事实，不在 QML 重新规划、评估、认证、修补或授权 Mission | v0.5 §31；V05-09-DC-v1 | API/QML review、result matrix tests | 所有 readiness/quality/issue/tier 由 backend 字段提供 | BLOCKING |
| UI10-02 | 真实 QML/PlanView 用例覆盖 D0–D3、Ready/Warning/Review/Failed/InvalidInput、coverage/transit、issues/suggestions/residual/provenance/stale | v0.5 §§13–14、25–26、31；V05-09-DC-v1 | CoveragePresentationTest 与真实 PlanView callback suite | 各状态视觉/文本不同且符合发布模型；D2 标不可执行，D3 无伪连线 | BLOCKING |
| UI10-03 | 高度图产品路径保持可见且被实际测试；不得折叠、隐藏、禁用组件或绕过 callback 规避警告 | Owner Decision V05-09-R2；V05-09 contract | CoveragePlanViewUITest 显示图表并实走保存/上传 callback | 有效图表、完整真实路径、严格日志检查持续启用 | BLOCKING |
| UI10-04 | 共享 TerrainProfile 对零距离、重复距离、恒定高度与未知 AMSL marker 使用有限、非退化的展示轴/marker；不改写 Mission 原始数据 | V05-09 R2 F1 根因证据；TerrainStatus.qml/TerrainProfile.cc API | 实际 chart 数据、轴和严格日志；PlanView 保存回归 | 图表尺寸非零；轴 finite 且有序；曲线不含 Inf；未知点保持未知语义；无 QPainter warning | BLOCKING |
| UI10-05 | 恒定 altitude 50m、Marine 平坦 altitude 0m、普通 zero-distance 航线和真实 Plan Save As 均通过 | V05-09 R2 F1/F3 machine evidence 与当前 focused tests | 相关 focused suites 和 PlanViewTest 实际执行 | 数据/行为与既有 F1 修复一致；不得用历史结果替代当前本轮记录 | BLOCKING |
| UI10-06 | 人工 checklist 分别覆盖 M00/M04/M05/M07/M08/M09 的准备、操作、路径/readiness/residual/issues/upload/save-reload 和证据 | v0.5 §§32–33；technical design §16 | 在 Windows GUI 上逐场景人工执行并绑定 fixture/build identity | 每列有可观察结果；D2/D3 不进入 Mission；ReviewRequired/stale 拒绝；报告不将人工观察冒充机器断言 | BLOCKING（若宣称 GUI acceptance 完成） |

### T10 — 共享 terrain 回归

| ID | 要求 | 来源 | 验证方式 | PASS 判据 | 未满足分类 |
| --- | --- | --- | --- | --- | --- |
| T10-01 | 实际 PlanView profile 对 zero mission distance、重复距离、constant AMSL 及 unknown AMSL 首 marker 生成有限坐标和 finite ordered 非退化轴 | Owner Decision V05-09-R2 F1；TerrainStatus.qml/TerrainProfile.cc | CoveragePlanViewUITest 真实 chart 数据及轴断言、严格日志检查 | chart 宽高非零；axis finite 且 strictly ordered；曲线无 Inf；无 QPainter invalid/extreme coordinate warning | BLOCKING |
| T10-02 | terrain missing/unknown 不伪装成数值零或 Inf；display bound 不改写 mission/terrain 原始事实 | V05-09 R2 F1 生产修复；v0.5 §§18–21（数据可用性原则） | known/unknown samples 对照、source data 前后比较 | unavailable 状态保持 unknown；仅渲染坐标用有限 display bound；原始路径/海拔/availability 不变 | BLOCKING |
| T10-03 | 图表保持可见，Save As 走真实 PlanView/Mission 路径；不通过折叠、隐藏、禁用、warning suppression 或降低 strict logging 绕开失败 | Owner Decision V05-09-R2 | CoveragePlanViewUITest + PlanViewTest Save As 实际运行及日志检查 | 产品图仍出现，真实保存 callback 完成；没有过滤/期望 QPainter warning | BLOCKING |
| T10-04 | 记录 missing terrain、constant 50m、Marine constant 0m 和 zero-distance 航线的逐项运行结果 | V05-09 R2 R2 report/machine evidence；V05-10 validation plan | 当前 build focused suite 逐 case 报告 | 每一 case 有 fixture/input、输出轴/曲线检查、状态和原始日志；历史结果不算本轮 PASS | BLOCKING |

### R10 — 回归验证与 F3 归因

| ID | 要求 | 来源 | 验证方式 | PASS 判据 | 未满足分类 |
| --- | --- | --- | --- | --- | --- |
| R10-01 | 运行 Marine planner/geometry/safety/quality/repair/identity/persistence/integration/upload focused suites 与 Mission/QGC affected regressions | AGENTS.md §8–9；v0.5 §§32–33；V05-08/09 contracts | Windows 主平台依 tools/README.md、ci-overview 执行 | 每套件逐 case 记录，所有必须 case 无未解释 FAIL | BLOCKING |
| R10-02 | 运行指定 V05-09 terrain/PlanView 产品路径测试，保留 strict log check | Owner Decision V05-09-R2 | 实际执行 CoveragePlanViewUITest、PlanViewTest 及受影响 suites | 图表路径启用、真实保存/callback 成功，无过滤或期望 warning | BLOCKING |
| R10-03 | 对当前出现的失败逐案在基线 4a948c8842a915fe3125322ec9eec9e66a007fb4 复现或修复；包括已报告三个 localization raw FAIL 的复核 | Owner Decision F3；V05-09 R2 report | 等价 Windows build/runtime 下分别执行同一精确用例 | 基线具体复现才可标 PRE-EXISTING；不复现/条件不等价则 unresolved；本包引入则修复 | BLOCKING |
| R10-04 | 至少重新核对 MissionManager::_testErrorAckFailureStrings、PlanView::_testPlanViewStates、_testRoverWaypointOnEmptyPlan 的当前与基线 raw 结果 | V05-09 R2 report | 同配置逐用例运行并存原始输出 | 每例有两侧状态、SHA、命令、环境和可比日志；结论与证据一致 | BLOCKING |
| R10-05 | 现存 legacy opt-in SITL SKIP 与新 SITL 未执行状态不得写成 PASS | v0.5 §§33、36；V05-09 evidence | 检查测试报告摘要与环境变量/日志 | skip 原因明确，新旧执行状态精确分类 | BLOCKING（若错误宣称通过） |

### S10 — SITL 场景与证据

| ID | 要求 | 来源 | 验证方式 | PASS 判据 | 未满足分类 |
| --- | --- | --- | --- | --- | --- |
| S10-01 | v0.5 SITL 使用本合同冻结的最小 acceptance set；legacy P2-13 S01–S04 仅作历史回归证据，不自动满足 | v0.5 §§33、36；AGENTS.md §8；独立设计审查 RQ-01 裁决 | 审核 scenario contract 与实际 fixture/backend callback | 每个情景有固定 scenario/fixture identity、输入、预期状态、执行/拒绝步骤、停止条件、oracle、日志字段 | BLOCKING |
| S10-02 | ALLOW+EXECUTE 场景固定为 M00 D0/Ready 与 M04 D1/ReadyWithWarning；必须经当前 `Task → Integrated Artifact → backend upload gate → MissionAdapter → MissionController/PlanMasterController → ArduRover SITL` 产品后端链上传并执行；SITL 不强制依赖 QML/PlanView 自动化 | v0.5 §§27、33、36；RQ-01 resolved decision | 连接受控 ArduRover SITL，使用 current/non-stale matching Artifact；核对 gate、转换、上传、Mission Start ACK、ACTIVE、ordered waypoint progression、Mission Complete | 两个 allow 场景均完整执行；仅产生预期 Mission waypoint；无 Marine 自动 RTL；readiness/warning 与 Artifact/source 一致 | BLOCKING |
| S10-03 | CONNECTED BACKEND REJECTION 场景固定为 M08 ReviewRequired、M05 D2/DiagnosticOnly、M09 有效 D3，以及一个由此前有效 M00 Artifact 经 planning-critical input mutation 形成的 stale 场景 | v0.5 §§13–14、27、29–30、33、36；RQ-01 resolved decision | 在已连接 SITL vehicle 环境沿真实 backend send 路径触发拒绝，不进入 AUTO execution | backend refusal；0 `writeMissionItems`；0 new executable mission；0 partial conversion/send；mission/sequence 不变；不得误清已有 mission；send/sync state 不 stuck | BLOCKING |
| S10-04 | ALLOW+EXECUTE 场景必须显式处理 `IngressNotAssessed`：SITL vehicle 初始/current position 与首个 canonical Mission waypoint 在 scenario contract 记录的明确容差内共址；不得把未认证 ingress 混入 v0.5 safety evidence | v0.5 §§9、12、31；现有 legacy fixture 的 `anchorFirstPathPoint()` 工程先例 | 运行前记录 vehicle start、first canonical waypoint、距离及 tolerance | 距离在合同容差内后才允许将后续执行作为 v0.5 SITL evidence；超出则场景不得开始/不得计 PASS | BLOCKING |
| S10-05 | 默认验证环境复用已验证的 P2-13K ArduRover 4.7.0 环境：Docker `qgc-ardurover-sitl:rover-4.7.0`，image ID `sha256:001f20d07215138f2cb4aebc8eb691c8f4c3a23825a01f343b3f5397d262d3f0`，frame `rover`，home `47.397742,8.545594,488,0`，TCP `127.0.0.1:5760`；记录 WP_RADIUS/WP_SPEED/TURN_RADIUS/ATC_TURN_MAX_G/WP_ACCEL/WP_JERK、command/env、plan hash、fixture/seed；无随机 seed 时记 N/A | v0.5 §§33、36；P2-13K regression environment | 审核 manifest、容器/image identity、参数快照、原始日志与 plan | 能重建运行上下文；环境偏离必须事先记录并说明等价性/影响，不能静默替换 | BLOCKING |
| S10-06 | 无批准场景合同、环境未提供或 SITL 未运行时，不宣称 SITL 通过；历史 opt-in skip 也不得改写为本轮 PASS | v0.5 §§33、36 | 对照本合同及原始证据 | 明确 BLOCKED/ENVIRONMENT BLOCKED/NOT EXECUTED/SKIP，并不伪报通过 | BLOCKING（若宣称通过） |

### H10 — 人工验收和现场边界

| ID | 要求 | 来源 | 验证方式 | PASS 判据 | 未满足分类 |
| --- | --- | --- | --- | --- | --- |
| H10-01 | GUI 验收使用真实 Windows QGC build、Marine complex item 和 PlanView 路径，并记录版本、截图/录屏、计划文件、日志与检查项 | v0.5 §§32–33；AGENTS.md §9 | 人工运行 checklist，机器/人工证据互相绑定 | 状态、几何、stale、上传拒绝、普通 Mission 和 terrain UI oracle 均满足 | BLOCKING（若宣称 GUI 验收完成） |
| H10-02 | GUI 不能替代机器 M00–M09、Mission gate 或 regression 用例；截图不能证明 backend safety authorization | v0.5 §§27、31–33 | 审核人工证据映射与机器 suite | 每项结论有正确的证据类型支撑 | BLOCKING |
| H10-03 | Field 状态必须记录 FIELD VALIDATION DEFERRED BY FROZEN SPEC；P1_REAL_USV_FIELD_VALIDATION_PROTOCOL.md 为 DEFERRED AND NOT EXECUTED | v0.5 §33 明示 P1 real-USV field validation deferred/not executed，并要求按 P1 protocol 完成前置工作再评估 P2 real-USV acceptance；§36 禁止无证据宣称外场前置完成；AGENTS.md §8–9 | 核对报告和协议，不进行现场推断 | 不将设计、软件、SITL、GUI 结果提升为实船接受；保留 deferred 状态 | NON-BLOCKING / DEFERRED；对外错误声称则 BLOCKING |
| H10-04 | 历史 P2-13K SITL 仅回归证据，不认证 v0.5 或 field readiness | AGENTS.md §8；v0.5 §§33、36 | 检查报告措辞和证据链 | 历史运行与本包结论分开 | NON-BLOCKING；错误认证则 BLOCKING |

### FZ10 — 冻结审计、报告与停止门

| ID | 要求 | 来源 | 验证方式 | PASS 判据 | 未满足分类 |
| --- | --- | --- | --- | --- | --- |
| FZ10-01 | 最终 implementation report 对应唯一 HEAD/工作树，列出变更文件、架构决策、build/test/core static analysis/SITL/GUI/field 状态及未执行项 | AGENTS.md §9；v0.5 §§32–37 | 将报告和 manifest、Git state、原始证据核对 | 每个结论都能定位到实际证据；Clazy 不可用时标 supplemental static-analysis SKIP | BLOCKING |
| FZ10-02 | 冻结审计逐条核对 v0.5 语义、Task/Artifact 版本、identity、质量校准、D0–D3、候选/repair、backend gate、普通 .plan 兼容和所有 M 场景 | AGENTS.md §§5–8；v0.5 §§32–37 | Implementation/code/tests/evidence trace matrix | 所有冻结要求有证据；无未批准语义偏差和未归因回归 | BLOCKING |
| FZ10-03 | 作者不自我批准；独立设计 reviewer 先审合同/设计，实施后的独立 reviewer 再审代码和证据 | Owner task authorization；AGENTS.md §2–3 | 审核记录含独立 reviewer 身份/结论 | 独立结论存在；实现作者没有自签通过 | BLOCKING |
| FZ10-04 | 本迭代仅文档审计，交付为 READY FOR INDEPENDENT REVIEW 或 BLOCKED 并列精确根因；不 stage/commit/push、不开始 V05-11 | Owner authorization 和明确停止门 | 查看 Git index、工作树、最终报告措辞 | 仅预期两份文档修改、index 无变化、无后续阶段操作 | BLOCKING（若越界） |

## 4. 本轮文档交付状态

本轮只创建/修改本合同与 V05_10_TECHNICAL_DESIGN.md 两份文档。未编辑产品、生产代码、测试、冻结语义或 SITL fixture；未运行 build、test、SITL、GUI 或 field validation；未创建机器结果；未 stage/commit/push。文档草案的最大许可状态为 V05-10 DESIGN READY FOR INDEPENDENT REVIEW；独立审查须处理下列 RQ-01、M 场景证据充分性及 F3 基线逐案归因要求。

### 相关证据缺口（不是新增语义决定）

| ID | 缺口 | 处理条款 | 当前状态 |
| --- | --- | --- | --- |
| GAP-01 | M00–M09 有些语义由 planner/evaluator/publication/UI/Mission 多个 suite 组合断言；单一全链路 fixture 不齐全。 | M10-00–M10-09、R10-01；采用“每个 Mxx 一个 canonical fixture/scenario identity，允许同一 identity 下组合多层证据”的规则 | RESOLVED BY DESIGN REVIEW；实现阶段若任一冻结断言无法绑定同一 scenario/Task/Artifact/binary identity，则 BLOCKING |
| GAP-02 | 三项 localization raw FAIL 的当前重现状态及等价基线归因尚未重新执行。 | R10-03、R10-04 | NOT EXECUTED；未归因前 BLOCKING |
| GAP-03 | P1 real-USV field validation 仍 deferred/not executed。 | H10-03 | DEFERRED BY FROZEN SPEC；不是 V05-10 软件条款 |

## RESOLVED REVIEW DECISIONS

| Decision ID | 原始歧义 | 当前实现事实 | 独立审查裁决 | 影响条款 |
| --- | --- | --- | --- | --- |
| RQ-01 | v0.5 §§33、36 要求 v0.5 SITL 场景/证据，但没有规定全部 M 场景都必须进入 SITL，也未冻结固件/环境矩阵、停止条件、seed 或日志 schema。 | MarineSITLValidationTest 当前是 opt-in legacy P2-13 S01–S04/offline analysis/diagnostic；已有 `anchorFirstPathPoint()` 说明历史 fixture 主动规避未认证 ingress。 | RESOLVED BY INDEPENDENT DESIGN REVIEW：SITL 最小集冻结为 M00/M04 ALLOW+EXECUTE，以及 M08/M05/M09-valid-D3/stale CONNECTED BACKEND REJECTION；SITL 验证真实 backend product upload chain，不强制 GUI；allow 场景车辆起点锚定首 canonical waypoint；默认复用 P2-13K ArduRover 4.7.0 环境并完整留证。 | S10-01–S10-06、E10-04、FZ10-01–FZ10-02 |
