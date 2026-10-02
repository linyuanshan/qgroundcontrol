# V05-06 Implementation Report

## Scope and state

V05-06 implementation scope: **PASS**, with the owner-approved clang-format deferral described below.
独立审核仍待完成；本报告不变更治理状态，也不表示允许提交或进入下一工作包。

工作区：qgroundcontrol；分支：feature/marine-p2-complex-coverage。
基线及当前 HEAD：99f7f93017b49979c8ea22775a6c73056c468a63。
V05-06A/B/C 已完成独立代码审核；V05-06D 执行累计审计、最终验证及证据报告；D-R2 另获授权修正一项陈旧测试断言。
D/R2 未修改任何 production 源文件或生产行为。原 25 个修改全部保留且哈希不变；
R2 仅新增 CoverageInspectionPlanCreatorTest.cc 的 tracked 修改，共 26 个未暂存修改。
唯一新增 source-tree 文件为本报告。

本包实现目标驱动 BCD 分解、多分量/孔洞目标、target/track 分离的单元覆盖、N−C 内的安全 Transit、
Preferred/Hard BCD 候选、coverage-first 确定性排序、当前 BCD semantic、Auto 的真实 BCD 委派、
旧 BCD semantic 的 stale 行为，并移除生产 BCD 的无条件边界支持。
未实现 V05-07 repair、V05-08 readiness/diagnostics/full persistence/upload gate、V05-09 QML；
未执行 V05-10 最终验收。

完整相关回归为 **56/57 PASS**；唯一失败是重新核验的历史 MissionManager 本地化缺陷。
Repository-wide selected regression set: **not globally clean**。
此结论与本包 implementation scope PASS 分开记录。

## Architecture and semantics

Task != Plan != Mission；规划仍为纯 Marine 领域逻辑。CoverageTarget 定义覆盖义务，
导航/执行区域仅约束中心线及 Transit；N−C 不成为额外覆盖目标。

- BCD id：marine.coverage.bcd；当前 semantic：bcd.v0.5.v1。
- 全局 planning semantic 保持 p2.v0.5.planning.1。
- 指纹回归固定值：548e77c9c6d0f59cea9eac2905c9f8669c4b735eccd6bc336b606a837e2b5235。
- 当前 BCD semantic 支持；bcd.pre-v05-06.v1 与 bcd.v0.5.pending-v05-06 不支持，旧 artifact 恢复为 stale。
- 采用单一全局 sweep；不会因覆盖/安全失败进行 alternate-angle 或参数搜索。

V05-06 仍采用 V05-08 之前的 transitional result model：

- Coverage Insufficient → Failed / CoverageIncomplete → 不发布 canonical path。
- Coverage AssessmentError → Failed / GeometryFailure → 不发布 canonical path。

这不是最终 v0.5 §12 的 Success + ReviewRequired；最终 MissionReadiness 映射属于 V05-08。

Artifact v3 保持 resultContract = InfrastructureOnly。当前 BCD identity 可保存并用于 stale matching；
匹配 artifact 恢复后仍为 Unplanned、无可执行 planning path，不自动重规划、不生成 mission items。
完整 plannerSource/CoverageQuality 持久化、canonical executable-path restore 与 upload readiness 仍属 V05-08。
本报告中的“upload gate 未实现”指最终 V05-08 readiness 门禁；既有 InfrastructureOnly 恢复阻断仍保留。

## V05-06A — target-driven BCD decomposition

decomposeBoustrophedon 接收 CoverageTarget PolygonRegionSet2D；保留单分量事件驱动 slab 算法，
通过内部 helper 独立处理各分量。支持单/多分量及分量内孔洞；无效输入原子失败，
空目标返回 InvalidInput / EmptyCoverageTarget。

局部 canonical ordering key 对 open ring 枚举所有 cyclic starts 和 forward/reverse traversal，
按严格 (xM, yM) 字典序取最小序列；holes 各自 canonicalize 后排序。
排序不 rounding、不改变用户几何，不增加公共 API。
全局 cell IDs 从 0 连续编号；adjacency 偏移、排序、去重并防溢出，不跨目标分量造连接。

分解测试验证 component 顺序、outer 起点/方向、hole 表示变化后的完全确定性，
以及 cell finite/valid/hole-free/monotone、区域并集等于完整目标和无跨分量 adjacency。

## V05-06B — target/track-separated cell coverage

