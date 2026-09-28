# P2 v0.5 — 自适应规划修订规范

状态：**P2 v0.5 架构设计冻结（Architecture Design Freeze）已完成；V05-00B 规范编写及独立审查已完成；V05-00C AGENTS.md 权威协调已完成；V05-00C-OPT AGENTS.md 上下文优化已完成。**

基线协调：**Pre-V05-01 工作区/基线协调已完成；V05-01 生产实现已获项目所有者明确授权，现为当前工作包（Current / Authorized）；V05-02 仍未获授权（NOT AUTHORIZED）。**

决策依据：经批准的 **V05-00B 规范编写决策记录**及其**权威基线补充说明**，以及 V05-00A 证据审计和
V05-00A-2 架构决策。历史仓库基线：分支 `feature/marine-p2-complex-coverage`，提交
`0c65869fd27330243c3a1c57120db297a479c215`.

本文规定 v0.5 的设计契约。架构设计冻结已完成，但不表示实现已经完成、验证已经通过或已获准开始下一个工作包。
文中的“必须”“不得”“建议”分别表示强制要求、禁止事项和建议事项。实现表示方式必须保留这些语义契约。

## 1. 目的

P2 v0.5 必须采用足以表达覆盖任务的最简单策略，仅在确有需要时才提高算法复杂度。不得以牺牲硬性可执行安全性
换取覆盖率或路径质量。覆盖质量必须与几何数值容差分开评估。

CoverageArea（覆盖区域）规定必须观测的范围；NavigationArea（导航区域）规定车辆允许行驶的范围；No-Go 区域规定
禁止通行区域。优选间距属于质量条件，不得据此削弱硬安全要求。硬安全但覆盖不足的候选结果必须保留并供审查。
用于解释问题的不安全几何绝不能成为 Mission 路径。边界覆盖必须是有条件的修补步骤，而非无条件阶段。

确定性行为以及 Task != Plan != Mission 的分层仍为强制要求。新的 Marine 专属持久化基线明确采用预发布阶段的
不兼容切换（Clean Break）。

## 2. 范围

v0.5 覆盖单个连续 Mission 的静态多边形覆盖规划，包括 Auto 策略解析、分离的覆盖/导航几何、H/P/E 安全模型、统一覆盖质量评估、
条件修补、任务就绪状态、诊断几何、结构化问题与建议、持久化身份以及语义化可视化。

架构链路保持如下：

```text
计划级 MarinePlanContext 中的 MarineTask v3
    -> CoverageTaskAdapter 使用一致的局部坐标基准进行转换
    -> 纯覆盖/导航/安全规划问题
    -> Auto 策略解析、候选生成、评估和条件修补
    -> 具有规范几何、评估、来源信息和独立诊断数据的纯规划结果
    -> CoverageTaskAdapter
    -> PlanningResult
    -> readiness-gated ArduPilotMissionAdapter
    -> MAVLink waypoint mission
```

规划器逻辑不得依赖 QML、QObject、QGeoCoordinate、Vehicle、Fact、MissionItem 或 PlanMasterController。QGC 集成必须留在
适配器和 custom-build 区域。若集成层访问车辆参数，仍须遵守 QGC Fact 系统和固件插件边界。本规范不引入 ROS 运行时依赖。
V05-00B 不授权修改源代码。

## 3. 权威来源与取代关系

### 3.1 来源与当前有效权威

P2 v0.5 是一项经批准的设计修订，其依据包括已提交的 P0/P1/P2 基线、已提交的 v0.3 边界支持修订和 v0.4 执行安全修订、
V05-00A 仓库证据，以及经批准的 v0.5 架构决策记录。权威基线补充说明规定了来源与证据的解释方式。

在 V05 中，文档只有已提交，或已作为当前编写的规范明确获批，并且属于有效权威链时，才具有规范效力。本文仅纳入该决策记录
授权的决策。未受修订影响的历史架构要求继续有效。

未跟踪文件 `P2_COMPLEX_COVERAGE_SPEC_v0.3_Mid-course_Alignment.md` 和
`QGroundControl_海洋机器人智能作业平台总体规划_v1.2_Mid-course_Alignment.md` 仅为非规范性输入。它们自称的权威
对 V05-00B 不产生独立效力，不得据此建立并行的 v0.3 权威，也不得将其描述为被本修订取代的历史规则。

未提交的实现试验（包括 M00 规划器/测试修改）不具有规范效力。取消无条件边界支持的授权来自经批准的 v0.5 决策记录，
而非这些工作区修改。AGENTS.md 和 v0.2 中未提交的工程流程补充不是已提交的架构变更。会话中的执行指令不同于已提交的
Marine 设计权威，也不属于本规范的产品要求。

### 3.2 对历史规范的取代

本表所引历史条款均指上述基线提交中的内容。

| 已提交来源与条款 | 被取代的行为 | v0.5 替代规则 / 保留边界 |
| --- | --- | --- |
| P2 v0.2 §§42–44 与 v0.4 的 Marine 专属持久化版本/所有权条款 | 仅取代其中要求运行时继续接受已废弃 Marine 专属 Task/规划产物序列化版本，或依赖其旧版结构作为可执行输入的部分 | 第 4、28–30 节：Marine extension v2、Task v3、Artifact v3；拒绝旧版且不在运行时迁移。P0/P1 的数据模型、持久化架构及算法语义仍有效；本表不取代 P0/P1 的持久化要求。 |
| P2 v0.2 §§3–11 中 CoverageTarget / TrackFeasibleRegion / 安全可达性规则 | WorkRegion 同时作为覆盖边界和导航边界；要求所有 No-Go 均位于覆盖边界内；把全覆盖目标可达性作为所有规划成功的前置条件 | 第 7–10 节：C 包含于 N；No-Go 相对 N 验证；目标可为区域集；H/P/E 几何；导航安全可行性与覆盖质量分别评估。 |
| P2 v0.2 主流程及 §§19–33、75 | 固定采用 BCD 优先的生产覆盖流程，并覆盖仅从旧导航域生成的单元 | 第 15–17 节：Auto；能力足够时使用 SimpleMonotone；按能力需要使用 BCD；生成与分解以覆盖目标为依据。 |
| P2 v0.2 §§61–70、76、107 | 所有情况均须完整覆盖否则 Failed；不显示覆盖不完整但安全的结果；旧覆盖/优化排序 | 第 11–24 节：统一 Standard/Strict 评估；允许 Success + ReviewRequired；诊断结果分离；硬安全为必需筛选条件。 |
| P2 v0.3 边界支持修订中的冻结覆盖职责和组装顺序 | 无条件边界支持、必须遍历每条边界、必须先走边界再覆盖单元 | 第 22–23 节：仅在策略未通过后，按残余区域逐组件增量修补。确定性几何、Coverage 角色和安全路由仍可复用。 |
| P2 v0.4 冻结架构决策和间距语义 | WorkRegion 同时作为两类几何输入；未定义优选层级、就绪状态和诊断数据；覆盖失败必然使安全路径失效 | 分离 N/C、优选安全、就绪状态和诊断数据。H 仍表示名义间距；E 仍是硬性预留量；保守执行几何和单一硬安全规范路径继续有效。 |
| P2 v0.4 MissionAdapter 纯转换要求 | 仅依据状态即可进行转换 | 第 27 节增加就绪状态纵深门禁；适配器仍不负责规划或几何评估。 |
| P2 v0.2 §§90–110 工作包、验收和 DoD | 旧工作包顺序以及适用于所有新 V05 工作的数值完整性 DoD | 第 32–36 节规定 V05 工作包和依策略确定的验收。历史验证记录不作改写。 |

除上述明确变更外，确定性几何、导航角度约定（0 度为北、90 度为东、顺时针为正）、复用 MonotoneCoverage 基元、
按计划隔离的上下文、custom-build 集成、路径角色用途语义以及无损航点转换继续具有权威效力。

### 3.3 审查后需协调 AGENTS.md

对于本工作包，与 AGENTS.md 冲突的当前明确编写指令优先。V05-00B 不得修改 AGENTS.md。V05-00C 协调时必须区分以下变更：

| 已提交的 AGENTS 条款 | 将 v0.5 审查通过后纳入权威要求时的协调事项 |
| --- | --- |
| §§2、7–8：阶段依据及 v0.2/BCD 基线 | 增加审查通过的 v0.5 有效权威和自适应流程；保留历史基线引用。 |
| §§9–11、17–21：分层、纯度和集成 | 保留这些边界；诊断/评估算法不得移入 QML 或 ComplexItem。 |
| §12：MissionAdapter 边界 | 保持仅转换且不得重规划；增加明确的就绪门禁。 |
| §§13–14 及 §25 的持久化部分 | 以 Clean Break 和新 schema 版本取代旧 Marine 专属 schema 的运行时兼容。 |
| §§14、20、24：共享数据、展示和失效 | 记录目标/导航/安全分离、诊断/就绪元数据及基于指纹的失效规则。 |
| §§15–16：范围和泛化排除项 | 仅允许批准的名义残余/诊断可视化、Auto、策略和修补契约；不得引入传感器质量图、通用运行时框架或其他排除功能。 |
| §§22–23、25：工作顺序、测试和 DoD | 采用 V05-01–V05-10 及策略/就绪验收；保留相关算法回归和未完成的外场前置条件。 |

