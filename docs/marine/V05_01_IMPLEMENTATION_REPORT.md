# V05-01 — Clean Break 基础设施

状态：实现、Windows 构建与选定测试完成；提交检查受环境阻塞，待独立审查；V05-02 未授权。

起始基线：`23621c71e6531dc1ec40c370c7ef2512ed6483d6`，开始时工作区干净。

## 已获项目所有者批准的过渡契约

授权记录：项目所有者于 2026-09-28 明确回复“明确批准此过渡契约”，批准当前报告所述
Artifact v3 `resultContract=InfrastructureOnly` 边界，即恢复后保持不可执行；该批准不扩展至
V05-08 语义。

Artifact v3 的 `resultContract` 必须显式为 `InfrastructureOnly`。它只表示基础设施产物，
不表示已经通过 v0.5 覆盖评估、安全认证或就绪认证。缺失契约字段不得猜测默认值。

结构和身份匹配时，原样恢复保存结果至独立的 Marine 产物记录。当前可执行
`PlanningResult` 保持空，集成状态保持 `Unplanned`；不调用规划器，也不重新评估。
新生成的部分实现结果同样隔离。ComplexItem 不从基础设施记录生成 Mission 航点。
该隔离不推导 MissionReadiness，也不实现 V05-08 的 Ready/ReviewRequired 上传门禁。

输入或受支持语义身份不匹配时，记录标为 stale，并清除可执行状态。重新保存保留产物
原有身份，不将旧结果重新绑定到当前任务。重命名、传感器开关、再次保存/加载均不会
授予执行资格。未完成的就绪、覆盖认证和诊断数据不补造，`InfrastructureOnly` 明确
表示这些集成数据不可用。

V05-08 在同一 Artifact v3 版本内完成集成结果契约。旧的 InfrastructureOnly 记录
不能自动升级或补齐为认证结果；必须明确重新规划。此过渡契约不声称完整满足规范 §29。

## Task 与规划算法边界

Marine extension 仅接受 v2，Task 仅接受 v3，Artifact 仅接受 v3。普通无 Marine 扩展
的上游 `.plan` 继续允许加载。旧 Marine schema 明确拒绝，不迁移、不补默认值。

Task 显式拥有 C、N、No-Go、覆盖要求、Swath、扫描模式/角度、H/P/E、规划器 ID 及传感器配置。
安全配置独立于 CoverageConfig。codec 校验字段、类型、枚举与有限数值，不验证 C/N
包含关系或其他拓扑。新建任务可以复制 N=C；编辑 C 和加载过程不修改 N。

默认规划器 ID 为 `marine.coverage.auto`，本包不实现其解析。现有历史规划器仍通过
adapter 接收 C 和 H 作为旧问题的 outerBoundary 和 safetyMarginM，仅供算法回归与
非执行基础设施数据生成；这不构成 v0.5 C/N 或 H/P/E 几何实现。

## 指纹编码 v1

散列为 SHA-256，小写十六进制。输入字节顺序冻结如下：

1. 长度前缀字符串 `marine.planning-input`，编码版本 1。
2. planningVersion、policyVersion、resolvedStrategy、strategyVersion。
3. coverageBoundary、navigationBoundary、所有 No-Go。
4. swathWidthM、H、P、E。
5. `Standard` 或 `Strict`，`Manual` 或 `Auto`。
6. 仅 Manual 时编码按 180 度取模的角度。
7. 请求的 plannerId。

整数和长度为无符号 64 位大端序；字符串为 UTF-8，前缀为字节数。
数值使用 IEEE-754 binary64 大端编码，不量化，负零统一为正零。
多边形使用二维经纬度（纬度在前），忽略非规划用的高度；可选闭合重复端点剔除，
所有循环起点和两个方向中选择字节字典序最小的表示。每个环有顶点数量前缀，外层
以字节长度界定。No-Go 环按其规范字节排序，列表有数量前缀。无效输入不生成指纹。

当前 planningVersion 为 `p2.v0.5.infrastructure.1`，policyVersion、resolvedStrategy、
strategyVersion 显式为 `unresolved`。基础设施允许表达未来语义字符串，加载时必须与
当前支持的完整语义身份比较，未知身份只会过期；不能通过读取存储值自证受支持。
名称、纯 UI 状态和非规划用传感器配置不进入指纹。

