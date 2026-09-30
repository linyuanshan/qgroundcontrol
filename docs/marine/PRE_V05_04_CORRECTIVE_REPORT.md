# Pre-V05-04 Corrective Gate 实现报告（最终纠偏）

日期：2026-09-30  
分支：`feature/marine-p2-complex-coverage`  
起始及当前 HEAD：`7d5f6d82ee0d1d3f1795f40d838446669081de77`  
提交状态：未提交  
后续阶段：V05-04 未授权、未实现

## 1. 基线与范围

工作开始前已确认工作区 clean，分支和起始 HEAD 与任务给出的基线一致。预检读取了
`AGENTS.md`、`CODING_STYLE.md`、`.github/CONTRIBUTING.md`、`test/README.md`、
`.github/ci-overview.md`、`tools/README.md` 和
`P2_COMPLEX_COVERAGE_SPEC_v0.5_ADAPTIVE_PLANNING_AMENDMENT.md`，并在编辑前检查了当前
Task、身份指纹、地理坐标转换、安全区域、规划结果、Artifact 保存/加载和新建任务 API。

第一轮处理五项纠偏；第二轮关闭 stale Artifact 校验与默认 E 两项 blocker，并分离路径指标一致性容差；
最后一轮将 H/P 与 E 统一为显式配置。
第二轮开始时保留全部第一轮未提交变更。没有实现 CoverageQuality、Standard/Strict 数值门槛、
CAL-01/CAL-02、Auto planner、v0.5 BCD 改造、repair、MissionReadiness、完整 Artifact/upload
gate 或 QML 新功能；没有修改规范正文、`AGENTS.md`、QML 或 MissionAdapter。

## 2. 实现结果

### 2.1 polygon closing duplicate

- 新增统一的 `openRingVertexCount()`：只把一个与首点二维坐标相同的末点视为可选闭环标记。
- `A,B,C,D` 与 `A,B,C,D,A` 在 Task schema、PlanningInputIdentity、GeoReference 原点和
  CoverageTaskAdapter 本地几何转换中采用相同的开环顶点集合。
- 两个连续 closing duplicates 不属于规范化范围，作为无效输入拒绝；没有增加一般性几何修复、
  点移动、简化或拓扑修补。
- 回归覆盖相同 fingerprint、相同本地几何、可重新规划及 Artifact 加载，不再出现指纹相同但
  重新规划为 InvalidInput 的分歧。

### 2.2 Success path / Artifact 不变量

- 新增共享 `PlanningPathMetrics` 校验：Success path 至少两个有限点；每条 leg 长度严格大于零；
  `legRoles.size() == path.size() - 1`；角色必须是 Coverage 或 Transit。
- coverage、transit 和总路径长度从实际路径几何重新计算。第二轮使用独立常量
  `PathMetricsConsistencyToleranceM = 0.01 m`，用于指标一致性，不使用几何安全判定的 1 mm
  容差，也不把该常量当成真实路径统计精度或安全 clearance 要求。
- planner solution 在本地坐标映射前校验；ComplexItem 生成及 identity 匹配的保存/加载再次
  按当前 Task 地理参考转换实际路径并验证指标。
- 无效 Success result 会降级为 Failed 并清空可执行路径；无效 Artifact 拒绝加载或保存为合法
  Success。
- 第二轮 load 先解析 `inputIdentity` 并与当前 Task 比较，再选择校验范围。identity 不匹配时
  只校验有限坐标、非零航段、角色数量/枚举、有限非负指标及指标合计等结构不变量，不使用
  当前 Task 的 GeoReference 重新认证旧路径。
- 有效但不匹配的产物成功恢复为 `stale=true / Unplanned`；当前 planning result/path 为空，
  不重规划，不生成 mission items。已加载原始 JSON 由 adapter 保留，过期时按原 artifact/
  identity 重新保存；当前 Task 的 E 未配置也不妨碍保留过期产物。
- 新增跨洲移动/改变 reference 的回归：将 Task 从欧洲移到澳大利亚并改变区域尺寸，明确验证
  新坐标参考计算出的旧路径指标不匹配，但旧产物仍成功加载、原样重新保存并保持不可执行。