其他 AGENTS 规则不被取代。本规范不采用仅见于 Alignment 草案的阶段或里程碑术语。

## 4. 预发布 Clean Break 策略

受支持的 Marine 专属持久化基线必须严格限定为：

| 层 | 支持的版本 |
| --- | --- |
| 顶层 Marine extension | `marine.version = 2` |
| MarineTask | `version = 3` |
| 覆盖规划产物 | `version = 3` |

运行时必须拒绝 Marine Task v1/v2、规划产物 v1/v2、旧版 Marine extension，以及缺少 v0.5 必需身份/就绪元数据的产物。
旧版无角色 Marine 产物不得执行。错误必须明确指出不受支持的开发期 schema 及其层级/版本。

运行时不得自动迁移，不得猜测旧版默认值，也不得设置旧产物 ReviewRequired 兼容模式。历史文件转换如有需要，应作为独立的
开发/工具任务处理。版本号不得重置或复用；Git 历史保留各历史版本的含义。

此不兼容切换仅适用于项目自有的 Marine 专属 schema。不得改变上游 QGroundControl `.plan` 兼容性，也不得仅因普通上游计划
没有 Marine extension 就拒绝它。

## 5. 术语

| 术语 | 规范含义 |
| --- | --- |
| C / CoverageArea | `coverageBoundary` 表示的区域；定义观测任务。 |
| N / NavigationArea | `navigationBoundary` 表示的区域；限定允许的静态行驶范围。 |
| O | 经验证的 No-Go 多边形并集；导航禁区。 |
| T / CoverageTarget | C 减去 O，表示为多边形区域集。 |
| RawNavigationFreeSpace | 扣除 O 且尚未进行间距偏移的 N 区域。 |
| H / P / E | 硬性名义间距、优选名义间距、额外硬执行预留量。 |
| 活动执行 Track 区域 | 当前候选生成所用的 PreferredExecutionTrackRegion 或 HardExecutionTrackRegion。 |
| 规范路径 | 唯一选定的硬安全路径；是否允许转换成 Mission 由就绪状态单独决定。 |
| DiagnosticCandidate | 单独存储并明确不可执行的说明性路径。 |
| DiagnosticOverlay | 用于说明问题，不构成路线或 Mission 路径的几何。 |
| Coverage / Transit | 航段用途，与安全分类相互独立。 |
| 覆盖策略 | 对统一覆盖评估采用 Standard 或 Strict。 |
| 修补分量 | 用于改善覆盖残余的确定性目标相关边界支持分量。 |

所有规划距离和面积必须在一致的局部坐标系中使用米和平方米。两条边界及所有 No-Go 几何必须使用相同的计划级坐标转换；
地理坐标转换由适配器负责。不得混淆数值几何有效性、导航可行性、覆盖质量、就绪状态和车辆实际执行安全。

## 6. Task v3 schema 语义

Task v3 必须保留任务身份、任务类型、显示名称、车辆关联和现有传感器配置。以下语义归属及持久化规划输入为必需项：

| 归属 | 字段 / 契约 |
| --- | --- |
| `region` | 显式的 `coverageBoundary`、`navigationBoundary`、`noGoRegions[]`。 |
| `coverage` | `swathWidthM`、`coverageRequirement`、`sweepAngleMode`、`sweepAngleDeg`。 |
| `safety` | `hardSafetyMarginM`、`preferredSafetyMarginM`。 |
| `planner` | 请求的 `plannerId`；`executionSafety.executionMarginM`。 |
| `sensors` | 现有相机/声呐启用和记录配置；不新增运行时协议。 |

Safety 不得继续在语义上归属于 CoverageConfig。C++ 结构体布局属于实现细节，但领域归属边界不是。Swath 必须为有限正值；
H/P/E 必须符合第 9 节。坐标和明确的数值输入必须为有限值。枚举/领域值及必需字段类型必须经过校验，不得从无关旧字段推断。

本规范批准的新任务默认值为 `plannerId = marine.coverage.auto` 和 `coverageRequirement = Standard`。创建时可以将
navigationBoundary 初始化为 coverageBoundary 的副本；此后两者必须作为独立的显式值保存。编辑 C 不得暗中扩张或修改 N。
本规范不冻结新的 H/P/E 或 Swath 数值默认值。开始规划前，任务必须从创建输入/配置中取得明确且有效的值。

Sweep 模式为 Manual 或 Auto。Manual 角度沿用导航角度约定，并按 180 度取模归一化。Auto sweep 与 Auto planner 是不同输入。
即使 Auto 模式下角度不生效，持久化的 sweepAngleDeg 仍必须显式存在；指纹必须区分角度生效与不生效的语义。

## 7. CoverageArea / NavigationArea / No-Go 模型

v0.5 支持 C 和 N 各一个简单、有限、无自交且非退化的多边形，以及有限个简单、有限、非退化的 No-Go 多边形。
本节保留已提交 P2 v0.2 §§3.1–3.2 对简单多边形和 No-Go 两两关系的限制，但按批准的 C/N 分离，将障碍物包含边界
改为 N；同时允许 No-Go 按下列规则与 C 发生关系。受支持的输入拓扑为：

1. C 必须包含于 N。允许 C 与 N 相等，也允许二者边界接触。
2. 每个 No-Go 必须严格位于 N 内部，且不得接触或越过 N 的边界。
3. No-Go 多边形之间不得重叠、接触或相互包含。不得暗中修复无效拓扑。
4. No-Go 可以位于 C 内、C 外但 N 内，也可以与 C 的边界相交/接触，前提是所得多边形区域几何有效。旧规范中 No-Go 必须严格位于
   覆盖边界内的要求在此取消。
5. C 减去 O 后必须得到面积为正且数值有效的目标区域。空的/退化的观测目标属于无效输入，不能视为覆盖自然成功。
6. 后端派生的目标区域可以含孔洞并有多个分量。不得仅因旧结果类型只能容纳带孔洞的单个多边形就拒绝它们。

位于 C 以外的障碍仍必须约束导航、连接段、转弯和诊断路由。不得仅因它没有从覆盖目标中扣除面积就将其过滤掉。
不得为求解成功而暗中扩张、移动、简化或以其他方式修改用户几何。

多个 NavigationArea 输入多边形、非简单输入多边形、No-Go 与 N 接触、No-Go 彼此冲突以及违反上述输入规则的情况均属于
InvalidInput / None。可以附带 D3 说明性覆盖图层，但不得进入候选求解，也不得产生规范路径或诊断路线几何。

由偏移操作导致障碍合并或区域分裂属于派生几何，不属于输入拓扑违规。对于有效输入，若派生连通性/能力不受支持，或未能得到
连续的硬安全候选结果，则属于规划失败：有意义的原始候选存在时为 Failed / DiagnosticOnly / D2，否则为 Failed /
DiagnosticOnly / D3。若存在硬安全候选但覆盖不完整，则按 Success / ReviewRequired 或策略通过时的就绪状态处理；不得仅因
覆盖不足就将其重新归类为拓扑输入无效。任意不连通域的 Mission 分区仍不在范围内。

## 8. CoverageTarget / RawNavigationFreeSpace

```text
CoverageTarget = T = C - O
RawNavigationFreeSpace = N - O
C subset-of N
```

CoverageTarget 必须为 PolygonRegionSet，并为评估和可视化提供分量身份。每个目标分量都必须参与质量评估，包括候选路径未到达
的分量。评估器不得按策略实际覆盖的范围缩小 T。

覆盖生成和分解必须以 T 为依据。N 可以提供安全布置覆盖中心线、转弯、连接段和转场的空间，但不得因此成为额外的观测要求。
尤其禁止采用 `BCD(N) -> 覆盖每个导航单元` 作为 v0.5 语义。

结果至多包含一条连续规范路径。多个目标分量可以通过安全导航空间连接；这不代表允许拆成多个 Mission 或分配多辆车辆。
不得用未经认证的路径段跨越不连通的活动区域。必须报告不受支持的连通性；能够连续生成的安全候选仍须对完整且未改变的 T
进行评估，并保留所有覆盖缺口。

## 9. H/P/E 安全模型

```text
H = hardSafetyMarginM       finite, H >= 0
P = preferredSafetyMarginM finite, P >= H
E = executionMarginM        finite, E >= 0
```

H 表示路径中心线与 NavigationArea 及 No-Go 边界之间的硬性名义间距。明确命名不得削弱历史名义安全要求。P 表示包含 H 在内的
优选名义间距，不是在 H 上再额外叠加的数值。E 表示航点容差、跟踪和转弯动力学所需的额外硬性预留量，沿用 v0.4 含义。

