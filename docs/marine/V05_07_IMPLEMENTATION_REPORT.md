# V05-07 — Conditional Coverage Repair 实现报告

## 状态与授权

- 工作区：`F:\Projects\qgroundcontrol`。
- 分支：`feature/marine-p2-complex-coverage`。
- 起始及结束 HEAD：`1403062e7a2f4258412962882f370c8ab4f68bc6`；起始工作树 clean。
- 授权来源：项目所有者本轮明确要求执行 V05-07，附件 Design Contract 为本包直接 authority。
  此授权优先于 AGENTS.md 中尚未更新的 V05-06 lifecycle 状态；本包未修改治理文件或冻结规范。
- V05-07A → B → C → D 已按顺序实现和验证；A/B 通过 focused build/test 后才接入生产策略。
- 实现及 T1–T14 证据已备齐，等待独立审核；不将整个仓库检查宣称为全绿。
- 20 个 focused suites PASS；57 个既有回归中 56 PASS，唯一失败为已证明的既有翻译缺陷。
- core 静态检查 PASS；正常 pre-commit 在 hooks 执行前遭遇缓存权限故障。
- 无 stage、无 commit；V05-08 未实施，后续工作包不自动授权。

## 实现架构与决策

### A：完整的目标相对支持分量

新增 `CoverageRepairSupport`，以 `T ∩ activeExecutionRegion` 的完整 outer/hole rings 为有限支持集合。
T 是既有 C/O 纯几何合同生成的 CoverageTarget；active region 只约束安全可行中心线。
这不是把 NavigationArea 周界作为覆盖义务，也不截取任意边界 arc。

复用既有 `BoundaryCoverageSupport` 生成闭环、稳定 ID 和 Coverage roles。
输入经过现有 backend intersection 后具有 lattice 坐标，既有 canonicalization 能消除 ring 起点、
winding、hole/component 顺序的表示差异。每个闭环完整逐段验证在 active region 内；无部分失败结果。
原历史 helper 本身未改变，也未恢复其 unconditional production semantics。

每轮只考虑其 footprint 能减少当前可靠 U 的完整支持环。该 residual 操作仅判定资格，不提供另一种
质量排名；是否接受仍完全由完整 trial 的统一 CoverageQuality comparator 决定。

选择此方案是为了复用已验证的完整边界与路由基元，保留目标/导航分离，避免引入一般 repair framework。
不是全局路径优化，不保证通过边界环修补所有可能的内部覆盖缺口。

### B：共享的逐分量引擎

`CoverageRepair` 由 Simple/BCD 共用；`evaluateCoverageRepairTrial()` 是纯内部 trial 基元，
只接受支持生成器提供的完整闭环。测试直接验证其实际路由、组装、安全和 evaluator 拒绝结果，未引入 mock
coverage evaluator 或 repair 专属质量评分。

对每个合法入口枚举 forward/reverse：

1. 在不变的 active region 中调用 `routeStatic()`，使用其实际 route length 作为 transition cost。
2. 复制保留候选，追加 Transit 连接及一个完整 Coverage 环；去除相邻重复点。
3. 由实际 path/roles 重算全部 lengths、turn count。
4. 通过 `evaluateSafetyCandidate()` 验证整条路径，并逐段验证其仍属于 active tier。
   Preferred qualification 由修补后的全路径重新认证。
5. 调用既有 `evaluateCoverageQuality()`，沿用原 policy semantic identity。
6. 仅当 `compareCoverageQuality(trial,current) == Better` 时保留 trial。

合法 improving trials 按统一 quality comparator、真实 route cost、component ID、entry index、
direction 排序；forward 在最终完全平局时优先。不使用加权评分或 Euclidean route proxy。

一次只应用一个分量，应用后保存全路径事实，重新开始下一轮；PASS 立即结束。
支持集合只生成一次，每次应用消耗一个逻辑分量，已使用分量不可重复。
因此最多应用 `availableComponentCount` 次；无任意 timeout/iteration cap。
Equivalent、Worse、NotComparable、AssessmentError、unsafe、invalid、unroutable 均不能替换当前候选。

### C：策略接入与结果边界

Simple 和 BCD 都先完成原有全部 initial candidates 的生成、独立 hard safety 和统一 quality evaluation。
若任何 initial candidate PASS，全集合跳过 repair，包括初始 coverage FAIL 的其他候选。
否则，每个可靠 Insufficient 候选分别在其原生成 tier 中获得 repair opportunity。
不只修补 initial-best；Hard tier 保持原 H+E，不通过改变 P 获得 fallback。