### 2.3 1 mm 格网 hard-safety 量化

- `CoordinateScalePerM` 保持 `1000.0`，未修改格网尺度。
- V05-03 安全认证专用 offset 使用
  `ceil(marginM * CoordinateScalePerM) + 3` 个格网单位，始终向安全侧取整。
- 3 mm guard 覆盖输入和输出坐标各自最多 `sqrt(2)/2 mm` 的二维格网位移，以及既有闭区域
  predicate 的 1 mm 容差；总上界小于 3 mm。H=0、E=0 时也保持导航边界的安全侧表示，避免
  容差接受边界外点。
- 保守取整只用于 V05-03 的 NominalHard/HardExecution/PreferredExecution 安全区域入口。
  历史 `insetPolygon()`、`buildTrackFeasibleRegion()` 和旧 planner 的几何结果保持原行为，避免
  把本纠偏扩大为历史 planner 重构。
- focused tests 覆盖 H 为 0、0.1、0.49、0.5、0.51、0.9 mm，E 为 0、0.1、0.5 mm，以及输入
  坐标位于 -0.5 mm 和 +0.5 mm 量化边界的组合；验证 N 边界和 No-Go 边界的 under-clearance
  点及完整路径段均被拒绝。

### 2.4 safety fallback helper 边界

- `selectPreferredOrHardCandidate()` 的接口说明明确限定为 V05-03 safety-only fallback。
- 该 helper 只在调用方提供的 preferred/hard path 之间做 D0/D1 安全回退，不是规范 §24 的最终
  candidate comparator；Coverage certification 和 CoverageQuality 排序不在本包实现。

### 2.5 新 Task 的 E 配置

- 删除 P2-13E 的 `0.25 m` 隐式通用默认值。
- 第二轮将共享 `ExecutionSafetyProfile` 的默认 E 改为 NaN，所有新构造 MarineTask 均保持
  明确的未配置状态；仅在 plan creator 中设置 NaN 的第一轮局部修复已替换为域层默认契约。
- 未配置 E 的 Task 不通过 validation、JSON save 或规划；v3 JSON 显式加载合法 E 的行为不变，
  测试覆盖显式 E=0 和 E=0.25 的保存/加载。
- 历史 planner/problem 测试 fixture 所需 E=0 均由测试显式赋值。SITL 测试的 Task 从其
  显式配置的 problem 复制 E；没有执行新的 SITL 或 field 验收。
- 本包没有引入车辆配置系统或 QML 编辑功能；现有测试通过显式设置 E 后继续规划。后续显式
  配置 UI/来源仍属于已规划的后续工作包。

### 2.6 新 Task 的 H/P 显式配置闭环

- `SafetyConfig.hardSafetyMarginM` 与 `preferredSafetyMarginM` 的默认值改为 NaN；新构造
  `MarineTask` 时 H/P/E 均明确处于未配置状态，`isValid()`、`schemaValid()`、规划适配和
  Task v3 保存均 fail-closed。
- PlanCreator 不再以 `P = H` 暗中完成配置。历史 planner/problem fixture 所需 `H=0/P=0`
  均显式赋值；新建任务只有显式设置 H、P、E 后才可通过 schema validation。
- 显式 `H=0,P=0,E=0` 和任意有限 `P >= H >= 0` 合法；`P < H`、NaN、Inf、负值继续拒绝。
  v3 JSON 显式加载 H/P/E 的序列化与往返行为保持不变。
- focused tests 覆盖默认三者未配置、仅配置 Swath/E、显式零值、非法边界、PlanCreator 新任务
  保持 invalid 及 JSON round-trip。未修改安全几何算法、CoordinateScalePerM、offset 规则、
  CAL、CoverageQuality、Auto、repair、readiness 或 QML。

### 2.7 数值策略的适用边界

当前 1 mm lattice 是 Clipper 几何后端的内部数值实现细节，不代表真实 USV 具有毫米级定位、
控制或安全保证。第一轮的保守取整只针对当前数值表示中的 under-clearance 风险；第二轮未修改
格网尺度，也未重构或改变 H/P/E 安全区域算法。