规划/修补不得自动降低 H 或 E、停用 E、扩张 N 或改变 O。仅允许通过明确的候选层级流程从 P 回退到 H。此回退不得修改任务中
保存的 P。违反优选间距时，对仍符合硬安全的几何结果发出警告；这不构成硬安全豁免。

H/P/E 不代表引入车辆外形模型，也不代表对完整动态运动进行认证。当前车辆位置到任务路径的进入路线不在范围内。每个展示的结果
必须带有以下说明：
“未评估从当前车辆位置到首个 Mission 航点的进入路线。”

## 10. Track-region 方程与不变量

```text
NominalHardTrackRegion
    = Inset(N, H) - RoundInflate(O, H)

HardExecutionTrackRegion
    = Inset(N, H + E) - ConservativeMiterInflate(O, H + E)

PreferredExecutionTrackRegion
    = Inset(N, P + E) - ConservativeMiterInflate(O, P + E)

PreferredExecutionTrackRegion subset-of HardExecutionTrackRegion
HardExecutionTrackRegion subset-of NominalHardTrackRegion
```

操作必须使用经过验证的 Marine 自有几何 API。保守执行膨胀在拐角处仍须安全；不得降低后端精度以获得稀疏航点或制造保守的假象。
这些方程定义的是区域集运算，并允许偏移导致分量变化；不得假定所有中间结果都只有一个多边形。边界归属按经过验证的闭区域
谓词确定；孔洞内部禁止通行。

后端必须证明所要求的包含关系。无法建立保守几何属于几何/规划失败，不属于优选安全警告。优选区域在数学上为空仍可满足子集
关系；此时允许尝试硬安全层级。硬安全区域为空时不存在硬安全路径。几何运算失败不得当作普通空区域处理。

每个规范路径段（包括航线连接段和修补转场）都必须位于 HardExecutionTrackRegion。优选层级的候选结果还必须满足
PreferredExecutionTrackRegion。发布规范结果前必须验证整段路径，不能只检查端点。

全目标覆盖幅宽可达性与导航安全必须分别评估。无法到达 T 的全部区域不得一概中止安全候选生成；结果可以通过 Standard，
也可以为 ReviewRequired。只要存在连续硬安全候选，历史半幅宽/可达性失败门槛就不得继续作为无条件的 Success 阻断项。覆盖缺口
不允许因此放宽硬安全要求。

## 11. PlanningStatus（规划状态）

| 状态 | 含义 |
| --- | --- |
| Success | 存在经过验证的硬安全规范候选；该状态本身不允许上传。 |
| InvalidInput | 必需 schema、数值、几何或受支持的输入拓扑不变量被违反。 |
| Failed | 未能生成硬安全规范候选；可以存在单独的诊断信息。 |

Success 可以与 Ready、ReadyWithWarning 或 ReviewRequired 同时出现。不得仅因覆盖失败就将已有硬安全候选改为 Failed 或清除路径。
InvalidInput/Failed 的规范路径必须为空。错误/问题语义必须区分导航不安全或不可用、能力不受支持、覆盖不足以及几何数值失败；
不得再用旧名称 CoverageImpossibleWithSafetyMargin 将它们合并为一类。

## 12. MissionReadiness（任务就绪状态）

MissionReadiness 由 None、Ready、ReadyWithWarning、ReviewRequired 和 DiagnosticOnly 构成。

| 存在硬安全规范路径 | 覆盖评估 | 覆盖策略 | 满足优选安全 | 就绪状态 | 允许上传 |
| --- | --- | --- | --- | --- | --- |
| 是 | 可靠完成 | 通过 | 是 | Ready | 是 |
| 是 | 可靠完成 | 通过 | 否 | ReadyWithWarning | 是 |
| 是 | 可靠完成 | 未通过 | 任意 | ReviewRequired | 否 |
| 是 | AssessmentError | 不可判定 | 任意 | ReviewRequired | 否 |
| 否，且已完成有效输入的求解 | 不适用 | 不适用 | 不适用 | DiagnosticOnly | 否 |
| 输入无效 | 不适用 | 不适用 | 不适用 | None | 否 |

若覆盖评估错误，且硬安全规范路径已经独立验证，则 PlanningStatus 必须为 Success，MissionReadiness 必须为 ReviewRequired，并产生
CoverageAssessmentFailed 问题码；保留路径并阻止上传。此时 CoverageQualityStatus 为 AssessmentError，不能据此声称覆盖通过或未通过。
InvalidInput/Failed 不适用此例外。其余 Success 可以与 Ready、ReadyWithWarning 或 ReviewRequired 同时出现；不得仅因覆盖不足就将已有
硬安全候选改为 Failed 或清除路径。None 也用于表示 Unplanned/过期状态，即当前没有经批准的结果。ReviewRequired 必须保留硬安全规范路径及覆盖残余。仅确认或签收
不得让该结果变为可上传；当前验证结果必须通过所选覆盖策略。本表描述计算得到的候选结果，不是车辆运行状态机。就绪状态不得声称
已认证从实时车辆位置进入任务路径。

## 13. D0–D3 安全/诊断层级

SafetySolutionTier 必须与 MissionReadiness 和覆盖质量保持独立。

| 层级 | 几何契约 | 与就绪状态的关系 |
| --- | --- | --- |
| D0 — 优选可执行 | 存在规范路径，且每段均位于 PreferredExecutionTrackRegion 内。 | 覆盖策略通过：Ready；可靠评估后未通过或 CoverageQualityStatus = AssessmentError：ReviewRequired。 |
| D1 — 硬安全可执行 | 规范路径位于 HardExecutionTrackRegion 内；至少一段不满足优选区域。 | 覆盖策略通过：ReadyWithWarning；可靠评估后未通过或 CoverageQualityStatus = AssessmentError：ReviewRequired。 |
| D2 — 诊断候选 | 不存在硬安全规范候选；RawNavigationFreeSpace 内存在连续候选，但违反硬性/执行间距。规范路径为空。 | Failed / DiagnosticOnly；绝不可上传。 |
| D3 — 仅诊断/无候选路线 | 不存在硬安全规范候选，也没有有意义的连续原始候选；或者属于不受支持的拓扑/输入类别。规范路径为空，诊断候选可以不存在。 | 有效输入求解失败为 Failed / DiagnosticOnly；无效输入为 InvalidInput / None。绝不可上传。 |

D0/D1 中的“可执行”表示几何满足硬安全条件，不代表已获准上传。AssessmentError 对应 ReviewRequired，不属于覆盖策略未通过。
D1 覆盖策略通过时为 ReadyWithWarning。不得将不安全坐标放入规范路径来伪造 D2/D3。

仅当不存在硬安全规范候选时，才可以为解释问题尝试诊断求解。D2 可以违反 H、E 或 P，但必须是原始自由空间内真实、连续的路径，
不得穿过 No-Go。D3 必须通过问题/覆盖图层解释不受支持或不连通的几何。未解决连接段的示意线只能是覆盖图层，不能伪造成路线。
诊断层级不改变受支持的 Mission 拓扑。

## 14. 规范几何与诊断几何

选定的 `CoveragePlanningSolution.path` 和 `PlanningResult.path` 只能包含一条硬安全规范路径，也可以为空。候选搜索可以考察
多个内部候选，但至多发布一条规范结果。Success / ReviewRequired 可以保留该路径，但不因此授权生成 Mission。

DiagnosticCandidate 和 DiagnosticOverlay 必须使用与规范几何分开的数据成员/类型，不得作为 MissionAdapter 的输入。
DiagnosticCandidate 保存自己的路径及诊断航段评估。覆盖图层可以指向间距违规、未覆盖区域、不受支持的拓扑、不可达区域或未解决
的连接。其坐标系和几何引用必须明确。

PathLegRole 仍严格限定为 Coverage 或 Transit，用于表示用途。规范航段的 PathLegAssessment 必须分别标识 PreferredSafe 和
HardSafeWarning。规范路径评估不得包含 HardUnsafe 或 ExecutionUnsafe 状态；不安全评估只能放在诊断数据中。

非空规范路径必须至少包含两个有限点，路径长度必须为正，并且角色和规范评估数量必须恰为 `path.size() - 1`。每个航段都必须
具有有效的非零长度；组装时必须处理重复连接点，且不得破坏元数据。CoverageLength 与 TransitLength 之和必须在数值容差内等于
实际规范路径长度。规范几何为空时不得保留可执行航段元数据。诊断路径指标不得混入规范路径/Mission 指标。

## 15. Planner Auto 与解析后的策略

新 Task v3 必须请求 `marine.coverage.auto`。Auto 必须区分请求的规划器身份和最终解析出的策略，持久化两者，并说明升级策略的原因。
生产策略概念上包括 SimpleMonotone 和 BCD；不需要通用能力协商框架。

策略升级必须主要由能力/拓扑决定：所选/固定扫描角下目标非单调、目标或受支持的 No-Go 拓扑需要分解，或触及其他明确的简单策略
能力边界。此类升级必须在来源信息/问题中记录 PlannerEscalated。