生产调用四参数 generateCellCoverage(cells, activeTrackRegion, swathWidthM, navigationAngleDeg)。
lane lattice 来自目标单元；中心线被活动执行 track 区域约束。
多个可用区间采用确定性选择；单元内部连接由 StaticSafeRouter 在活动 track 中完成并标记 Transit。

Transit 可以离开目标、使用 N−C，但不计入覆盖足迹。不可达/无法覆盖的目标义务不因 track clipping 消失：
最终 evaluator 始终评估原始完整 CoverageTarget。路径长度/turn metrics 来自实际路径和 legRoles。
历史三参数 overload 保留为受限回归基元，不用于 v0.5 production BCD。

## V05-06C — production BCD and Auto integration

生产 BCD pipeline 为：

1. buildCoverageGeometry。
2. buildSafetyTrackRegions。
3. 对 CoverageTarget 调用 decomposeBoustrophedon。
4. 四参数 generateCellCoverage。
5. orderCellTraversals。
6. 三参数 assembleComplexCoverage(trackRegion, cells, visits)。
7. evaluateSafetyCandidate。
8. 对完整目标调用 evaluateCoverageQuality。

Preferred 和 Hard 候选分别生成并验证整条路径；H/E 不降低，Task 中 P 不修改。
候选先按 reliable policy PASS > reliable FAIL > AssessmentError 排序；
可靠候选复用统一 CoverageQuality comparator，然后按 preferred satisfaction、path length、
turn count、稳定 generation order 破同值。AssessmentError 没有 CoverageQuality key。

Auto 的 SimpleMonotone applicable 路径继续使用 Simple；
仅 topology/capability not applicable 才委派真实 BCD，并保留 resolved strategy/failure provenance。
已选择的全局角度作为 delegated Manual angle 传入；Simple coverage/safety failure 不触发 BCD retry。

生产 BCD 不调用 buildCoverageFreeSpace 或 generateBoundaryCoverageSupport，
不从 execution region 分解目标，且组装不传 boundary components。
BoundaryCoverageSupport.* 保留用于历史回归及可能的 V05-07 repair primitive；
文件存在不表示生产 BCD 具有 unconditional boundary support。

## Numerical corrective work

- R1：MarinePlanIntegration 暴露 CoverageQuality NumericalFailure。
- R2：定位独立 direct Intersection / Difference 的格网面积差异，观测到两条约 1 mm 宽的长条带。
- R3：尝试通用 subject-complement Intersection 后，因违反一般 commutativity/partition 行为而被否决，
  该修改已完整回退。
- R4：恢复公共 direct Intersection；CoverageQuality 改为单一 residual truth，
  并通过当轮要求的 focused/regression 验证。本轮 D 另行运行最终完整集合。

当前 U = T − CoveredFootprint；coveredArea = targetArea − uncoveredArea。
BoundaryShortfall = U − CriticalCore；critical residual = U − (U − CriticalCore)。
evaluator 内无独立 direct Intersection 的面积分割认证；公共 PolygonRegion.cc 与 HEAD 无 diff。
几何/面积运算失败仍为 AssessmentError；无效标量仍拒绝，不将失败变成 policy PASS。

面积容差保持 max(0.01 m², 1e−6 × A)，Standard CAL-01 = 0.99、
CAL-02 = 0.50 m、coverage-quality.v1 均未变。
Standard/Strict 共享 metrics/residual truth；Strict 仍只接受 Complete。

Geo regression 在 R4 诊断中测得 targetArea ≈ 7637.381358 m²、
uncoveredArea ≈ 0.0018355 m²、quality = Complete、passesRequirement = true。
这些数值是 fixture 证据，不是产品常量；本轮无 instrumentation 修改，
最终 binary 重新通过该 fixture 及整套 evaluator/BCD/integration tests。
原数值记录位于 build/v05-06c-r4-initial-focused.log 和 build/v05-06c-r4-evidence.json。


D-R2 的 regression-test compatibility correction：原 integration test 在 Success、非空路径、
path/role consistency 和 metrics 断言已通过之后，仅因成功消息不再包含 Boustrophedon 而失败。
删除 message-string 依赖，改为验证 plannerSource 存在、requested/resolved ID 为 marine.coverage.bcd、
semantic version 等于当前 BoustrophedonVersion、Resolved / None、escalated = false。
未增加 production API，未把文本断言换成 BCD 文本断言。