修补后继续使用原来的 planner-level comparator：认证类别 PASS > reliable FAIL > AssessmentError，
再按同一 quality comparator、Preferred qualification、path length、turn count、稳定生成顺序选择。
这些排序函数未改为 repair 排名。Auto 源码未修改：依旧 resolve/delegate，传递 resolved strategy 与 runtime
repair facts；Simple repair 失败不会使 Auto 升级 BCD。
BCD 只复用本次既有 decomposition 和选定角度，不重新选角、不搜索参数、不更换策略。

新增 `CoverageRepairData` 只保存纯 domain runtime facts：attempted、可用分量数、最终保留候选、
各步 canonical component path/ID、entry/direction、transition cost、quality before、完整 candidate after。
是否应用 repair 可由非空 steps 精确判定；不能把 attempted 等同于 applied。
保留每个 initial candidate 的记录，可审计全部候选的 repair opportunity 和最佳硬安全事实。
这些数据没有进入 `PlanningResult`、JSON、QML、MissionAdapter 或上传逻辑。

外部过渡语义保持：PASS 发布 Success canonical path；可靠 Insufficient 发布 Failed/CoverageIncomplete
且外部 path 为空，内部 `repairCandidates` 保留最佳硬安全几何。
AssessmentError 沿用现有 Failed/GeometryFailure 过渡行为，不执行 repair，不读取失败 assessment 的
metrics/residual 作为基线。没有提前实施 Success/ReviewRequired 或绕过 InfrastructureOnly 执行阻断。

## 硬合同逐项对照

| 合同 | 落实位置与不变量 |
| --- | --- |
| §§3、15 | engine/trial 先检查可靠 Insufficient；初始与 trial AssessmentError 均不能修补或替换基线 |
| §4 | Simple/BCD 在 repair 前对全集合执行 initial PASS 检查 |
| §§5–6 | 完整 `T ∩ active` canonical rings；可靠 U 驱动资格，不产生 N 周界覆盖义务 |
| §§7–9 | const 原输入；真实 routing；每个 trial 重算 metrics/turns/full safety/unified quality |
| §§10–12 | 只接受 Better；一次一个；有限集合不重复；统一 quality/route/ID/entry/direction 字典序 |
| §§13–14 | 保留原 planner comparator；所有受支持 Insufficient initial candidates 都尝试 repair |
| §§16–18 | resolved Simple/BCD 内部后处理；Auto 没有修补或 coverage-failure escalation |
| §§19–20 | 只增加 runtime facts；外部过渡失败不发布路径，内部最佳硬安全事实保留 |
| §21 | 审计实际 identity 后仅升级 Simple/BCD resolved semantic versions；旧身份和 Artifact stale |

## T1–T14 验证

下面列出的 `CoverageRepairTest` cases 均使用真实 geometry、routing、safety、quality primitives。
逐 case Qt JUnit：`build/v05-07-contract-CoverageRepairTest.xml`。
机器映射及最终 binary/source/test hashes：`build/v05-07-evidence.json`。

| 合同 | 精确测试与证明 |
| --- | --- |
| T1 / M00 | `_testPlannerInitialPassAndStandardStrict`：原 4-lane path 保持 8 points；全部 attempted=false；另测 Preferred FAIL + initial Hard PASS 抑制整个集合；重复坐标一致 |
| T2 | `_testAssessmentErrorAndInitialPass`：hard-safe path 携带 NaN metric 和非法 residual 的 AssessmentError；empty target/active 仍不进入支持生成，attempted=false |
| T3 / M07 | `_testSuccessfulIncrementalRepair`：每步 Better、全路径 hard safe、metrics 来自实际路径；两个可用环仅应用一个即 PASS，剩余环不追加 |
| T4 / M08 | `_testFiniteInsufficientAndNoUsefulRepair`：仍不足的最佳候选保留；step 数不超分量数，ID 不重复；再次等价修补不改变事实 |
| T5 | 同上：实际 Equivalent trial、unsafe 全环、invalid 环、无法路由分量、真实 trial AssessmentError 均拒绝；保留 path/roles/metrics/quality；统一 comparator 的 Worse/NotComparable 另由既有 comparator tests 验证 |
| T6 | `_testSuccessfulIncrementalRepair`、`_testHoleAndRouteAwareOrdering`：N 明显大于 T，实际 repair 全环属于 T，真实 Transit 可位于 N−C，未沿 N perimeter 生成 Coverage |
| T7 | `_testHoleAndRouteAwareOrdering`：hole support 实际应用并严格改善，全部新旧路径段 hard safe，不穿 No-Go |
| T8 | 同上：两个等价 improvement；穿越障碍的近点直线距离更短但真实 route 更长；引擎选择实际 route cost 更低的另一分量 |
| T9 | `_testStableTrialKeys`：分别证明 component ID、entry index、direction 的稳定平局优先级；AssessmentError 不获得质量键 |
| T10 | `_testAllCandidatesAndHardPassPriority`：4 个初始候选均尝试；Preferred repaired FAIL、Hard repaired PASS；最终全路径 D1 且 PASS |
| T11 / M06 | `_testPlannerInitialPassAndStandardStrict`：同一初始几何 Standard Acceptable/PASS 不修补；Strict Insufficient 可修补至 Complete；初始 U 和 numerical tolerance 一致 |
| T12 | `_testAutoNoEscalationAndBcdIntegration`：Simple repair 后仍 Insufficient，Auto source/strategy 保持 Simple、escalated=false；内部最佳候选保留，外部 path 为空 |
| T13 | `_testTargetRelativeSupport`、`_testRepairDeterminism`：重复、cyclic shift、winding、hole/component order permutations 的 ID/entry/direction、path/roles、metrics、quality 一致 |
| T14 | `PlanningInputIdentityTest::_testSupportedSemanticMatching`、`CoverageComplexItemTest::_testAutoResolvedStrategyIdentityAndBcdArtifacts`：pre-repair Simple/BCD identity unsupported；旧 Artifact stale/Unplanned/no Mission items；旧 Simple JSON 原样重存 |