不得将首个简单策略候选的覆盖不足重新标记为拓扑失败。有效的硬安全简单策略候选必须先进入统一评估；如有需要，也必须先进入获批
的修补阶段，然后才能考虑其他策略处理。仅因简单策略修补后仍不足，不授权自动升级到 BCD；该情况可以保留为 ReviewRequired。

Auto planner 和 Auto sweep 必须在输入、界面、来源信息和测试中明确区分。策略解析及修补期间，Manual sweep 保持固定。Auto sweep
按照其记录的策略语义，确定性地选择一个全局扫描角；不得自动执行参数敏感性搜索，也不得暗中更改角度来消除覆盖缺口。来源信息
必须记录模式、选定角度及策略语义身份。

## 16. SimpleMonotone 策略

SimpleMonotone 是 v0.5 的简单往复式覆盖策略。在适用时建议复用纯 MonotoneCoverage 基元，并分别处理以目标为依据的航线布置和
中心线/连接段的可导航性。它必须支持新的 T/N 和 H/P/E 契约、统一评估器及就绪语义。

不得仅因目标凹陷就要求分解。是否适用取决于所选扫描方向和实际拓扑/能力。如果目标可以表达为一个单调覆盖任务，则必须先考虑
简单策略，再考虑 BCD。所有生成的连接段必须通过活动导航执行区域验证；安全间距区域不必与观测边界重合。

不得暗中将冻结的 P1 LawnmowerCoveragePlanner 改作 Auto，也不得声称它支持当前会拒绝的非零执行配置。相关 P1 算法回归行为
继续有效。复用基元不表示保留废弃的 Marine 持久化格式，也不允许绕过 v0.5 评估器。

## 17. BCD 的目标/导航职责

当受支持的目标拓扑需要分解时，使用 BCD 复杂策略。覆盖义务和单元必须从 CoverageTarget 推导，包括 No-Go 导致的目标分割；
不得从 N 的每个部分推导覆盖义务。路由和中心线布置必须使用选定的活动执行 Track 区域。

改造后的策略必须区分目标覆盖单元/片区与车辆为覆盖它而可行驶的几何区域。不得将导航单元替代为强制覆盖单元，也不得在最终评估
中用执行区域替代 T。N 较大时，不得因此系统性覆盖 N 减 C 的区域。

该策略可以保留受限的事件驱动条带分解、共享的纯单调覆盖基元、Forward/Reverse 遍历、确定性的有向单元排序，以及 Visibility
Graph + Dijkstra 路由。组装候选中的每次转换都必须安全。无法到达/覆盖的目标区域必须保留在残余评估中，不得从 T 中消失，
也不得被 Transit 误算为已覆盖。

单条连续规范候选是唯一可用作 Mission 的几何。该策略不得创建多个 Mission、在车辆之间分配任务、用不安全直线跨越不连通区域，
或调用无条件边界支持。组装后的覆盖质量由第 18 节决定，不由分解成功与否或访问单元数量单独决定。

## 18. 统一 CoverageQualityEvaluator（覆盖质量评估器）

每个 v0.5 生产策略和每个修补候选都必须使用与策略无关的同一评估器契约。评估器必须接收未改变的目标区域集、规范候选、角色、
Swath、所选策略及策略语义版本。安全资格必须单独确认；评估器不得仅凭覆盖良好就认证不安全路径。

```text
CoveredFootprint = union of round-cap buffers of Coverage-role legs, radius = swathWidthM / 2
CoveredTarget = CoverageTarget intersection CoveredFootprint
UncoveredRegion = CoverageTarget - CoveredFootprint
CoverageRatio = Area(CoveredTarget) / Area(CoverageTarget)
```

Transit 不得计入覆盖。T 之外的面积不得提高 CoverageRatio。重叠的覆盖足迹只能计算一次。输入目标面积必须为有限正值。实现必须
一致处理数值残余；不得将无效几何/评估失败转换为策略通过。

覆盖质量状态类型 `CoverageQualityStatus` 必须明确支持且仅按以下语义区分 Complete、Acceptable、Insufficient 和 AssessmentError：

| CoverageQualityStatus | 语义 |
| --- | --- |
| Complete | 覆盖评估可靠完成，且达到数值完整性。 |
| Acceptable | 覆盖评估可靠完成；未达到数值完整性，但通过 Standard。Strict 不接受此状态。 |
| Insufficient | 覆盖评估可靠完成，但未通过当前所选覆盖策略。CoverageBelowRequirement 必须且仅用于此状态。 |
| AssessmentError | 因几何、数值或评估器故障，无法可靠完成覆盖评估；不代表覆盖通过或未通过。 |

评估至少必须包含 targetAreaM2、coveredAreaM2、uncoveredAreaM2、coverageRatio、criticalUncoveredAreaM2、
numericalToleranceM2、上述质量状态、所选策略及是否通过、策略语义身份。Strict 只接受 Complete。评估器失败时必须报告
AssessmentError，不得以 Insufficient、CoverageBelowRequirement 或其他策略结论代替。对于已独立验证的硬安全规范路径，
AssessmentError 必须按第 12 节处理为 Success + ReviewRequired，并发出 CoverageAssessmentFailed。Coverage Repair 只有在覆盖评估
可靠完成且 CoverageQualityStatus = Insufficient 时才可以触发；AssessmentError 不是策略未通过，不能触发修补。

评估还必须返回与目标关联的残余几何，足以显示边界覆盖不足和关键未覆盖区域。仅返回标量不足以满足要求。同一目标、路径、角色、
Swath 和策略语义必须在所有策略下得到相同评估。若存在经过独立验证的硬安全规范路径，但覆盖评估因数值或几何错误而失败，
结果状态必须为 Success，MissionReadiness 必须为 ReviewRequired，并产生 CoverageAssessmentFailed 问题码；必须保留该路径，
同时保留可用的残余几何或明确标记残余不可用。该结果不得上传，也不得将评估错误误报为覆盖不足。若不存在硬安全规范路径，
则保留相应的 Failed / DiagnosticOnly 或 InvalidInput / None 状态。AssessmentError 对应的指标及残余几何不得用于修补决策。
任何评估失败都不得被转成策略通过或策略未通过。

## 19. 数值容差

面积数值容差定义为：

```text
NumericalCoverageTolerance(A) = max(0.01 m², 1e-6 * A)
```

整体目标评估中的 A 为 CoverageTargetArea。该容差用于处理几何后端、格网、缓冲和差集运算产生的残余。不得将其解释为 Standard
产品质量门槛，也不得为使候选通过而调大，或将其作为操作员可配置的质量设置。

评估器的所有运算必须使用一致单位，并报告所用容差。非有限面积、超出数值误差范围的负面积，或无效的差集/交集几何均属于评估失败，
不能视作可接受的覆盖不足。数值归一化不得掩盖有实际意义的关键缺口。

## 20. Standard / Strict 策略

Task v3 只支持 Standard 和 Strict，不引入 Custom 策略。新任务默认使用 Standard。Strict 仍可用于研究、回归测试和要求数值完整的任务。

```text
StrictPass = TotalUncoveredArea <= NumericalCoverageTolerance(CoverageTargetArea)

StandardPass =
    CoverageRatio >= StandardCoveragePolicy.minimumCoverageRatio
    AND CriticalUncoveredArea <= NumericalCoverageTolerance(CoverageTargetArea)
    AND the empty/degenerate-core safeguard in section 21 passes
```

Standard 允许有限的边界覆盖不足，同时防止用较高的整体覆盖率掩盖内部缺口。Strict 仅允许数值残余，不提供产品层面的边界容差。
两种策略都对相同的 T、覆盖足迹、安全合格候选及数值公式进行评估。切换策略不得修改几何、Swath、H、P、E 或数值容差。

Standard 策略仅有第 37 节列出的两个常量尚未确定。所有门槛实现必须等到责任方批准校准值后才能开始。不得采用临时数值常量
或猜测运行时默认值。校准语义批准后，策略身份必须能标识该语义。

## 21. CriticalCoverageCore（关键覆盖核心）

```text
CriticalCoverageCore = Inset(CoverageTarget, StandardCoveragePolicy.boundaryToleranceM)
CriticalUncovered = UncoveredRegion intersection CriticalCoverageCore
BoundaryShortfall = UncoveredRegion - CriticalUncovered
```

Inset 是纯区域集几何运算。所有相关目标边界都参与计算，包括外边界和由 No-Go 形成的边界。它们使用统一边界容差；v0.5 不设
单独的外边界/孔洞策略。Inset 结果可以是区域集，不要求为单一多边形。

对于区域集 T，经批准的防护规则中的“受影响目标几何”指每个连通目标分量。必须逐分量评估，避免狭窄分量被另一个具有较大核心的
分量掩盖。若分量核心为空，或其面积不大于以该分量目标面积计算的 NumericalCoverageTolerance，则视为数值退化，并触发防护规则。
对每个受影响分量，Standard 必须要求该分量满足 Strict 数值完整性，使用相同容差公式并代入该分量的目标面积。整体目标覆盖率
和关键未覆盖面积检查仍然适用。若整个目标核心为空/退化，则还必须对整个目标执行 Strict 检查。Inset 无效属于评估失败，不能
视为空核心后判定通过。