## 验证记录

### Windows 构建

使用 MSVC 2022 x64、Qt 6.11.1、现有 Debug Ninja 目录 `build/M00-debug`：

```text
vcvars64.bat
cmake --build F:/Projects/qgroundcontrol/build/M00-debug --parallel 6
```

受影响编译单元增量构建、完整链接均 PASS。最终构建日志为
`build/v05-01-final-build.log`。曾遇到 AutoMOC 临时失败，原命令单独重试及后续构建通过。
本地旧构建缓存的 MSVC include 前缀乱码导致 Ninja 未记录头文件依赖；修正生成文件中的
前缀并重建 53 个 Marine/MOC 对象后，确认依赖重新记录。该修复仅涉及忽略的 build 目录，
没有修改构建源码或提交机器配置；修复前的旧 ABI 测试崩溃不计为通过证据。

最终 H/P 修复后，强制重编并链接通过。日志 `build/v05-01-final-fix-build.log` 明确记录
`CoverageInspectionComplexItem.cc.obj`、`CoverageComplexItemTest.cc.obj` 编译及
`QGroundControl.exe` 链接成功（exit 0）。

### 聚焦测试与回归

8 个聚焦套件全部 PASS：MarineTaskModelTest、MarineTaskJsonTest、PlanningInputIdentityTest、
CoverageTaskAdapterTest、CoverageComplexItemTest、CustomPluginIntegrationTest、
MarinePlanIntegrationTest、CoverageInspectionPlanCreatorTest。

验证包括严格版本/字段拒绝、独立 C/N 保存、H/P/E、枚举和有限数值、完整任务往返、
无 Marine 扩展的上游计划、无规划器注册时的产物恢复、原始身份保留、各规划输入的
stale 判定、名称/传感器不引起 stale，以及恢复/新建基础设施产物均不生成 Mission 航点。
指纹使用独立编码计算的固定 SHA-256 向量，并测试环起点/方向、闭合端点、No-Go 顺序、
Manual 角度周期、负零及语义版本。

首轮通过 CTest 运行 48 个套件（26 个 Marine、22 个上游 QGC Mission），33 PASS / 15 FAIL。
26 个 Marine 全部 PASS；失败均出现在上游套件，随后用 JUnit 逐项诊断。
首轮记录保留在 `build/v05-01-regression.log`，选择列表在
`build/v05-01-regression-selection.json`。

复测沿用同一测试二进制与原始断言，允许写入用户测试缓存，指定 Windows 系统字体目录，
并在测试进程设置英文 locale 和 AppMessage 调试日志。没有修改上游源码、测试断言或系统语言。
直接调用方式为：

```text
QGroundControl.exe --unittest:<Suite> --allow-multiple --unittest-output <JUnit.xml>
```

环境：`QT_QPA_PLATFORM=offscreen`、`QT_QUICK_BACKEND=software`、
`QT_QPA_FONTDIR=C:/Windows/Fonts`、
`QT_LOGGING_RULES=*.debug=false;API.QGCApplication.AppMessage.debug=true`、
`LANG=en_US.UTF-8`、`LC_ALL=en_US.UTF-8`、`QT_LOCALE=en_US`。
最终 15 个失败套件全部复测 PASS：11 个见 `build/v05-01-regression-verified.json`，
编辑器见 `build/v05-01-editor-font-MissionCommandTreeEditorTest.xml`，其余 3 个见
`build/v05-01-locale-<Suite>.xml`。合并首轮通过项，48 个选定套件全部获得 PASS 记录；
该结论是首轮加针对性复测，不声称原始 CTest 环境下单轮全通过。

最终 H/P 修复后再次运行三个指定套件并保存独立 JUnit 和标准输出日志：
`CoverageComplexItemTest` 22/22、`MarineTaskModelTest` 7/7、`MarinePlanIntegrationTest` 3/3，
全部通过。证据分别为 `build/v05-01-final-fix-CoverageComplexItemTest.xml`、
`build/v05-01-final-fix-MarineTaskModelTest.xml`、
`build/v05-01-final-fix-MarinePlanIntegrationTest.xml` 及同名前缀 `.log` 文件。

未运行网络/SITL、实艇或 V05-02 及后续集成验收。MarineSITLValidationTest 仅机械替换
Task 字段及显式初始化 N/P，未改变其 SITL 测试行为。