既有 `CoverageQualityEvaluatorTest::_testComparatorOrderingAndEquivalence` 与
`_testComparatorErrorAndTransitivity` 仍通过，未引入新的覆盖 truth 或数值门槛。
原 Simple/Auto “不足后不发布路径/不升级”案例改用仍存在 Strict corner deficit 的 H=P=2 fixture；
不冻结已能被本包合法修补的历史 Standard deficit。

## Semantic identity 审计与修改

实际 fingerprint 输入包括 planning/policy、resolved strategy/version、canonical C/N/O、swath、H/P/E、
requirement、sweep、requested planner ID。`matchesSupported()` 检查版本及当前支持的策略 semantic。
Auto 返回最终委托策略，因此其 artifacts 同样绑定 Simple/BCD resolved version；Auto 自身 version 不单独进入 identity。

| 身份 | 基线 | 本包 |
| --- | --- | --- |
| Simple | `simple-monotone.v1` | `simple-monotone.v2` |
| BCD | `bcd.v0.5.v1` | `bcd.v0.5.v2` |
| Auto | `auto.v1` | 不变，resolve/delegate 行为不变 |
| Planning | `p2.v0.5.planning.1` | 不变，resolved version 已覆盖新行为 |
| Coverage policy | `coverage-quality.v1` | 不变 |
| Sweep | `global-sweep.v1` | 不变 |

既有固定输入 fixture 的新 Simple SHA-256：
`5a95cc6ccab5dbf53a2663e7324e54f7368bdba19b3b6ac63ace9a15bd8ca7b6`。
新 BCD SHA-256：`b1910e0795b3ad2f1da551c25a79df5316025f6b30adf3d062b45e2121b2e434`。
由编码规则独立计算并与实际 C++ 测试一致；旧 BCD 固定 hash 亦可独立复现。
未改 fingerprint 编码、坐标尺度、CAL-01=0.99、CAL-02=0.50 m、数值容差或 Artifact schema。

## 修改文件

新增：

- `custom/src/Marine/Planning/CoverageRepairSupport.h/.cc`
- `custom/src/Marine/Planning/CoverageRepairData.h`
- `custom/src/Marine/Planning/CoverageRepair.h/.cc`
- `custom/test/Marine/CoverageRepairTest.h/.cc`
- `docs/marine/V05_07_IMPLEMENTATION_REPORT.md`

修改：

- `custom/CMakeLists.txt`：注册生产文件、focused suite。
- `custom/src/Marine/Planning/CoveragePlanningProblem.h`：纯 runtime repair facts。
- `custom/src/Marine/Planning/SimpleMonotoneCoveragePlanner.cc`：全集合 PASS gate、每候选 repair。
- `custom/src/Marine/Planning/BoustrophedonCoveragePlanner.cc`：同上，原 decomposition/angle 不变。
- `custom/src/Marine/Planning/CoverageStrategySemantics.h`：两项必要版本升级。
- `custom/test/Marine/CoverageComplexItemTest.cc`：旧 repair semantics Artifact stale 测试。
- `custom/test/Marine/PlanningInputIdentityTest.cc`：新固定 hash、旧 semantic 不受支持。
- `custom/test/Marine/CustomPluginIntegrationTest.cc`：Simple 版本预期。
- `custom/test/Marine/SimpleMonotoneCoveragePlannerTest.cc`：post-repair 不足 fixture。
- `custom/test/Marine/AutoCoveragePlannerTest.cc`：同上，不升级断言保留。

未修改 safety/geometry/routing/quality backend、Auto 源码、CellCoverage、decomposition、排序器、
身份匹配实现、Task JSON、Artifact loader、QGC production、翻译、QML、AGENTS.md 或冻结规范。