评估必须公开是否触发回退以及受影响的几何，以便测试和界面解释为何狭窄/较小的目标需要数值完整。不得将空核心回退实现为
CriticalUncoveredArea = 0 后无条件接受 Standard。

## 22. 条件式 Coverage Repair（覆盖修补）

通用生产流程如下：

```text
初始硬安全候选
    -> 统一覆盖评估
    -> 所选策略通过：保留初始候选，不做边界修补
    -> 评估可靠完成且 CoverageQualityStatus = Insufficient：可尝试目标相对的条件式 Coverage Repair
    -> CoverageQualityStatus = AssessmentError：保留硬安全结果并设为 ReviewRequired；不得触发 Coverage Repair
```

只有当覆盖评估可靠完成并产生 CoverageQualityStatus = Insufficient 时，Coverage Repair 才可以触发。只有初始候选的评估
结果为 CoverageQualityStatus = Insufficient 时，才允许根据该次可靠计算的未覆盖目标残余决定修补。Complete 或 Acceptable 表示
所选策略通过，不得执行边界修补；AssessmentError 不是策略失败，不得触发修补，也不得依据其不可靠的指标或残余几何作出修补决策。
此要求适用于所有策略和安全层级的尝试，是 M00 的核心回归不变量。但这不禁止初始策略为形成连续路径所需的普通连接段。

规划器可以另行生成一个独立的硬安全候选，并对该候选重新执行覆盖评估。该新候选只有在自身评估可靠完成且结果为 Insufficient 时，
才可触发修补；先前 AssessmentError 结果及其指标/几何不得作为修补触发条件或收益基线。

修补必须由未覆盖的 CoverageTarget 驱动。NavigationArea 或 Track 区域的周界本身不是覆盖义务。只有当已有边界支持基元的几何
确实用于目标相对修补时，才可以复用。禁止遍历所有导航边界、添加合成周界航线，或要求每个候选都先组装边界航线。

修补以分量为单位：每个可用分量都必须是确定性的、与目标相关的边界支持分量。任意选择部分边界弧段不在范围内。每次修补必须
只添加一个有用分量，重新组装并评估候选；所选策略一旦通过就立即停止。重新评估前不得追加所有可用分量。

结果必须记录是否尝试/应用修补、原因、所选分量来源、覆盖改善量以及路径/转弯指标变化。若没有实际加入修补分量，
repairApplied 不得为 true。若已无可用的安全分量而覆盖仍未通过，则以 Success / ReviewRequired 保留最佳硬安全规范候选及其
残余，并阻止上传。

## 23. 修补顺序和不变量

修补分量必须按以下字典序决策：

1. 该分量及其完整组装必须安全地位于当前优选/硬执行区域。
2. 与当前候选相比，未覆盖 CoverageTarget 必须按第 24 节完全相同的 CoverageQuality 比较器获得严格改善，即首个不同的量化键分量必须更优。
3. 对满足第 2 项的分量，完成统一评估器和所选策略的重新评估后，优先选择按同一比较器排名更高的分量。
4. 优先选择安全转场成本更低的分量。
5. 仍相同时，按稳定的规范分量 ID 排序。
6. 再按稳定的合法入口点索引排序。
7. 最后按稳定的遍历方向顺序排序。

安全转场成本必须来自活动区域内的合法路由；穿过禁区的直线距离不是有效的连接成本。相同规划输入必须得到规范且可重复的分量 ID、
点索引和方向顺序，且结果不得受容器遍历或内存分配顺序影响。不得使用加权目标函数。几何表示发生变化时也必须保持此决策顺序。

修补可以添加 Coverage 和安全 Transit 航段，选择合法入口/方向/顺序，并重新组装完整规范候选。不得降低 H 或 E、暗中修改 P、
扩张 N、修改 C 或 O、改变 CoverageRequirement 或 Swath，或绕过硬执行安全。硬层级尝试必须明确使用未变更的 H + E 限制，
不能通过修改优选安全配置来实现。

每次重新组装都必须一致地重建路径角色、航段评估、指标和来源信息。包括新路由连接段在内的完整候选必须通过安全验证和统一覆盖评估。
最终验证后如再添加几何，必须重新组装并验证完整结果。无效或未改善的尝试不得替换已保留的有效硬安全候选。可用分量数量有限，
不得反复插入分量形成修补循环。

## 24. 候选选择的字典序优先级

候选选择必须按以下优先级执行，不得计算加权分数：

1. 硬性可执行安全：强制资格筛选。
2. 覆盖认证类别：策略通过优先于评估可靠完成但策略未通过；后者优先于 AssessmentError。该排序表示认证结果的可靠程度，不表示 AssessmentError 在几何上覆盖更差。
3. 在同一“策略通过”类别或同一“可靠评估但策略未通过”类别内，覆盖质量由统一评估器按下述确定性比较器判定。AssessmentError 没有 CoverageQuality 排序键。
4. 是否满足优选安全。
5. 路径/转场长度。
6. 转弯次数。
7. 稳定的确定性平局规则。

只有 CoverageQualityStatus 为 Complete、Acceptable 或 Insufficient，且评估指标有效的候选，才能参与以下质量比较。AssessmentError
没有可用于排序的质量键，不得伪造覆盖指标或让它替代任何可比较的硬安全规范候选。在可比较候选之间，统一 CoverageQuality 比较器
必须按以下顺序比较，并对所有策略和修补尝试保持一致。对本次比较中的每个
候选，以相同的 `numericalToleranceM2` 定义面积量化单位 `q`：`q(x) = floor(x / numericalToleranceM2 + 0.5)`。随后按
`(q(criticalUncoveredAreaM2) 升序, q(uncoveredAreaM2) 升序)` 作字典序比较；较小者质量更好。完全相同则质量相同，继续比较
第 4 项优选安全。coverageRatio 仍由评估器计算、展示和用于策略通过判定，但因其由 coveredAreaM2 / targetAreaM2 唯一确定，
不再作为重复的平局指标。此量化键具备确定性和传递性；不得用两两浮点 epsilon 比较或策略专属定义打破平局。可比较候选的路径/
转场长度及转弯指标必须用一致方式计算；不得将不存在/不适用的单元指标伪造为零，以制造质量优势。

硬安全且策略通过的候选必须优先于硬安全但可靠评估后策略未通过的候选。因此，生成任意优选安全路径都不足以作为跳过硬安全层级的理由。
对适用策略，候选生成和条件修补必须充分考虑各种候选，依次确定是否存在优选安全且策略通过的解；否则是否存在硬安全且策略通过的解；
再否则是否存在可靠评估但策略未通过的硬安全解。若剩余硬安全候选的覆盖评估全部为 AssessmentError，则不使用覆盖质量键，
改为依次按满足优选安全、路径/转场长度较短、转弯较少、稳定确定性平局规则选择。该情形的选定结果必须为 Success + ReviewRequired，
且不得上传。所有实际比较的候选均须遵循上述优先级。

规划器不必穷举所有可能的几何路线。本规则不引入全局优化或自动参数搜索。但当优选候选策略未通过或发生 AssessmentError 时，
必须考虑所有受支持的优选/硬安全层级候选。条件修补仍只可由同一候选的可靠 Insufficient 结果触发。只有不存在硬安全候选时，
才可通过原始空间诊断求解解释不可行性；诊断结果不能参与可执行候选选择。

## 25. 结构化问题

规划问题必须使用稳定的语义码。每条记录必须支持严重级别和面向用户的说明，也可以引用受影响的航段范围及几何。引用必须明确
几何属于规范路径、诊断候选还是覆盖图层/残余；不允许使用含糊的共享索引。说明文本可以本地化。界面行为不得解析英文文本。

| 稳定问题码 | 必须表达的含义 |
| --- | --- |
| PlannerEscalated | Auto 的能力评估将请求的简单策略解析为更复杂策略，并给出能力/拓扑原因。 |
| PreferredSafetyViolated | 规范几何满足硬执行安全，但未达到优选间距。 |
| HardSafetyUnavailable | 未能获得满足硬性名义间距的候选；不得以此为由放宽硬安全。 |
| ExecutionReserveUnavailable | 额外 E 预留量使硬安全候选不可用；E 仍为强制要求。 |
| UnsafeDiagnosticCandidate | 分离存储的原始空间说明几何不满足 H + E，不能执行。 |
| CoverageBoundaryShortfall | 目标边界处存在残余面积；是否可接受由覆盖策略决定。 |
| CriticalCoverageGap | 关键目标几何存在有意义的未覆盖区域。 |
| CoverageBelowRequirement | 覆盖评估已可靠完成，且 CoverageQualityStatus = Insufficient；不得用于 AssessmentError。 |
| CoverageRepairApplied | 已加入有用修补分量；报告覆盖和路径/转弯影响。 |
| CoverageRepairInsufficient | 修补后仍未通过所选策略；保留可用的硬安全几何。 |
| NavigationRegionDisconnected | 相关导航几何不连通，无法建立受支持的连续执行路线。 |
| NavigationRegionUnsupported | 相关导航几何超出支持范围，无法建立受支持的连续执行路线。 |
| UnresolvedConnection | 必需连接尚未解决；任何示意线都只能是覆盖图层，不能作为路线。 |
| IngressNotAssessed | 本认证不包含从实时车辆位置到首个规范航点的路线。 |
| CoverageAssessmentFailed | 覆盖几何/数值评估失败；不代表覆盖通过或覆盖不足。存在硬安全规范路径时保留该路径并置为 ReviewRequired；阻止上传。 |

