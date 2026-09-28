# V05-02 — C/N/O 几何分离及目标/导航区域集契约

状态：实现、Windows 构建、聚焦测试及相关回归完成；未提交，等待独立审核。V05-03 及以后未授权。

起始 HEAD：`aa0de110a0aad979a3b36c1d85bc56ec94fc85fd`。
分支：`feature/marine-p2-complex-coverage`。开始时工作区干净。

## 实现与架构边界

`Region2D` 现在显式拥有 `coverageBoundary`（C）、`navigationBoundary`（N）及 `noGoRegions`（O）。
规划输入不再拥有含糊的 `outerBoundary`。`PolygonRegion2D.outerBoundary` 仍用于表达带孔洞区域的外环。
`CoverageTaskAdapter` 用同一个 `GeoReference` 分别转换 C、N 及全部 O，不从 C 推断 N。

新增 `buildCoverageGeometry(const Region2D&)`，以纯 Marine 数据返回：

- `coverageTarget: PolygonRegionSet2D = C - O`；
- `rawNavigationFreeSpace: PolygonRegionSet2D = N - O`。

入口只接收 C/N/O，不接收 H/P/E、swath 或覆盖策略，也不调用 offset、reachability 或规划器。
它先验证 C/N 简单、有限且非退化，再验证 C 包含于 N（允许边界接触），并相对 N 严格验证 O
及 O 两两不重叠、不接触、不包含。C 与 O 的相交/接触不再按旧 No-Go/C 规则拒绝。
差集通过现有通用区域集 Boolean 后端计算，保留全部分量和孔洞；空目标或无效目标拒绝，
失败结果不暴露部分构造的区域集。C 外的 O 仍保留在导航差集输入中。

输入使用 const 引用，不修改、修复、移动、扩张或简化任务几何。沿用已有后端的
`CoordinateScalePerM = 1000` 和 `LengthEpsilonM = 1e-3` 数值策略；没有新增精度或拓扑修补规则。
派生区域沿用后端的规范顶点顺序和分量排序。

`CoverageProblemValidator` 接入新的纯拓扑/目标校验，原有数字及角度校验保留。
历史 Lawnmower、Mock 和 BCD/`CoverageFreeSpace` 在通用校验后增加明确的 C=N 能力门槛；
有效但 C≠N 的输入返回 `Failed / UnsupportedSeparateBoundaries`，不会忽略 N 或把 N 变成覆盖目标。
该门槛利用已验证 C⊆N 加 N⊆C 的区域包含关系判定，不依赖顶点起点/方向相同。
旧 `CoverageFreeSpace` 的单目标类型、H/E 偏移和可达性实现仍只服务历史 C=N 回归，
不作为 V05-02 的区域集契约，也不声称满足 v0.5 C/N 规划语义。

没有架构偏离。没有实现 H/P/E 新几何、安全降级、CoverageQuality、Auto、完整 C/N BCD、
repair、readiness 或 QML。历史规划器算法没有扩展；相关旧测试显式构造 C=N。
SITL 测试源码仅机械迁移输入字段及同步其历史 C=N 场景，未执行或改变 SITL 场景行为。

## 验证证据

Windows 环境：MSVC 2022 x64、Qt 6.11.1、现有 `build/M00-debug` Debug Ninja 目录。

```text
vcvars64.bat
cmake --build F:/Projects/qgroundcontrol/build/M00-debug --parallel 6
```

首轮及最终增量构建 PASS，见 `build/v05-02-build.log`、`build/v05-02-final-build.log`。

八个聚焦套件首轮 PASS：CoverageGeometryTest、CoverageTaskAdapterTest、
CoverageProblemValidatorTest、GeometryTypesTest、CoverageFreeSpaceTest、CoveragePlannerTest、
LawnmowerCoveragePlannerTest、BoustrophedonCoveragePlannerTest。
最终二进制另运行 49 个相关套件（27 个 Marine、22 个上游 QGC Mission），49/49 PASS。

新 CoverageGeometryTest 的最终 JUnit 包含 34 个 testcase（含初始化/清理），全部 PASS。
矩阵覆盖 C=N、C 严格包含于 N、C/N 边界接触、O 在 C 内/外、O 与 C 跨越/边/点接触、
C 超出 N（含凹 N 的边跨出）、无效 C/N/O、O 接触/越过 N、O 重叠/边接触/点接触/包含、
T 空/零面积、多分量、孔洞及分量内孔洞。面积和点包含断言验证 C 外导航区域保留及 C 外 O 扣除。
Adapter 测试覆盖独立 N 及 C 外 O 的同基准转换、缺失 N 拒绝；历史规划器能力门槛有专门回归。