The integration test previously depended on human-readable planner message wording. V05-06 exposes
planner identity through structured planner-source semantics, so the stale message-string assertion
was replaced by a stable semantic assertion. No production behavior was changed.

## Changed files

基线到工作区的 26 个 tracked 修改（原 25 个，加 R2 唯一测试修正）：

- custom/src/Marine/Planning/AutoCoveragePlanner.cc
- custom/src/Marine/Planning/BoustrophedonCoveragePlanner.cc
- custom/src/Marine/Planning/BoustrophedonDecomposition.cc
- custom/src/Marine/Planning/BoustrophedonDecomposition.h
- custom/src/Marine/Planning/CellCoverage.cc
- custom/src/Marine/Planning/CellCoverage.h
- custom/src/Marine/Planning/CoverageQualityEvaluator.cc
- custom/src/Marine/Planning/CoverageStrategySemantics.h
- custom/test/Marine/AutoCoveragePlannerTest.cc
- custom/test/Marine/AutoCoveragePlannerTest.h
- custom/test/Marine/BoustrophedonCoveragePlannerTest.cc
- custom/test/Marine/BoustrophedonCoveragePlannerTest.h
- custom/test/Marine/BoustrophedonDecompositionTest.cc
- custom/test/Marine/BoustrophedonDecompositionTest.h
- custom/test/Marine/CellCoverageTest.cc
- custom/test/Marine/CellCoverageTest.h
- custom/test/Marine/CoverageComplexItemTest.cc
- custom/test/Marine/CoverageComplexItemTest.h
- custom/test/Marine/CoverageGeometryTest.cc
- custom/test/Marine/CoverageInspectionPlanCreatorTest.cc
- custom/test/Marine/CoveragePlannerTest.cc
- custom/test/Marine/CoverageQualityEvaluatorTest.cc
- custom/test/Marine/CoverageQualityEvaluatorTest.h
- custom/test/Marine/MarineGeometryTest.cc
- custom/test/Marine/MarineGeometryTest.h
- custom/test/Marine/PlanningInputIdentityTest.cc

本轮唯一新增 source-tree 文件：docs/marine/V05_06_IMPLEMENTATION_REPORT.md。
build 日志、JUnit、selection、helper 及 JSON 证据均为 ignored 验证产物，不计入 implementation files。

## Validation

Windows/MSVC x64 最终增量 build：**PASS**，exitCode 0；日志 build/v05-06-r2-final-build.log。
经 VS 18 BuildTools VsDevCmd.bat 初始化后执行：

    C:\Qt\Tools\CMake_64\bin\cmake.exe --build build/P0-01-marine-debug --target QGroundControl --parallel 8

最终 Debug/QGroundControl.exe SHA-256：

    DB0B45B3C364A7019A401A11B144ADEC21C0F40A521BEBC13457FB8042E27C9E

为使单项复测真实执行新断言，先编译 R2 测试改动（build/v05-06-r2-prepare-build.log，PASS），
确认 CoverageInspectionPlanCreatorTest 单项 PASS，再执行最终增量 build（no work to do，exitCode 0）。
原 25 个源码 SHA-256 保持一致；唯一新增测试修改及最终 binary hash 单独记录。
本报告仅采用 R2 最终 binary 的单项、18-suite 和完整 57-suite 结果。

- 单项 CoverageInspectionPlanCreatorTest：**PASS**，build/v05-06-r2-plancreator.log / .xml。
- Focused：**18/18 PASS**，exitCode 0，build/v05-06-r2-focused.log / .xml。
- 完整相关 regression：**56/57 PASS**，exitCode 8，唯一失败 MissionManagerTest。
  精确 57 个唯一 suite 名称见 build/v05-06-regression-57-selection.txt，
  全部最终结果见 build/v05-06-r2-regression-57.log / .xml。
- 初次 sandbox run 因测试 AppData/cache 写入拒绝导致 strict-log failures，已中断并单独保留；
  在任务允许的正常 Windows 测试权限下，MissionControllerTest 单独复测 20 cases / 0 failure，
  然后完整重跑 57-suite 集合。最终 MissionControllerTest/Tree 均 PASS。