CoverageBelowRequirement 只对应评估成功完成但 CoverageQualityStatus = Insufficient 的情况。AssessmentError 只能使用
CoverageAssessmentFailed，不得同时报告 CoverageBelowRequirement。严重级别必须反映结果上下文：策略升级/修补/未评估进入路线可以作为提示；优选间距违规属于警告；覆盖策略失败阻止上传，但不否定
硬安全；硬安全不可行则阻止执行。相同输入必须产生确定的问题码、受影响几何和排列顺序。这些是最低类别要求，不允许将就绪状态
简化成通用错误字符串。

## 26. 确定性建议

建议必须由确定性规则生成，并使用稳定语义码和面向用户的说明。可以关联依据问题或几何；建议列表为空也是有效结果。不授权引入 AI
建议引擎、多参数敏感性搜索或自动应用建议。

| 独立稳定建议码 | 触发条件和面向操作员的建议 |
| --- | --- |
| ExpandNavigationArea | PreferredSafetyViolated：考虑扩大导航区域。 |
| ReviewPreferredClearance | PreferredSafetyViolated：复核优选间距设置。 |
| ReviewSwathWidth | CriticalCoverageGap：考虑减小 Swath。 |
| IncreaseNavigationRoom | CriticalCoverageGap：考虑在高亮残余区域附近增加导航空间。 |
| ReviewHardNavigationFeasibility | 硬导航不可行：考虑扩大 N；任务拆分属于未来流程；完成现场评估后再复核硬性要求。 |
| InspectRepairCost | CoverageRepairApplied：报告修补导致的路径长度和转弯变化。 |

建议不得声称更改参数必然可行、暗中修改任务，或把降低硬安全要求当成自动解决办法。操作员修改任何规划关键输入都会使旧产物失效，
并要求明确发起新规划。建议顺序必须稳定。

## 27. MissionAdapter 上传门禁

生成任何 Marine MAVLink 航点之前，MissionAdapter 必须执行以下纵深防护门禁：

```text
uploadable = current/non-stale result
             AND PlanningStatus == Success
             AND (MissionReadiness == Ready OR MissionReadiness == ReadyWithWarning)
```

必须拒绝 ReviewRequired、DiagnosticOnly、None、InvalidInput 和 Failed。不得因缺失/不一致的 v3 元数据而推断任务已就绪。
拒绝时不得从被拦截结果生成部分 Marine Mission。门禁必须在 QML 以下实现；仅禁用上传按钮不够。允许 ReadyWithWarning，但必须
保留并显示警告；不得将警告重新解释为硬安全豁免。

MissionAdapter 仍只转换已经生成的规范 `PlanningResult.path`。不得偏移几何、校验多边形、重新计算覆盖、重新路由、修补路径或调用
规划器。原有坐标/航点完整性检查和序列处理继续适用。Coverage/Transit 角色本身不增加速度、传感器、相机、悬停、RTL 或其他
执行命令。

诊断候选、覆盖图层和残余多边形不得进入 Mission 编码路径。规范几何为空时，适配器不得改选这些数据。
appendMissionItems() 必须使用当前已存结果；加载和上传都不得隐式重规划。门禁保护 v0.5 认证边界，但仍不包括从实时车辆
位置到首个航点的进入路线。

## 28. Task v3 持久化

顶层 Marine extension 必须使用 marine.version = 2。MarineTask 必须使用 version = 3，并按第 6 节明确持久化 C、N、No-Go
多边形、Swath、Standard/Strict 要求、扫描模式和角度、H、P、请求的规划器 ID，以及归属于执行安全配置的 E。任务身份和现有传感器
配置仍属于 Marine 任务数据。JSON 转换继续集中在 MarineTaskJsonCodec。

Marine 复合项必须保留其与计划级上下文的 taskId 关联。不得在 Mission 项中重复保存完整任务。Marine 数据继续与上游 Mission 对象分离。

加载器必须明确拒绝不受支持的 Marine extension/Task 版本、缺失的规划关键字段、未知的必需枚举值及无效字段值。不得用 C 推断缺失
的 N、猜测旧安全字段含义、补造缺失的 v3 身份，或暗中重解释旧 Task。新任务初始化与加载不同：N 可以初始复制 C，但保存后的
两个边界必须是相互独立的显式值。重命名任务或更改非规划用的传感器/显示数据不触发重规划。

Marine Task v1/v2 和 v0.5 之前的 Marine 专属 schema 必须产生“不支持的开发期 schema”错误。不得进行运行时自动迁移，也不得
使用旧版 ReviewRequired 模式。没有 Marine extension 的普通上游 QGC .plan 数据继续保持上游兼容；不得仅因本次 Clean Break 而拒绝。

## 29. Artifact v3 持久化

Planning Artifact v3 必须在不重规划的情况下恢复完全一致的通过/审查/诊断结果。必须按明确版本化语义保存下列信息，不得保留 v1/v2
结构例外：

| 字段组 | 必须保留的信息 |
| --- | --- |
| 结果状态 | PlanningStatus、MissionReadiness、安全/诊断层级及语义/版本来源。 |
| 规范几何 | 唯一规范路径、逐航段 Coverage/Transit 角色及规范航段评估。 |
| 覆盖评估 | CoverageQualityStatus（包括 AssessmentError）、策略/通过结果、标量指标、容差、残余几何、关键/边界区分和核心回退信息。AssessmentError 时，每项未能可靠计算的指标或残余几何都必须显式标记为不可用/缺失；不得伪造为零、空几何、Complete、Acceptable 或 Insufficient。AssessmentError 必须持久化，并在匹配产物加载时原样恢复其状态及各项数据的可用性；加载时不得重新评估。 |
| 规划器来源 | 请求的规划器、解析出的策略及语义身份、能力/升级原因和选定的扫描信息。 |
| 修补信息 | 是否尝试/应用、原因、已加入分量的来源，以及覆盖/路径/转弯影响。 |
| 操作员说明 | 带有效几何/航段引用的结构化问题和建议。 |
| 诊断数据 | 存在时单独保存的 DiagnosticCandidate、DiagnosticOverlays 及其评估。 |
| 指标 | 路径/Coverage/Transit 长度，以及有意义的单元/转弯指标；不适用项必须明确标为不适用。 |
| 输入身份 | 规划输入指纹及判断其有效性所需的版本。 |

加载受支持的 v3 时，加载器必须校验结构一致性，包括有限坐标/可用指标、显式的数据可用性标记、必需枚举值、路径/角色/评估数量、
有效引用，以及允许的状态/就绪/层级组合。AssessmentError 下标记为不可用/缺失的指标不得被要求填成数值。规范路径含 HardUnsafe 内容，
或仅有诊断数据的产物却标为 Ready，均属于格式错误，不得作为可执行结果加载。
这是持久化校验，不会把几何规划或覆盖评估职责移交给 MissionAdapter。

| 加载条件 | 必须采取的行为 |
| --- | --- |
| 有效的 Artifact v3，且规划输入/语义身份与当前匹配 | 准确恢复保存结果，包括路径顺序、就绪状态和说明；不得重规划。 |
| 有效的 Artifact v3，但输入或受支持策略/策略语义身份不匹配 | 标记过期，丢弃可执行规划状态，转为 Unplanned/None；不得自动重规划。 |
| Task/Artifact v1/v2、旧版无角色产物，或缺少 v0.5 必需身份/就绪元数据 | 明确报不支持的开发期 schema；不得猜测就绪状态或进入兼容模式。 |
| 当前 v3 数据格式错误 | 明确报校验错误且不产生可执行结果；不得暗中将诊断数据转为路径。 |

任何规划关键任务字段发生变化，都必须按同一身份规则使产物失效。Unplanned 是规划缺失/过期时的集成生命周期状态，不表示失败产物
已变得安全。如果界面保留了诊断说明，可以继续展示，但必须标为过期；它不得作为当前规划结果或执行来源。

## 30. 规划输入指纹

指纹必须至少关联以下规划关键输入与产物：

- coverageBoundary, navigationBoundary and every No-Go geometry;
- swathWidthM, H, P and E;
- CoverageRequirement and CoveragePolicy semantic version, including approved calibration semantics;
- sweep mode and sweep angle where applicable;
- requested planner ID and resolved strategy/planner semantic identity;
- the v0.5 planning-semantics version.

编码必须由 Marine 自行定义且具备确定性；散列前必须明确规范字段顺序、几何表示、数值编码和枚举身份。不得依赖 QJson 对象键的
任意遍历顺序决定身份。相同输入和语义必须得到相同身份；必须为每种规划关键输入变更编写过期检测测试。具体散列算法属于实现细节。