测试命令及进程环境：

```text
QGroundControl.exe --unittest:<Suite> --allow-multiple --unittest-output <JUnit.xml>
QT_QPA_PLATFORM=offscreen
QT_QUICK_BACKEND=software
QT_QPA_FONTDIR=C:/Windows/Fonts
QT_LOGGING_RULES=*.debug=false;API.QGCApplication.AppMessage.debug=true
LANG=en_US.UTF-8
LC_ALL=en_US.UTF-8
QT_LOCALE=en_US
```

选定套件清单：`build/v05-02-regression-selection.json`。
汇总：`build/v05-02-regressions.log`；逐套件：`build/v05-02-results-<Suite>.xml`、
`build/v05-02-<Suite>.log` 和 `.json`。没有重写上游断言或修改系统语言。

静态检查：

- 7 个修改/新增生产 `.cc` 通过仓库 clang-tidy 错误门槛（exit 0），见
  `build/v05-02-static-results.json`；非致命建议不记为零警告。
- 12 个修改/新增生产文件通过 `vehicle_null_check.py`、`qt_translate_noop_check.py`，
  见 `build/v05-02-vehicle_null_check.py.log`、`build/v05-02-qt_translate_noop_check.py.log`。
- 本地 clang-format 22.1.8 仅处理修改范围及完整新增文件；不替代 hook 固定 23.1.0 的检查结果。
- Clazy 不可用：supplemental static-analysis SKIP。
- pre-commit：ENVIRONMENT BLOCKED。当前失败为 `InvalidManifestError`，缓存
  `repo6pr8_rwh/.pre-commit-hooks.yaml is not a file`，见 `build/v05-02-precommit.log`。
  没有跳过钩子或创建提交。

## 范围与剩余事项

治理只更新 lifecycle metadata：V05-01 Complete、V05-02 Current / Authorized、V05-03 Not authorized。
规范 §1–37 设计正文不变。不存在新增上游核心、QML 或 V05-03 实现文件。
`git diff --check` PASS。共 30 个修改/新增文件；暂存区为空，HEAD 和分支未变。
实现、构建、测试和可用核心静态检查通过；完整 DoD 仍待独立审核和 pre-commit 环境问题处理，
不声明提交门槛 PASS。机器可读证据及最终源文件/二进制哈希见 `build/v05-02-evidence.json`。
下一工作包为 V05-03，仅在项目所有者明确授权后开始。本包 STOP，暂不提交。

## 修改文件

- `AGENTS.md`
- `custom/CMakeLists.txt`
- `custom/src/Marine/Geometry/GeometryTypes.h`
- `custom/src/Marine/Planning/BoustrophedonCoveragePlanner.cc`
- `custom/src/Marine/Planning/CoverageFreeSpace.cc`
- `custom/src/Marine/Planning/CoverageFreeSpace.h`
- `custom/src/Marine/Planning/CoveragePlanningProblem.h`
- `custom/src/Marine/Planning/CoverageProblemValidator.cc`
- `custom/src/Marine/Planning/CoverageProblemValidator.h`
- `custom/src/Marine/Planning/CoverageTaskAdapter.cc`
- `custom/src/Marine/Planning/LawnmowerCoveragePlanner.cc`
- `custom/src/Marine/Planning/MockCoveragePlanner.cc`
- `custom/test/Marine/BoustrophedonCoveragePlannerTest.cc`
- `custom/test/Marine/BoustrophedonDecompositionTest.cc`
- `custom/test/Marine/CellCoverageTest.cc`
- `custom/test/Marine/CoverageFreeSpaceTest.cc`
- `custom/test/Marine/CoveragePlannerTest.cc`
- `custom/test/Marine/CoverageProblemValidatorTest.cc`
- `custom/test/Marine/CoverageTaskAdapterTest.cc`
- `custom/test/Marine/CoverageTaskAdapterTest.h`
- `custom/test/Marine/GeometryTypesTest.cc`
- `custom/test/Marine/LawnmowerCoveragePlannerTest.cc`
- `custom/test/Marine/MarineSITLValidationTest.cc`
- `custom/test/Marine/NominalCoverageValidatorTest.cc`
- `docs/marine/P2_COMPLEX_COVERAGE_SPEC_v0.5_ADAPTIVE_PLANNING_AMENDMENT.md`
- `custom/src/Marine/Planning/CoverageGeometry.cc`
- `custom/src/Marine/Planning/CoverageGeometry.h`
- `custom/test/Marine/CoverageGeometryTest.cc`
- `custom/test/Marine/CoverageGeometryTest.h`
- `docs/marine/V05_02_IMPLEMENTATION_REPORT.md`