## 验证命令、证据与已知阻塞

Windows MSVC environment：

```text
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
"C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build/P0-01-marine-debug --target QGroundControl --parallel 8
```

增量 build PASS；日志 `build/v05-07-final-build.log`，exit 及 binary SHA-256 单独落盘。
本机 Ninja/MSVC header dependency 记录存在 `#deps 0`，首次 C build 混用旧布局对象导致 ABI 崩溃、旧 semantic
判断错误。已明确刷新现有 build 中 69 个 custom `.obj` 后重编译，恢复一致 binary；未改环境配置或源码来掩盖问题。
记录为构建环境 debt；后续 header 变化必须继续核验实际重编译，不能只相信 no-work 增量输出。

```text
ctest.exe --test-dir build/P0-01-marine-debug --output-on-failure --verbose --output-junit F:/Projects/qgroundcontrol/build/v05-07-focused.xml -R "^(<20 focused names>)$"
ctest.exe --test-dir build/P0-01-marine-debug --output-on-failure --verbose --output-junit F:/Projects/qgroundcontrol/build/v05-07-regression.xml -R "^(<57 established names>)$"
QGroundControl.exe --unittest:<suite> --allow-multiple --unittest-output:F:/Projects/qgroundcontrol/build/v05-07-contract.xml
```

完整名称/命令在机器 evidence 和 `build/v05-07-focused-selection.txt`、
`build/v05-07-regression-selection.txt`，未只跑 repair suite 代替 regressions。
测试使用既有 Windows offscreen/software/字体环境和正常临时缓存权限。
focused 20/20 PASS；established 56/57 PASS；逐 case Qt XML 另验证 repair、identity、Artifact stale 和唯一失败原因。

唯一 regression failure：`MissionManagerTest::_testErrorAckFailureStrings`，strict log 捕获
`QString::arg: Argument missing: "框架1", 3`。
HEAD 的 `translations/qgc_source_zh_CN.ts` / `PlanManager` context 中，`Frame: %1` 已翻译成缺少 `%1` 的 `框架1`。
HEAD blob 与 working-tree diff 证据证明该资源、`src/MissionManager/PlanManager.cc`、对应历史测试均未修改。
因此分类为 pre-existing/unrelated；证据 `build/v05-07-mission-baseline-proof.json` 及
`build/v05-07-contract-MissionManagerTest.xml`。未修改 production messages 或 translations 以取得全绿。

```text
clang-tidy.exe -p build/P0-01-marine-debug --extra-arg=/Y- --extra-arg=-Wno-unused-command-line-argument <changed production .cc>
clang-format.exe --dry-run --Werror [--lines=<modified ranges>] <changed C++ file>
.venv/Scripts/python.exe tools/analyzers/vehicle_null_check.py <4 production .cc>
.venv/Scripts/python.exe tools/analyzers/qt_translate_noop_check.py <4 production .cc>
git -c core.autocrlf=false diff --check
.venv/Scripts/pre-commit.exe run --files <4 production .cc>
```

- clang-tidy 4/4 exit 0，非阻断 warning 原样保留在日志，未修改规则或禁用核心检查。
- clang-format 16/16 exit 0：新增文件完整检查，既有文件只检查本次修改区段，不混入历史格式债务。
- vehicle-null、qt-translate、diff-check PASS。
- 正常 pre-commit exit 1：`OperationalError: attempt to write a readonly database`，随后缓存日志权限错误；
  hooks 尚未执行。无 waiver、无缓存权限变更、无 bypass commit。
- Clazy 不可用：supplemental static-analysis SKIP，遵循 Marine Windows 规则。

最终 machine evidence 关联 HEAD、工作树文件 hashes、binary hash、测试 XML/log hashes、命令、
逐测试状态及 baseline proof。早期失败诊断保留，最终结论仅使用最新 binary 的最终验证。

## 边界、遗留与停止

架构偏离：无。完整 rings 是本合同允许的最小支持算法，不承诺内部缺口都能修复或全局最优。
无法严格改善时保留最佳可靠候选，不搜索角度或降低安全参数。

V05-08 范围仍未实现：MissionReadiness 最终映射、issues/suggestions、完整 repair/artifact provenance、
诊断候选、最终 MissionAdapter upload gate。QML/结果可视化属 V05-09，未实施。
当前 InfrastructureOnly 产物恢复后继续不可执行。

V05-07 硬行为合同与可用核心检查已满足；仓库整体 gate 仍有上述既有回归失败和 pre-commit 环境阻塞，
需要独立审核明确处理，不能称为无例外 DoD 全绿。
本轮到此 STOP。下一候选工作包为 V05-08，须独立审核及新的所有者授权后才可实施。