显示名称、纯界面状态和与规划无关的传感器字段不得进入该指纹。若要让传感器设置影响规划，必须由后续规范批准。

加载时必须将已解析的语义身份与当前支持的策略/版本对比，但不得运行规划器。策略、策略校准或规划语义发生变化都会使旧结果失效；
即使 Task 几何相同，也不得暗中接受旧结果。Auto 不会抹去已解析的身份。存储的散列不能替代 schema/一致性校验或安全评估。

## 31. QML / 可视化语义

QML 负责展示和用户交互；几何、路由、质量评估、修补和就绪状态推导仍由 Marine C++ 层负责。结果展示必须读取结构化字段/编码。

| 展示对象 | 必须采用的语义样式 |
| --- | --- |
| 满足优选安全的规范 Coverage 航段 | 主要任务轨迹样式。 |
| 满足优选安全的规范 Transit 航段 | 中性/次要轨迹样式。 |
| 规范 HardSafeWarning 航段 | 保持规范路径身份，同时显示警告/琥珀色语义。 |
| 不安全的 DiagnosticCandidate | 错误/红色语义，且明确标识为不可执行。 |
| 未解决的 DiagnosticOverlay | 明确作为说明信息，不得视觉上伪装成连通的任务路线。 |
| 边界覆盖不足 | 与 T 关联的半透明警告区域。 |
| 关键未覆盖残余 | 半透明错误/高关注区域。 |

这些是主题语义角色，不是固定颜色常量。即使仅靠颜色无法区分，也必须通过样式/标签区分诊断几何和规范几何。C、N、O、规范路径、
诊断几何及未覆盖几何必须彼此可辨。航段用途与航段安全评估互相独立。

常用控件建议保持简明：覆盖宽度、硬性最小安全间距、覆盖要求、扫描模式/角度及 Planner = Auto。次要/高级区域可以展示优选安全、
执行配置说明、Strict 覆盖和规划器来源。数值几何 epsilon 不得作为用户设置。Planner Auto 和扫描角 Auto 必须在展示、来源信息
和测试中分别处理。

结果必须显示就绪状态、请求/解析后的策略、覆盖率/状态、未覆盖及关键未覆盖面积、安全状态、路径长度/转弯数、问题、建议和修补
使用情况/效果。ReviewRequired 必须保留硬安全路径和残余显示，并解释上传被阻止的原因。D2/D3 必须解释为什么没有可执行路径。
编辑规划关键几何/设置后必须显示结果已过期。

任何结果都不得暗示发射/进入/回收路线已经认证。展示任务就绪状态时必须显示 IngressNotAssessed。界面控件必须遵守与后端相同的
就绪门禁：允许 ReadyWithWarning，拒绝 ReviewRequired/DiagnosticOnly；操作员确认不得绕过门禁。

## 32. M00–M09 验收矩阵

这些场景冻结几何谓词和语义结果，不冻结任意坐标。后续测试夹具必须证明其满足场景谓词、安全可行性和策略条件。CAL-01/CAL-02
获批前，依赖 Standard 数值门槛的夹具不能判定 PASS。M00–M02 使用固定 Manual 扫描角，避免 Auto sweep 遮蔽规划器选择；扫描角
Auto 需单独进行针对性测试。

| 场景 | 必须满足的条件 | 必须得到的语义结果 |
| --- | --- | --- |
| M00 — 简单矩形 | C 为矩形；N = C；无 No-Go；固定 Manual 扫描角；H/P/E 有效；Standard；夹具中的初始候选通过 Standard 和优选安全。 | 请求 Auto，解析为 SimpleMonotone；硬安全、确定性；repairApplied = false；无合成周界航线；Ready；允许上传。 |
| M01 — 凹形但扫描单调 | 无 No-Go；目标为凹形，但在固定扫描角下单调；适用 v0.5 简单策略。 | Auto 解析为 SimpleMonotone；不能仅因凹形而分解；遵循统一安全/覆盖/就绪规则。 |
| M02 — 非单调目标 | 在受支持拓扑内，目标在固定扫描角下需要分解。 | Auto 完成简单策略能力评估后升级至 BCD；升级原因/来源可见；使用统一最终安全和覆盖评估。 |
| M03 — 导航区域更大 | C 位于较大的 N 内；夹具会用到 C 外的导航空间。 | 仅 T 是覆盖要求；合法转弯/转场可离开 C；所有规范几何都满足硬安全；不得系统性覆盖 N 减 C。 |
| M04 — 优选不可行 / 硬安全可行 | 不存在满足场景要求的优选解；存在硬安全且覆盖通过的候选。 | D1；H/E 不变；ReadyWithWarning；显示警告；允许上传。 |
| M05 — 硬安全不可行 / 存在原始诊断 | 无硬安全候选；存在有意义的连续原始空间候选。 | 规范路径为空；单独保存 D2 DiagnosticCandidate；DiagnosticOnly；明显标识为不可执行；阻止上传。 |
| M06 — Standard 通过 / Strict 未通过 | 对相同 T/路径/角色/Swath 评估；边界缺口有限；Standard 通过而 Strict 未通过。 | 两种策略使用相同几何/数值容差；修补行为遵从所选策略；不修改容差。 |
| M07 — 修补成功 | 初始硬安全候选未通过所选策略；存在可弥补缺口的安全目标相对修补分量。 | 有条件启动修补，覆盖有可测改善并在通过后停止；repairApplied = true；就绪状态按优选间距确定；允许上传。 |
| M08 — 修补后仍不足 | 存在硬安全候选，但受支持的修补无法通过所选策略。 | Success / ReviewRequired；保留规范路径和覆盖残余；阻止上传。 |
| M09 — 不支持/未解决的原始拓扑 | 原始拓扑不受支持，或无有意义且受支持的原始连接；无可执行候选。 | D3；无规范路径、不伪造 Mission 连接；问题/覆盖图层说明原因；诊断候选可以为空；阻止上传。 |

M00 必须使用语义判定条件：策略选择、无需的修补/周界航线未发生、硬安全、覆盖、确定性和就绪状态。除非另有理由证明这是规范化
规则，否则与历史单元遍历路径逐点完全相同不能作为产品判定条件。M00 不禁止其他合法采样/入口选择。相同输入重复运行必须得到
相同的 v0.5 选定结果。

M04 还必须覆盖以下优先级情形：存在优选安全但覆盖未通过的候选时，不得因此忽略硬安全且覆盖通过的候选。M08 在保留
ReviewRequired 之前，必须证明受支持的优选/硬安全候选生成及修补尝试均未得到策略通过的候选。M05/M09 必须验证后端拒绝上传，
不能只验证地图显示。M09 输入无效时为 InvalidInput/None；输入有效但求解不受支持时，按第 11–13 节处理为 Failed/DiagnosticOnly。
两种情况都不得生成 Mission 几何。

## 33. 持久化 / Mission / SITL 验证

第 3 节所述基线 HEAD 中的已提交文档确认了以下历史来源：

- 已提交的 P1 冻结审计批准了 P1 Engineering Freeze；P1 真实 USV 外场验证仍延期且未执行。已提交的 P1 外场协议仍列有
  P2 真实 USV 验收前必须完成的外场工作。
- 已提交的 P2 SITL 协议记录了 P2-13K 正式 S01–S04 PASS/CLOSED 及 P2-13 技术验证，同时仍将 P2-14 标为 DO NOT START。
  这些结论只适用于协议中记录的历史运行身份，不认证当前 HEAD、工作区试验或 v0.5。

相关来源文档：
[P1 冻结审计](P1_ENGINEERING_FREEZE_AUDIT.md)、[P1 外场协议](P1_REAL_USV_FIELD_VALIDATION_PROTOCOL.md)和
[P2 SITL 协议](P2_ARDUROVER_SITL_TEST_PROTOCOL.md)。
本修订不得扩大上述历史验收范围。历史回归证据不能认证新的几何、schema、质量或就绪语义。

v0.5 必须建立自己的单元、集成、M00–M09、持久化、上传门禁、SITL 和人工验收证据。验证至少必须覆盖：

1. Task v3/Artifact v3 往返保存，以及明确拒绝 Task/Artifact v1/v2、缺少 N 和缺少当前身份/就绪/角色信息，同时不影响普通上游
   .plan 兼容性。
2. 匹配的产物须准确恢复且不调用规划器；任一指纹输入变化都会使产物过期并转为 Unplanned/None，且不自动重规划；仅显示/传感器等
   非规划字段变化不会导致失效。