Geometry Numeric Policy 后续单独冻结：厘米/分米级几何容差、路径指标容差与 H/P/E 产品粒度仍需
作为统一产品数值策略审议。此次采用的 1 cm consistency tolerance 是指标一致性校验的工程值，
不构成该后续产品策略的冻结，也不改变安全 clearance。

容差测试分别验证 coverage/transit/path 三个指标在阈值以内、恰好位于阈值和阈值之外的结果，
并验证 planner result 在一致性容差内接受、超出容差拒绝。

## 3. 修改文件

生产与构建：

- `custom/CMakeLists.txt`
- `custom/src/Marine/MarineTypes.h`
- `custom/src/Marine/MarineTask.cc`
- `custom/src/Marine/MarineTask.h`
- `custom/src/Marine/PlanningInputIdentity.cc`
- `custom/src/Marine/Geometry/GeoReference.cc`
- `custom/src/Marine/Geometry/MarineGeometry.h`
- `custom/src/Marine/Geometry/MarineGeometry.cc`
- `custom/src/Marine/Geometry/PolygonRegion.h`
- `custom/src/Marine/Geometry/PolygonRegion.cc`
- `custom/src/Marine/Planning/CoverageSafety.h`
- `custom/src/Marine/Planning/CoverageSafety.cc`
- `custom/src/Marine/Planning/CoverageTaskAdapter.cc`
- `custom/src/Marine/Planning/PlanningPathMetrics.h`（新增）
- `custom/src/Marine/Planning/PlanningPathMetrics.cc`（新增）
- `custom/src/MissionManager/CoverageInspectionComplexItem.cc`
- `custom/src/MissionManager/CoverageInspectionComplexItem.h`
- `custom/src/MissionManager/CoverageInspectionPlanCreator.cc`

测试：

- `custom/test/Marine/CoverageTaskAdapterTest.h`
- `custom/test/Marine/CoverageTaskAdapterTest.cc`
- `custom/test/Marine/CoverageComplexItemTest.h`
- `custom/test/Marine/CoverageComplexItemTest.cc`
- `custom/test/Marine/CoverageSafetyTest.h`
- `custom/test/Marine/CoverageSafetyTest.cc`
- `custom/test/Marine/CoverageInspectionPlanCreatorTest.cc`
- `custom/test/Marine/MarineTaskModelTest.h`
- `custom/test/Marine/MarineTaskModelTest.cc`
- `custom/test/Marine/PlanningInputIdentityTest.cc`
- `custom/test/Marine/CustomPluginIntegrationTest.cc`
- `custom/test/Marine/BoustrophedonCoveragePlannerTest.cc`
- `custom/test/Marine/BoustrophedonDecompositionTest.cc`
- `custom/test/Marine/CellCoverageTest.cc`
- `custom/test/Marine/CoverageFreeSpaceTest.cc`
- `custom/test/Marine/CoverageGeometryTest.cc`
- `custom/test/Marine/CoveragePlannerTest.cc`
- `custom/test/Marine/CoverageProblemValidatorTest.cc`
- `custom/test/Marine/LawnmowerCoveragePlannerTest.cc`
- `custom/test/Marine/NominalCoverageValidatorTest.cc`
- `custom/test/Marine/MarineSITLValidationTest.cc`

第二轮的额外历史测试文件只调整显式 E fixture；原安全几何算法及格网尺度保持第一轮状态。

证据与报告：

- `docs/marine/PRE_V05_04_CORRECTIVE_REPORT.md`（本文件）
- `build/pre-v05-04-evidence.json`（机器可读证据，build 目录被 Git 忽略）
- `build/pre-v05-04-r3-evidence.json`（最终 H/P 显式配置闭环的机器证据）
- `build/pre-v05-04-r2-evidence.json`（第二轮独立机器证据，包含二进制、源码、日志、JUnit 哈希）
- `build/pre-v05-04-r1-evidence.json`（第一轮机器证据归档）

