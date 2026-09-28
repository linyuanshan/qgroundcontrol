# V05-03 — H/P/E 区域、安全层级评估及优选/硬安全回退

状态：实现、Windows 构建、focused tests 和相关回归完成；未提交，等待独立审核。
V05-04 及以后未授权。

起始 HEAD：`68f5278f25f2df98b3affdf40861dde996a3c611`。
分支：`feature/marine-p2-complex-coverage`。开始时工作区干净。

## 实现和架构边界

治理仅更新 lifecycle metadata：V05-02 Complete、V05-03 Current / Authorized、V05-04 Not authorized。
v0.5 规范 §§1–37 设计正文保持不变。

`CoveragePlanningProblem.safetyMarginM` 已移除。规划输入使用 `SafetyConfig safety` 显式传递 H/P，
并保留 `ExecutionSafetyProfile executionSafety` 传递 E。`CoverageTaskAdapter` 原样复制任务的 H/P/E，
共享校验拒绝非有限值、H<0、P<H、E<0；无自动钳制、缺省推断或 P 重写。

新增纯 Marine 接口 `CoverageSafety.h/.cc`：

| 接口 | 行为 |
| --- | --- |
| `validateSafetyMargins` | 统一校验 H/P/E；InvalidPreferredSafetyMargin 独立于 H/E 错误。 |
| `buildSafetyTrackRegions` | 在 V05-02 C/N/O 拓扑校验后，从 N/O 构建三个区域集并验证包含关系。 |
| `evaluateSafetyCandidate` | 验证整条候选路径，返回最小 D0/D1 和逐段 PreferredSafe / HardSafeWarning 分类。 |
| `selectPreferredOrHardCandidate` | 在已提供的 preferred/hard 候选之间进行安全层级回退，只输出经过硬安全验证的一条路径。 |

三层区域均为 `PolygonRegionSet2D`：

```text
NominalHardTrackRegion = Inset(N,H) - RoundInflate(O,H)
HardExecutionTrackRegion = Inset(N,H+E) - ConservativeMiterInflate(O,H+E)
PreferredExecutionTrackRegion = Inset(N,P+E) - ConservativeMiterInflate(O,P+E)
```

复用现有 Marine 几何后端的 round/miter 偏移、闭区域整段谓词和区域集包含关系运算。
保留现有毫米格网、圆弧容差和偏移数值策略，没有修改几何后端、降低精度或添加拓扑修补。
区域可以为空、多分量或含孔洞，C 外的 O 仍参与 N 的安全偏移。这里不施加旧 CoverageFreeSpace 的
单分量或全目标幅宽可达性门槛。

必须证明 Preferred ⊆ Hard ⊆ Nominal。无法证明包含关系返回 ExecutionRegionNotConservative；
运算失败或 H+E/P+E 溢出返回 GeometryFailure，不发布部分构造的区域。
成功构造的空 Preferred 可以回退；成功构造的空 Hard 返回区域构建成功，但路径评估为 NoNavigableArea。
Preferred 运算失败不能冒充空 Preferred 以触发回退。

候选至少有两个有限点，每段必须有有限正长度；整条路径的每一段都验证 Hard 区域，任何失败都清空层级和航段评估。
每段都满足 Preferred 才是 D0；硬安全但至少一段不满足 Preferred 才是 D1。该评估与 Coverage/Transit 用途无关，
不会漏过连接段。路径可能沿经过验证的闭区域边界行驶，但不能进入孔洞或跨越不连通分量之间的空隙。

选择接口接收已生成的候选，不生成覆盖路线、不引入 Auto 或策略比较器。有效 D0 preferred 候选优先；
preferred 不可用时验证 hard 候选，也可以保留 preferred 尝试中已经完整验证为硬安全的路径。
`usedHardFallback` 表示选择过程，D0/D1 由实际几何决定：hard 尝试若满足 Preferred，仍归类 D0。
接口不接收可修改的 Task，不改变 H/P/E、C/N/O。回退结果包含明确的 D1/HardSafeWarning 安全分类，
尚未接入 V05-08 的就绪状态、完整诊断、产物和上传门禁。

历史 Lawnmower/BCD/Mock 保留 V05-02 的 C=N 能力限制；旧 H/E 消费点改为显式 H 字段，历史算法和
CoverageFreeSpace 的历史可达性逻辑未重构。它们没有自动获得本接口的完整 v0.5 候选生成能力，
也不声称支持新的完整 C/N 规划或优选安全流程。SITL 夹具仅机械迁移输入字段，未运行 SITL。

无架构偏离；没有实现 V05-04 覆盖质量/策略/残余、V05-05 Auto、V05-06 完整 C/N BCD、V05-07 repair、
V05-08 readiness/D2/D3/artifact/upload 或 V05-09 QML。没有修改 CAL-01/CAL-02 或冻结设计正文。

## 验证证据

Windows：MSVC 2022 x64、Qt 6.11.1、现有 `build/M00-debug` Debug Ninja 目录。

```text
vcvars64.bat
cmake --build F:/Projects/qgroundcontrol/build/M00-debug --parallel 6
QGroundControl.exe --unittest:<Suite> --allow-multiple --unittest-output <JUnit.xml>
```