3. C/N/O 拓扑、多区域 T、位于 C 外或与 C 边界相交的 No-Go，以及防止意外覆盖整个 N。
4. H/P/E 校验、保守子集不变量、D0–D3 分离，以及不自动降低 H/E。
5. 各策略评估器结果一致、仅以 Coverage 足迹计算、四种 CoverageQualityStatus 及其转换、Standard/Strict、关键缺口、空/退化核心回退，以及数值容差与产品校准分离；评估错误不得伪装为策略未通过。
6. 由能力驱动的 Auto 选择、M00–M02 固定扫描角、独立 Auto sweep、确定性来源信息，以及在导航空间安全路由并由目标驱动的 BCD。
7. 初始候选通过时不修补；安全增量修补、确定性且考虑路由的排序、策略通过后立即停止、失败时保留 ReviewRequired 几何，及覆盖优先于优选安全。
8. MissionAdapter 后端门禁须覆盖状态/就绪组合；允许 ReadyWithWarning，拒绝过期/ReviewRequired/D2/D3/无效/失败/格式错误的结果，且不编码诊断航点。
9. 保持 Task/Plan/Mission 集成、计划级生命周期、相关 P0/P1 算法和 QGC Mission 回归；创建/规划/转换/保存/销毁/重建/加载链路
   只使用当前 Marine schema。
10. v0.5 SITL 和人工结果/可视化检查必须关联明确的测试版本和夹具身份。
11. AssessmentError 不触发 Coverage Repair；只允许同一候选可靠评估得到 Insufficient 时依据其残余启动修补。
12. 候选排序验证可靠评估的策略 PASS 优先于可靠评估的策略 FAIL，且策略 FAIL 优先于 AssessmentError。
13. 所有剩余硬安全候选均为 AssessmentError 时，按 §24 规定稳定、确定地选出结果，并保持 Success / ReviewRequired。
14. AssessmentError 中不可用的覆盖指标/残余几何在 Artifact v3 中以不可用/缺失形式持久化并原样恢复；验证不得出现伪造的零值或空几何。
15. AssessmentError 结果始终被 MissionAdapter 拒绝上传，包括其保留了硬安全规范路径的情况。

测试必须比较语义不变量和确定性结果，不得将工作区试验中的特定路径未经说明地继承为判定标准。未来实现工作包必须执行构建、
针对性测试、回归和独立审查；本规范编写不代表上述任何实现检查已经通过。P1 外场工作和 P2 外场前置条件仍待单独执行并批准。

## 34. 明确不在范围内的事项

v0.5 不得增加以下功能：

- StartAnchor / EndAnchor，或发射/回收/车辆进入路线；
- 车辆外形建模、转弯半径规划、Dubins、Hybrid A* 或完整动态运动可行性；
- 感知洋流/风况的优化、动态避障或在线重规划；
- 在 QGC 中链接 ROS 2 运行时，或新增 ROS/遥测/传感器记录桥接；
- 传感器感知的 EffectiveSwath、实际执行覆盖或断点续规划；
- 多车辆规划、舰队分配或多分量/多 Mission 分区；
- GTSP、MILP、RL 或其他全局优化、自动参数敏感性搜索或 AI 建议引擎；
- 任意选择的局部边界弧修补；
- 自动降低硬安全要求或自动修改 Task 几何；
- 通用能力/版本协商框架、新的规划器/路由器/排序器注册表、完整执行状态机，或为保留废弃开发期 schema 而设立的持久化迁移框架；
- P3 及以后阶段的 2.5D/3D ROV 规划或无关执行命令。

经批准的诊断层级、就绪门禁和确定性建议记录仅属于规划结果语义，不授权引入被排除的运行时/执行框架。未受影响的历史范围要求以及
最小化核心修改/custom-build 限制继续有效。

## 35. V05 工作包顺序

只有在规范通过独立审查并完成 Design Freeze 后才能开始生产实现。V05-00C 根据获批修订协调 AGENTS.md；V05-00B 仅修改本规范。

开始 V05-01 前，必须分类现有工作区修改；经批准的历史工作必须保留/提交；实验性或未批准的工作必须通过明确授权的清理操作
丢弃或归档；并建立干净的 v0.5 基线提交。V05-00B 不得执行该清理。

| 工作包 | 有界职责及依赖 |
| --- | --- |
| V05-01 | Clean Break 领域类型、Task v3、Artifact v3、拒绝旧 schema 及指纹基础设施。 |
| V05-02 | C/N/O 几何分离及目标/导航区域集契约。 |
| V05-03 | H/P/E 区域、安全层级评估及优选/硬安全回退。 |
| V05-04 | 统一 CoverageQualityEvaluator、Standard/Strict 和残余几何；两个校准值获批前，Standard 数值门槛实现均为 BLOCKED。 |
| V05-05 | Auto 规划器、v0.5 SimpleMonotone 和已解析策略来源信息。 |
| V05-06 | 适配目标/导航分离的 BCD，以及不含无条件边界支持的初始组装。 |
| V05-07 | 确定性、安全且考虑路由的目标相对条件修补。 |
| V05-08 | MissionReadiness、分离的诊断候选/覆盖图层数据、问题/建议、完整产物持久化及 MissionAdapter 上传门禁。 |
| V05-09 | QML 编辑器/结果展示、N 编辑、警告/诊断/残余可视化及操作员建议。 |
| V05-10 | M00–M09、回归、持久化、Mission 集成、SITL、人工验收及 v0.5 冻结审计。 |

V05-01 建立 schema/基础设施；V05-08 完成集成结果模型的持久化。前序工作包必须测试其最小层次，不得声称已具备后续包的集成
就绪能力。不得将临时的部分实现表示为可上传的 v0.5 生产结果。

未来每个工作包都必须遵循以下流程：检查当前 API；实现获批的最小修改；增量构建；运行针对性测试；运行相关回归；完成独立 diff 审查；
然后停止并汇报。不得自动进入下一工作包，也不得将所有工作包合并为一次实现。报告必须说明修改文件、架构偏离、构建/测试证据、
遗留问题和工作包 DoD。

## 36. 完成定义（DoD）

只有在明确标识的已测试版本上证明以下事项，v0.5 实现才算完成：

- Marine extension v2、Task v3 和 Artifact v3 明确且为唯一受支持版本；废弃 Marine schema 及旧版无角色/缺身份产物均被拒绝，
  不执行运行时迁移或猜测默认值。
- C/N/O 语义分离；T 支持所需的区域集；目标生成和修补不要求覆盖整个 N；每个规范路径段都受导航约束。
- H/P/E 不变量及保守区域包含关系成立；绝不放宽硬执行安全；优选层级回退可见且不修改任务。
- 至多存在一条连续硬安全规范路径；诊断候选/覆盖图层在结构和数据上均与之分离，且不存在 MAVLink 编码路径。
- Auto 选择适用的最简单策略，记录确定性的请求/解析来源，并依据能力/拓扑升级；所有生产策略共用覆盖质量契约。
- 已实施并测试获批的 Standard 校准、Strict 数值完整性、统一目标边界处理、空/退化核心防护和残余几何。
- 不存在无条件边界支持；目标相对修补按条件启动，增量执行，安全、确定且考虑路由；策略通过后立即停止，组装后重新完整验证。
- 候选优先级保持硬安全和覆盖优先于优选间距；硬安全但覆盖不足的路径以 Success / ReviewRequired 保留，显示残余并阻止上传。
- D0–D3 与就绪状态一致；DiagnosticOnly、ReviewRequired、过期和无效结果不得上传；MissionAdapter 执行后端门禁并保持纯转换职责。
- 匹配的产物可准确恢复且不重规划；指纹检测每项规划关键输入或语义变化；过期产物转为 Unplanned 且不自动重规划。
- QML 展示 C/N/O、航段用途/安全、就绪状态、来源、覆盖/残余、诊断、建议、修补效果及进入路线限制，但不执行规划逻辑。
- M00–M09、相关 P0/P1 算法和 QGC 回归、v0.5 持久化/Mission 集成、v0.5 SITL 和人工验收均通过；不得用历史 PASS 替代新证据。
- 按计划隔离的上下文、规划器纯度、Task/Plan/Mission 分层、角度约定、航点转换、固定版本的 Marine 专属几何依赖以及最小化
  QGC 核心改动均保持有效。
- 不引入范围外功能。没有各自要求的证据时，不得宣称 P1/P2 外场前置工作完成；软件完成与外场验收必须分别认定。

仅编写规范不能满足实现 DoD、认证车辆动力学/进入路线或关闭外场门槛。获批的 v0.5 实现范围及其必需门槛完成后，必须停止；
不得增加新抽象，也不得自动推进到后续阶段。

## 37. 待校准决策

在 V05-00B 阶段，恰有两个与架构无关的 Standard 策略常量尚未确定：

| 决策 | 策略常量 | 状态及影响 |
| --- | --- | --- |
| CAL-01 | StandardCoveragePolicy.minimumCoverageRatio | 待责任方校准；不指定规范数值。 |
| CAL-02 | StandardCoveragePolicy.boundaryToleranceM | 待责任方校准；不指定规范数值。 |

本修订已冻结策略结构、安全边界、结果/就绪语义和工作包依赖。两个值获批前，不得开始实现 Standard 数值门槛。批准时必须建立供
评估、持久化、测试夹具和过期检测使用的策略语义身份。候选工程值不是批准的默认值。本规范没有新增其他未决架构问题。