## 4. Windows 构建与测试证据

Windows 增量构建：

```powershell
$env:MOCCACHE_DISABLE='1'
build\v05-01-build.cmd
```

结果：PASS。关闭 moccache 是因为默认 moc 缓存在两个不同 target 上出现无诊断 AutoMoc 环境失败；
禁用缓存后最终源码完整编译并链接 `Debug/QGroundControl.exe`。最终日志：
`build/pre-v05-04-r3-build.log`。第一轮证据保留，第二轮证据使用独立 `r2` 文件前缀。

最终 H/P 修正后的 Focused tests：9 suites、173 tests，全部 PASS：

- CoverageTaskAdapterTest — 12
- PlanningInputIdentityTest — 7
- CoverageComplexItemTest — 25
- CoverageInspectionPlanCreatorTest — 6
- CoverageFreeSpaceTest — 16
- CoverageSafetyTest — 36
- MarineTaskModelTest — 8
- MarineTaskJsonTest — 60
- MarinePlanIntegrationTest — 3

最终 H/P 修正后的相关 Marine + QGC Mission regressions：50 suites、648 tests，全部 PASS。QGC Mission 测试第一轮在
sandbox 内因用户 cache/ParamCache 无写权限触发 strict-log 失败；在允许正常测试缓存写入后，
MissionManagerTest、MissionControllerTest、MissionControllerTreeTest、PlanMasterControllerTest 及其余
回归均通过。最终轮使用正常可写缓存环境，日志：`build/pre-v05-04-r3-regressions.log`。
每组 JSON 记录相同的最终二进制 SHA-256，独立 JUnit/XML 的路径与哈希写入机器证据。

## 5. 静态与仓库检查

- clang-tidy：10 个修改/新增的生产 `.cc` 文件，全部 PASS。
- clang-format：修改行和新增文件 PASS。
- `vehicle_null_check.py`：全部修改/新增生产文件 PASS。扩展扫描测试文件时有两处既有 SITL
  false positive，均已与 HEAD 核对：`vehicle` 使用前存在 `QVERIFY(vehicle != nullptr)`，
  本轮没有修改这些语句；保留原扫描日志，不将其隐瞒为测试文件扫描全 PASS。
- `qt_translate_noop_check.py`：PASS。
- `git diff --check`：PASS。
- Clazy：补充静态分析 SKIP，本机未安装。
- pre-commit：环境阻塞。sandbox 内默认缓存数据库只读；允许正常缓存访问后，既有 hook cache
  的 `.pre-commit-hooks.yaml` 仍被系统拒绝读取；改用工作区独立缓存后，环境无法连接
  `github.com:443` 获取 `pre-commit-hooks`。这是钩子基础设施失败，不是代码检查失败；未使用
  `--no-verify`，也没有提交。第二轮正常权限复测仍被既有 hook manifest 的系统权限拒绝，日志为
  `build/pre-v05-04-r2-precommit.log`。可用的本地核心检查已逐项执行并通过。

## 6. 架构偏离、遗留项与结论

没有架构偏离。新增的安全量化入口刻意与历史 planner 几何入口分离；Task != Plan != Mission、
planner 纯域边界、ComplexItem adapter 边界和现有 MissionAdapter 均保持不变。

显式 E 的最终产品配置来源/UI 及统一 Geometry Numeric Policy 留待各自后续审批，不属于本纠偏；
V05-09 尚未授权。新建 Task 在显式配置 E 前保持 schema-invalid，这是本包选择的最小
fail-closed 行为。pre-commit 环境故障尚未解除。

第一轮五项纠偏、第二轮两项 blocker/容差清理及最终 H/P 显式配置闭环的 Windows build、focused tests、相关回归和可用
核心静态检查均满足验收要求。默认 E 未配置、过期产物跨洲 reference 变化与指标容差边界均有
独立回归覆盖。Geometry Numeric Policy 后续单独冻结，未在本轮扩大实施。
pre-commit 环境故障已留证，因此当前状态为：实现可交独立审核，未提交；V05-04 仍未授权。