首轮发现并修复新测试 CMake 注册参数错误；修正后增量构建 PASS，见 `build/v05-03-build-retry.log`。
最终 LF 格式的当前源码再次增量构建 PASS，证据见 `build/v05-03-final-build.log`。

7 个 focused suites PASS：CoverageSafetyTest、CoverageTaskAdapterTest、CoverageProblemValidatorTest、
CoverageGeometryTest、CoverageFreeSpaceTest、LawnmowerCoveragePlannerTest、BoustrophedonCoveragePlannerTest。
其中新 CoverageSafetyTest 35 个 JUnit 条目（含初始化/清理）全部 PASS。

focused tests 覆盖 H=0、P=H、P>H、E=0/非零、非法 H/P/E、空 Preferred、空 Hard、全部为空、
区域分裂且保留孔洞、两级包含关系、包含关系被破坏时拒绝、C 外 No-Go 仍约束导航、round/miter 角部差异、
端点安全但中间穿孔洞、后续航段不安全、跨分量连接、闭区域边界、E 约束、非法路径、D0/D1 回退、
不修改 P、几何失败不可回退，以及 Adapter 独立 P 传递和 P<H 拒绝。

最终相关回归：50/50 PASS（28 个 Marine、22 个 QGC Mission），共 640 个 JUnit 条目（含初始化/清理），0 跳过。
汇总日志：`build/v05-03-regressions.log`；套件清单为 `build/v05-03-regression-selection.json`。
逐套件证据：`build/v05-03-<Suite>.log`、`.json` 和 `build/v05-03-results-<Suite>.xml`。
测试使用进程级 offscreen/software Qt 环境和 en_US locale，没有修改系统语言或上游断言。

静态检查：

- 5 个修改/新增生产 `.cc` 通过仓库 clang-tidy 错误门槛（exit 0）；见 `build/v05-03-static-results.json`。
  这不表示零警告。使用现有 compile database，并通过 `/Y-` 禁用分析时的 MSVC PCH。
- 7 个修改/新增生产文件通过 `vehicle_null_check.py` 和 `qt_translate_noop_check.py`；同名前缀日志保存于 build。
- `cmake-format --check` 和 `cmake-lint custom/CMakeLists.txt` PASS。
- clang-format 22.1.8 对修改范围及新增文件检查通过；不声称通过 hook 固定的 23.1.0 版本检查。
- Clazy 不可用：supplemental static-analysis SKIP。
- pre-commit：ENVIRONMENT BLOCKED。正常用户缓存中的
  `repo6pr8_rwh/.pre-commit-hooks.yaml is not a file` 导致 InvalidManifestError，见 `build/v05-03-precommit.log`。
  没有跳过钩子、修改缓存权限或创建提交。

## 范围、DoD 和剩余事项

`git diff --check` PASS。共 24 个修改/新增文件，暂存区为空，无冲突，HEAD 和分支未变。
规范仍为 §§1–37，设计正文与 HEAD 一致；没有修改 QML、上游核心或 V05-04 实现。
最终源文件/二进制及日志哈希、逐套件结果、范围核对见 `build/v05-03-evidence.json`。
完整 DoD 仍待独立审核与 pre-commit 环境问题闭环，不声明提交门槛 PASS。
下一工作包是 V05-04，但仍未授权，且其 Standard 数值门槛受 CAL-01/CAL-02 批准门槛约束。
本包完成后 STOP，暂不提交。

## 修改文件

- `AGENTS.md`
- `custom/CMakeLists.txt`
- `custom/src/Marine/Planning/CoverageFreeSpace.cc`
- `custom/src/Marine/Planning/CoveragePlanningProblem.h`
- `custom/src/Marine/Planning/CoverageProblemValidator.cc`
- `custom/src/Marine/Planning/CoverageTaskAdapter.cc`
- `custom/src/Marine/Planning/LawnmowerCoveragePlanner.cc`
- `custom/test/Marine/BoustrophedonCoveragePlannerTest.cc`
- `custom/test/Marine/BoustrophedonDecompositionTest.cc`
- `custom/test/Marine/CellCoverageTest.cc`
- `custom/test/Marine/CoverageFreeSpaceTest.cc`
- `custom/test/Marine/CoverageGeometryTest.cc`
- `custom/test/Marine/CoveragePlannerTest.cc`
- `custom/test/Marine/CoverageProblemValidatorTest.cc`
- `custom/test/Marine/CoverageTaskAdapterTest.cc`
- `custom/test/Marine/LawnmowerCoveragePlannerTest.cc`
- `custom/test/Marine/MarineSITLValidationTest.cc`
- `custom/test/Marine/NominalCoverageValidatorTest.cc`
- `docs/marine/P2_COMPLEX_COVERAGE_SPEC_v0.5_ADAPTIVE_PLANNING_AMENDMENT.md`
- `custom/src/Marine/Planning/CoverageSafety.cc`
- `custom/src/Marine/Planning/CoverageSafety.h`
- `custom/test/Marine/CoverageSafetyTest.cc`
- `custom/test/Marine/CoverageSafetyTest.h`
- `docs/marine/V05_03_IMPLEMENTATION_REPORT.md`