### 静态检查与提交门槛

- 7 个修改/新增的生产 `.cc` 在仓库 clang-tidy 配置下均 exit 0；有非致命建议，
  不宣称零警告。参数：`-p build/M00-debug --extra-arg=/Y-
  --extra-arg=-Wno-unused-command-line-argument`。证据：`build/v05-01-static-results.json`。
- 12 个修改/新增生产头文件与实现文件通过 `vehicle_null_check.py`、
  `qt_translate_noop_check.py`。扫描测试文件时，SITL 中两处已有 `QVERIFY(vehicle != nullptr)`
  的等待宏被空指针检查脚本报为命中；两处均非本次修改，不为静态工具改动 SITL 行为。
- 本地 clang-format 22.1.8 仅格式化既有文件修改范围及完整新增文件；
  hook 固定的 23.1.0 尚未执行，不能视作等价通过。
- `git diff --check`、冲突标记、文件大小、末尾换行、行末空白检查 PASS。
- Clazy 不可用：**supplemental static-analysis SKIP**。
- pre-commit 未通过执行：默认缓存因权限错误不可读；改用本包独立 build 缓存后，
  初始化依赖时连接 `github.com:443` 失败。日志：`build/v05-01-precommit.log`、
  `build/v05-01-precommit-fresh.log`。未更改全局配置、ACL 或钩子配置，未擅自跳过钩子。
  正常 `git commit` 已尝试，仍被同一缓存 PermissionError 阻止；没有生成提交。
  证据：`build/v05-01-commit.log`。

## 变更范围

共 29 个文件。除获批 InfrastructureOnly 过渡契约外，无新增架构偏离。
AGENTS 与规范仅同步生命周期；规范 §1–37 正文与 HEAD 完全一致。
未修改 ArduPilotMissionAdapter、QGC 上游核心、规划算法、QML 或 CAL-01/CAL-02。


- `AGENTS.md`
- `custom/CMakeLists.txt`
- `custom/src/CustomPlugin.cc`
- `custom/src/Marine/MarineTask.cc`
- `custom/src/Marine/MarineTask.h`
- `custom/src/Marine/MarineTaskJsonCodec.cc`
- `custom/src/Marine/MarineTypes.h`
- `custom/src/Marine/Planning/CoverageTaskAdapter.cc`
- `custom/src/MissionManager/CoverageInspectionComplexItem.cc`
- `custom/src/MissionManager/CoverageInspectionComplexItem.h`
- `custom/src/MissionManager/CoverageInspectionPlanCreator.cc`
- `custom/test/Marine/BoustrophedonCoveragePlannerTest.cc`
- `custom/test/Marine/CoverageComplexItemTest.cc`
- `custom/test/Marine/CoverageComplexItemTest.h`
- `custom/test/Marine/CoverageInspectionPlanCreatorTest.cc`
- `custom/test/Marine/CoverageTaskAdapterTest.cc`
- `custom/test/Marine/CustomPluginIntegrationTest.cc`
- `custom/test/Marine/MarinePlanIntegrationTest.cc`
- `custom/test/Marine/MarineSITLValidationTest.cc`
- `custom/test/Marine/MarineTaskJsonTest.cc`
- `custom/test/Marine/MarineTaskJsonTest.h`
- `custom/test/Marine/MarineTaskModelTest.cc`
- `docs/marine/P2_COMPLEX_COVERAGE_SPEC_v0.5_ADAPTIVE_PLANNING_AMENDMENT.md`
- `custom/src/Marine/PlanningArtifact.h`
- `custom/src/Marine/PlanningInputIdentity.cc`
- `custom/src/Marine/PlanningInputIdentity.h`
- `custom/test/Marine/PlanningInputIdentityTest.cc`
- `custom/test/Marine/PlanningInputIdentityTest.h`
- `docs/marine/V05_01_IMPLEMENTATION_REPORT.md`

## 完成状态与后续

实现范围、Windows 构建、8 个聚焦套件和 48 个选定回归套件均完成验证。
pre-commit 仍受环境阻塞，尚未提交；完整 DoD 尚未闭环，不能将钩子未执行记作 PASS。
下一步为独立审查；审查并授权后才进入 V05-02，本包未实现 V05-02。