- git diff --check：**PASS**。
- clang-tidy 22.1.8：保留本次 D 已完成的 **15 个 .cc PASS**（5 production、10 test），exitCode 均为 0；
  使用当前 compilation database、仓库 .clang-tidy、MSVC 环境及 /Y-，不使用 --fix。
  非致命 warnings 保留在逐文件日志中；结果见 build/v05-06-clang-tidy-results.json。
  这 15 个文件在 R2 中均未变化，故按 R2 授权保留已有 evidence；新增修改的 PlanCreator test 不在该 15-file run 中。
  R2 该文件通过编译、测试、diff-check，新增断言无超过 120 列行；未执行格式重写。
- vehicle-null-check / qt-translate-noop-check：**PASS**，均 exitCode 0。
- clang-format 22.1.8：**FAIL / owner-approved non-functional deferred debt**，exitCode 1；
  对 25 个修改 C++ 文件全文件检查，14 个文件出现 416 条格式诊断。
  不将全部诊断归因于新增代码；新增 Auto BCD 委派处明确有诊断。
  工具不在 PATH，但安装目录可用；不能记为 unavailable 或 PASS。
  Owner 明确要求“继续跑完整……不修改源码格式。把 clang-format 记录为已知非功能性 deferred debt。”
  因此本轮不修改源码格式；日志 build/v05-06-clang-format.log。
- 正常 pre-commit：**ENVIRONMENT BLOCKED**，正常尝试一次，exitCode 1，hooks 尚未执行。
  sqlite3.OperationalError: attempt to write a readonly database；随后写日志触发 PermissionError。
  缓存日志位置：C:\Users\Lin\.cache\pre-commit\pre-commit.log；本轮捕获 build/v05-06-r2-precommit.log。
  命令为 .venv/Scripts/pre-commit.exe run --files，显式传入 26 个修改文件及本报告，未暂存。
  未使用 --no-verify，未修改 cache 权限、未使用独立缓存或其他绕过。

测试使用 QT_QPA_FONTDIR=C:\Windows\Fonts，并保留配置的 offscreen/software 环境。
MarineSITLValidationTest 仅为本轮自动化 suite，不代表真实车辆、field validation 或完整 V05-10 SITL 验收。
机器证据：build/v05-06-evidence.json，包含精确命令、suite 结果、源码/binary/log hashes 和边界审计。

## Known unrelated issues

本轮 MissionManager native JUnit 只有 _testErrorAckFailureStrings 失败，
原始 system-err 为 QString::arg: Argument missing: "框架1", 3。
HEAD 的 translations/qgc_source_zh_CN.ts 中 source = Frame: %1、translation = 框架1；
translation 缺少 %1。PlanManager.cc 调用 tr("Frame: %1").arg(item->frame())。

translations/qgc_source_zh_CN.ts、src/MissionManager/PlanManager.cc、PlanManager.h、
src/Vehicle/Vehicle.cc 的 HEAD/worktree blob 完全相同，相关 diff 为空。
据此重新确认 pre-existing / unrelated to V05-06；未修改 translation 或相关 production。

证据：build/v05-06-r2-mission-manager-MissionManagerTest.xml、
build/v05-06-r2-mission-manager-summary.json、build/v05-06-r2-mission-baseline-proof.json、
build/v05-06-r2-mission-translation-head.txt。
该缺陷未解决；不得写成 57/57 PASS。

## Explicitly deferred scope

- V05-07：conditional Coverage Repair、boundary repair。
- V05-08：最终 MissionReadiness、D2/D3 diagnostics、issues/suggestions、
  完整 plannerSource/CoverageQuality artifact persistence、可执行恢复和 upload gate。
- V05-09：QML/editor/visualization。
- V05-10：最终 M00–M09/freeze/SITL/manual/field acceptance。

未新增 automatic parameter search、StartAnchor/EndAnchor、多 Mission，
未修改 geometry backend、安全算法、CAL、frozen specification、governance、translations 或 QGC core production。

## Review boundary

V05-06 implementation scope DoD：**PASS**，含本次 owner-approved formatting deferral。
最终 build/focused/相关实现回归均满足要求；无新增相关 regression failure，
source boundary 与 no V05-07+ leakage 审计通过，机器证据及报告完整。
完整 repository selected regression set 仍因一个历史本地化缺陷而 not globally clean。

架构偏离：0；超范围 implementation 修改：0。
HEAD 不变；26 tracked 修改未暂存；untracked 仅本报告；index/conflicts 为空；未提交。
下一工作包是 V05-07，但必须另获 owner 授权；本轮未开始。
**STOPPED BEFORE V05-07 — waiting for independent review.**
