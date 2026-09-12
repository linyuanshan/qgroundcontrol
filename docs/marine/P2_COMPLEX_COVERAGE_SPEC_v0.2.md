# P2_COMPLEX_COVERAGE_SPEC_v0.2

**状态：Frozen Design Authority**  
**阶段：P2 — Complex Coverage Planning V1**  
**前置条件：P1 Engineering Freeze 已批准并合入 `marine/main`；P1 Real-USV Field Validation 延后但不得取消**  
**设计原则：复杂区域覆盖能力优先；保持 P1 架构稳定；不提前建设完整自主导航或通用算法框架。**

---

# 1. P2 目标

P2 的核心目标是：

> 在 P1 单调简单区域 LawnMower Coverage 能力基础上，解决静态复杂水域，尤其是“简单外边界 + 有限内部 No-Go”场景下的区域分解、单元覆盖、单元排序和安全离线连接问题，最终生成完整、连续、可执行的 Coverage Mission。

P2 主链路：

```text
WorkRegion + No-Go
        ↓
Strict Input Validation
        ↓
CoverageTarget
+
TrackFeasibleRegion
        ↓
Restricted BCD
        ↓
P1-compatible Coverage Cells
        ↓
Monotone Coverage Primitive
        ↓
Forward / Reverse Cell States
        ↓
Greedy Oriented-Cell Ordering
        ↓
Visibility Graph + Dijkstra
        ↓
Canonical Path + Leg Roles
        ↓
Nominal Coverage Validation
        ↓
PlanningResult
        ↓
MissionAdapter
        ↓
ArduRover Mission
```

P2 首要解决：

```text
复杂静态区域
→ 可以正确分解
→ 所有 Coverage Cells 均可覆盖
→ 单元之间存在静态安全连接
→ 最终路径完整且连续
→ 名义 Coverage 完整
```

P2 不追求：

```text
动态避障
在线重规划
真实艇体运动学安全
全局路径最优
多机器人协同
真实传感器物理成像建模
```

---

# 2. P2 首要真实场景

P2 第一优先场景：

> **简单外边界 + 一个或多个内部静态 No-Go。**

典型：

```text
┌─────────────────────────┐
│                         │
│       ┌────────┐        │
│       │ No-Go  │        │
│       └────────┘        │
│                         │
└─────────────────────────┘
```

必须支持：

```text
Convex outer + No-Go
Concave outer
Strongly concave outer
Concave outer + No-Go
```

---

# 3. 输入拓扑范围

P2 v0.2 只处理受控 polygon topology。

## 3.1 Outer Boundary

必须：

```text
单个 polygon
simple
无自交
finite
non-degenerate
至少 3 个 distinct vertices
```

---

## 3.2 No-Go

允许：

```text
0..N 个 simple polygon
```

每一个必须：

```text
simple
non-degenerate
完全位于 outer boundary 内部
不接触 outer boundary
不与其它 No-Go 相交
不与其它 No-Go 接触
```

不支持：

```text
nested No-Go
island inside hole
touching topology
self-intersection
partial outside
```

这些均：

```text
InvalidInput
```

---

# 4. 不做输入自动修复

P2 禁止：

```text
auto clip
auto snap
auto merge invalid polygons
auto topology repair
auto remove self-intersection
```

非法输入：

> 明确拒绝并给出具体原因。

但：

> No-Go 在 safety inflation 后发生重叠或合并属于正常几何结果，不属于输入错误。

---

# 5. No-Go 统一语义

P2 定义：

```text
NoGoRegion
=
Static Polygonal Forbidden Region
```

表示：

> USV 中心规划轨迹禁止进入。

P2 不区分：

```text
礁石
网箱
浮标
设施
施工区
岸边障碍
```

不增加：

```text
type
priority
dynamic state
expiry
```

同时：

> No-Go ≠ No-Scan Region。

传感器遮挡、视线阻挡不属于 P2。

---

# 6. CoverageTarget

设：

```text
W = WorkRegion.outerBoundary
N = Union(NoGoRegions)
```

定义：

```text
CoverageTarget = W - N
```

即：

> WorkRegion 内除 No-Go 本体之外，都是要求进行名义 Coverage 的区域。

因此：

```text
No-Go 本体
→ 不要求覆盖

No-Go 周围 safety band
→ 仍属于 CoverageTarget
```

---

# 7. TrackFeasibleRegion

设：

```text
s = safetyMarginM
```

定义：

```text
TrackFeasibleRegion
=
Inset(W, s)
-
Union(Inflate(NoGoRegions, s))
```

含义：

> USV 中心规划轨迹允许存在的区域。

必须严格区分：

```text
CoverageTarget
!=
TrackFeasibleRegion
```

也就是：

```text
CoverageTarget
= 哪里需要巡检

TrackFeasibleRegion
= 船中心允许在哪里走
```

---

# 8. Safety Margin

继续沿用 P1：

> `safetyMarginM` 是规划中心线到 WorkRegion 外边界以及 No-Go 边界的最小几何距离。

P2 不引入：

```text
boat width
boat length
vehicle footprint
turning envelope
dynamic safety margin
```

所以 P2 仍然只保证：

> planned centerline polyline 的静态几何安全。

---

# 9. Safety / Swath 基础约束

设：

```text
w = swathWidthM
```

必须满足：

```text
safetyMarginM <= swathWidthM / 2
```

否则：

```text
CoverageImpossibleWithSafetyMargin
```

注意：

> 这只是必要条件，不是复杂区域 Coverage 完整的充分条件。

---

# 10. Coverage Reachability Precheck

规划早期可以做轻量几何预检查：

```text
ReachableCoverageRegion
=
Buffer(
    TrackFeasibleRegion,
    swathWidthM / 2
)
```

若：

```text
CoverageTarget
⊄
ReachableCoverageRegion
```

则无需继续 BCD，直接：

```text
CoverageImpossibleWithSafetyMargin
```

该检查仅表示：

> 几何上存在允许中心线到达、且 nominal swath 有可能覆盖目标区域的位置。

不表示最终 Coverage Path 一定完整。

该能力不要求单独建设 subsystem。

---

# 11. TrackFeasibleRegion 连通性

如果：

```text
TrackFeasibleRegion = empty
```

返回：

```text
NoNavigableArea
```

如果安全处理后得到多个互不连通 component：

```text
DisconnectedFeasibleRegion
```

P2 不：

```text
自动降低 safety margin
跳过某个 component
PartialSuccess
```

---

# 12. P2 Planner

P2 新增 Planner：

```text
marine.coverage.bcd
```

建议：

```cpp
class BoustrophedonCoveragePlanner final
    : public ICoveragePlanner
{
public:
    std::string id() const override;
    std::string displayName() const override;

    CoveragePlanningSolution plan(
        const CoveragePlanningProblem& problem) const override;
};
```

P1：

```text
marine.coverage.lawnmower
```

继续独立存在。

---

# 13. 算法插件边界

现有：

```text
PlannerRegistry
```

已经是整体 Planner 插件边界。

因此 P2 不提前引入：

```text
ICoverageDecomposer
DecomposerRegistry

IStaticRouter
RouterRegistry

ICellOrderer
OrdererRegistry
```

等额外抽象。

当前采用：

```text
plain functions
small classes
pure data structures
```

即可。

未来真正出现第二种实现后再抽象 interface。

---

# 14. CoveragePlanningProblem

继续复用 P1：

```cpp
struct CoveragePlanningProblem
{
    Region2D region;

    double swathWidthM;
    double safetyMarginM;

    SweepAngleMode sweepAngleMode;
    double requestedSweepAngleDeg;
};
```

P2 完整使用：

```text
region.outerBoundary
region.noGoRegions
```

不增加：

```text
Vehicle*
MissionItem
QGeoCoordinate
sensor runtime state
dynamic obstacles
```

---

# 15. PolygonWithHoles2D

P2 Geometry 内部允许增加：

```cpp
struct PolygonWithHoles2D
{
    Polygon2D outerBoundary;
    std::vector<Polygon2D> holes;
};
```

用于：

```text
CoverageTarget
TrackFeasibleRegion
```

这是派生几何模型。

不改变：

```text
MarineTask.WorkRegion
```

的数据定义。

---

# 16. Global Sweep Direction

P2 使用：

> **一个全局 sweep direction。**

整个：

```text
BCD
Cell Coverage
```

共享该方向。

P2 不做：

```text
per-cell independent sweep
multi-direction optimization
```

---

# 17. Manual Sweep

Manual 使用 P1 已冻结的导航角语义：

```text
0°   = North
90°  = East
clockwise positive
```

归一化：

```text
[0°, 180°)
```

复杂区域不再要求整体 monotone。

Restricted BCD 负责拆分。

---

# 18. Auto Sweep

P2 v0.2 不做“完整 BCD 后对多个角度逐一全局规划”。

Auto 候选来自：

```text
outer-boundary edge orientations
```

简单估计：

```text
perpendicular span
estimated lane count
estimated turns
```

然后：

```text
优先更少预计 turns
→ 更短 geometric span
→ angle deterministic tie-break
```

选出：

```text
selectedGlobalSweepAngle
```

之后只进行一次正式 decomposition。

---

# 19. Restricted BCD

P2 使用：

> **Restricted Boustrophedon Cellular Decomposition**

目标：

```text
TrackFeasibleRegion
        ↓
固定 global sweep
        ↓
分解为若干：
simple
connected
hole-free
sweep-monotone
Coverage Cells
```

P2 不建设通用 computational topology framework。

---

# 20. BCD 精确实现策略

P2 v0.2 正式冻结为：

> **Event-driven Slab BCD**

禁止使用固定空间步长：

```text
y += 0.1m
y += resolution
```

等 raster-like sweep。

---

# 21. Sweep Frame

先将：

```text
TrackFeasibleRegion
```

旋转到 sweep-aligned frame：

```text
Coverage lanes = +X
Sweep progression = +Y
```

之后 decomposition 使用：

```text
y = constant
```

扫描线。

---

# 22. Event Levels

收集：

```text
outer-boundary vertices
hole vertices
```

的所有：

```text
Y coordinates
```

得到：

```text
y0, y1, ..., yn
```

执行：

```text
sort
+
epsilon merge
```

形成 deterministic event levels。

---

# 23. Slab

每两个相邻 event levels：

```text
(yi, yi+1)
```

构成一个 slab。

选择：

```text
ymid
=
(yi + yi+1) / 2
```

作为 probe scanline。

计算：

```text
TrackFeasibleRegion
∩
y = ymid
```

得到该 slab 内的 free-space intervals。

---

# 24. BCD Topology Change

比较前后相邻 slab interval connectivity。

典型：

```text
1 → 2
```

表示：

```text
split
```

典型：

```text
2 → 1
```

表示：

```text
merge
```

另外：

```text
0 → 1
1 → 0
```

表示 free-space component 的出现和消失。

---

# 25. Cell Reconstruction

P2 不手写复杂 polygon boundary tracing。

推荐利用当前 polygon boolean backend：

```text
slab
∩
TrackFeasibleRegion
```

得到 slab polygon pieces。

根据：

```text
cell ownership / connectivity
```

将属于同一 cell 的 pieces 合并。

最终形成：

```text
CoverageCell.polygon
```

这样避免自行处理大量：

```text
critical vertex
ray extension
polygon splitting
boundary tracing
```

细节。

---

# 26. CoverageCell

建议：

```cpp
using CoverageCellId = std::uint32_t;

struct CoverageCell
{
    CoverageCellId id;
    Polygon2D polygon;
};
```

ID 必须 deterministic。

推荐：

```text
按 sweep progression
+
创建顺序
```

分配。

---

# 27. CoverageCell Invariants

每个 cell：

```text
finite
simple
non-degenerate
connected
hole-free
sweep-monotone
```

并且：

```text
Interior(Cell_i)
∩
Interior(Cell_j)
=
empty
```

同时：

```text
Union(All Cells)
≈
TrackFeasibleRegion
```

允许共享 measure-zero boundary。

---

# 28. Decomposition Result

P2 不建立独立大型 CellGraph subsystem。

建议：

```cpp
struct CellAdjacency
{
    CoverageCellId first;
    CoverageCellId second;
};

struct CoverageDecompositionResult
{
    std::vector<CoverageCell> cells;
    std::vector<CellAdjacency> adjacency;
};
```

adjacency 在 split / merge 时同步产生。

---

# 29. Cell Adjacency 的用途

仅用于：

```text
decomposition validation
topology diagnostics
debug visualization
future algorithms
```

P2 v0.2：

> 不使用 CellAdjacency 替代 StaticSafeRouter。

Cell adjacency：

```text
描述 coverage topology
```

Static Router：

```text
解决真实 transit geometry
```

两者严格分离。

---

# 30. P1 Coverage 的正确复用方式

P2 **不得**直接对 CoverageCell 调用完整：

```text
LawnmowerCoveragePlanner::plan()
```

原因：

P1 Planner 会再次：

```text
validate
safety inset
auto/manual angle selection
```

造成：

```text
double safety inset
重复 angle decision
```

---

# 31. Monotone Coverage Primitive

P2 应从 P1 已验证算法中提取一个最小纯规划 primitive。

概念接口：

```cpp
MonotoneCoverageResult generateMonotoneCoverage(
    const Polygon2D& polygon,
    double swathWidthM,
    double sweepAngleDeg);
```

职责：

```text
输入：
已合法、无需再次 safety inset 的 monotone polygon
固定 sweep angle
fixed swath

输出：
coverage path
lane structure
metrics
```

它不知道：

```text
MarineTask
No-Go
Safety Margin
BCD
Auto Angle
Mission
QGC
```

---

# 32. P1 / P2 对 Primitive 的关系

P1：

```text
CoveragePlanningProblem
→ validation
→ safety inset
→ angle selection
→ MonotoneCoveragePrimitive
```

P2：

```text
CoveragePlanningProblem
→ free-space
→ BCD
→ CoverageCell
→ MonotoneCoveragePrimitive
```

这样真正复用：

> P1 已验证的单调区域 Coverage 算法。

而不是复制实现。

---

# 33. Cell Coverage 不负责 Global Coverage Completeness

Cell Coverage Primitive 只保证：

```text
中心线在该 cell 内合法
lane spacing 满足 swath
内部 path 连续
```

不负责证明：

```text
CoverageTarget
```

整体完整覆盖。

因为：

```text
CoverageCell
⊂
TrackFeasibleRegion
```

而：

```text
CoverageTarget
```

还包括 safety band。

最终完整覆盖由：

```text
Global Coverage Validator
```

统一判断。

---

# 34. Path 数据模型演进原则

P2 不把：

```text
segments[]
```

改成新的唯一坐标数据源。

继续保持：

```cpp
std::vector<Point2D> path;
```

作为：

> **Canonical Path**

避免 P1 中：

```text
ComplexItem
MissionAdapter
Persistence
MapVisual
```

全部发生大规模联动。

---

# 35. PathLegRole

P2 增加：

```cpp
enum class PathLegRole
{
    Coverage,
    Transit,
};
```

对于：

```text
path[0]
path[1]
...
path[N-1]
```

定义：

```text
legRoles.size()
=
path.size() - 1
```

其中：

```text
legRoles[i]
```

表示：

```text
path[i]
→
path[i+1]
```

的规划语义。

---

# 36. Coverage Leg

表示：

> 该路径 leg 的主要任务语义是执行名义覆盖。

例如：

```text
lawnmower sweep line
```

Coverage Completeness 只统计：

```text
Coverage leg
```

---

# 37. Transit Leg

表示：

> 该路径 leg 仅用于安全移动。

包括：

```text
同一 Cell 中 lane connector
不同 Cell 之间 static safe route
```

Transit 不计入：

```text
Coverage completeness
```

---

# 38. 为什么使用 Path + LegRoles

相比：

```text
PathSegment {
  role
  points[]
}
```

P2 v0.2 采用：

```text
canonical path
+
legRoles
```

优点：

```text
坐标只有一个 truth
MissionAdapter 可继续直接使用 path
P1 ComplexItem 变化小
Persistence 简单
QML 容易映射
Coverage validation 明确
未来 sensor semantics 仍可使用
```

---

# 39. PlanningSolution 演进

建议：

```cpp
struct CoveragePlanningSolution
{
    PlanningStatus status;

    std::vector<Point2D> path;
    std::vector<PathLegRole> legRoles;

    double coverageLengthM;
    double transitLengthM;
    double pathLengthM;

    double selectedSweepAngleDeg;

    int cellCount;
    int turnCount;

    CoveragePlanningError error;
    std::string message;
};
```

必须：

```text
path.empty()
→ legRoles.empty()

otherwise
legRoles.size() == path.size() - 1
```

---

# 40. PlanningResult 演进

继续保留：

```cpp
std::vector<GeoPoint> path;
```

增加：

```cpp
std::vector<PathLegRole> legRoles;
```

不增加：

```text
GeoPathSegment
```

MissionAdapter 仍只依赖：

```text
PlanningResult.path
```

---

# 41. MissionAdapter

P2 不改变基本路径：

```text
PlanningResult.path
        ↓
ArduPilotMissionAdapter
        ↓
MAV_CMD_NAV_WAYPOINT
```

MissionAdapter 暂时忽略：

```text
legRoles
```

未来如果增加 sensor execution commands，再基于 `legRoles` 扩展。

---

# 42. Persistence

P2 ComplexItem planning artifact 建议：

```text
version 2
```

保留：

```text
generatedPath
```

新增：

```text
legRoles
coverageLengthM
transitLengthM
cellCount
```

不重复保存分段坐标。

---

# 43. P1 Plan Backward Compatibility

加载 P1 version 1：

```text
generatedPath
```

时：

> 保留原始 path。

对于缺失：

```text
legRoles
```

不自动重新规划。

可以：

```text
legRoles = Unknown/Legacy
```

或根据兼容策略整体映射为：

```text
Coverage
```

但不得改变 Mission waypoint 顺序。

---

# 44. 不保存 Planner 临时内部结构

P2 不强制保存：

```text
Coverage Cells
BCD slabs
Cell adjacency
Visibility Graph
Dijkstra predecessor map
```

这些属于：

```text
transient planning internals
```

保存：

```text
最终 path
legRoles
metrics
planning status
```

即可。

---

# 45. Cell Traversal State

每个 CoverageCell 生成：

```text
Forward
Reverse
```

两种 traversal。

建议：

```cpp
struct CellTraversalState
{
    CoverageCellId cellId;
    CellTraversalDirection direction;

    Point2D entry;
    Point2D exit;

    std::vector<Point2D> path;
    std::vector<PathLegRole> legRoles;

    double coverageLengthM;
};
```

---

# 46. Reverse Traversal

Reverse 通过：

```text
path reverse
+
legRoles reverse
```

得到。

不重新规划。

保证：

```text
same coverage geometry
same length
entry / exit exchanged
```

---

# 47. First Cell Selection

P2 v0.2 不把实时 Vehicle Position 放入：

```text
CoveragePlanningProblem
```

第一 oriented cell state 使用 deterministic rule。

例如：

```text
sweep progression coordinate
→ entry sweep coordinate
→ cellId
→ Forward before Reverse
```

---

# 48. First Waypoint 前的安全边界

P2 v0.2 明确：

> **Static-safety guarantee 从第一个 planned waypoint 开始。**

当前：

```text
current USV position
→ first planned entry
```

这一段：

> 不属于 P2 StaticSafeRouter 保证范围。

因此 P2 SITL / 水试要求：

```text
USV 初始位置位于 first entry 附近
```

或者人工安全驶入任务起始区域。

---

# 49. Future Start / End Anchor

未来可扩展：

```text
missionStartAnchor
missionEndAnchor
launch/recovery transit
```

然后：

```text
StartAnchor
→ StaticSafeRouter
→ First Coverage Entry
```

但这不进入 P2 Freeze。

---

# 50. Greedy Oriented-Cell Ordering

当前 exit：

```text
currentExit
```

枚举：

```text
所有未访问 Cell
×
Forward / Reverse
```

计算：

```text
StaticSafeRouter(
    currentExit,
    candidate.entry
)
```

排序：

```text
1. feasible
2. shortest transit length
3. lower cellId
4. Forward before Reverse
```

---

# 51. 不做 Global TSP

P2 禁止：

```text
TSP
GTSP
MILP
global DP
genetic algorithm
global orientation optimization
```

后续如果需要先进优化算法，可直接新增一个新的：

```text
ICoveragePlanner implementation
```

或在出现第二种排序算法后再抽象 Orderer。

---

# 52. StaticSafeRouter

P2 使用：

> **Visibility Graph + Dijkstra**

只解决：

```text
static polygon workspace
```

中的离线 transit。

---

# 53. Visibility Graph

节点：

```text
TrackFeasibleRegion outer vertices
TrackFeasibleRegion hole vertices
Start
Goal
```

若两个节点之间：

```text
whole segment
⊂
TrackFeasibleRegion
```

则建立 edge。

Edge cost：

```text
Euclidean length
```

---

# 54. Visibility Graph 生命周期

每次 Planner invocation：

```text
TrackFeasibleRegion
→ build base graph once
```

route query 时：

```text
临时加入 Start / Goal
```

不建立：

```text
global router service
persistent map cache
navigation database
```

---

# 55. Dijkstra

P2 v0.2 使用 Dijkstra：

```text
small graph
deterministic
simple
easy to test
```

未来：

```text
Dijkstra
→ A*
```

不需要修改：

```text
Coverage Problem
PlannerRegistry
QGC integration
```

---

# 56. 不使用 Grid A*

P2 不进行：

```text
polygon
→ occupancy grid
→ A*
```

原因：

```text
引入 resolution
产生离散误差
窄通道行为依赖 resolution
破坏 polygon-native planning
```

Grid / costmap 属于：

```text
Companion Computer / ROS 2
```

---

# 57. 不使用 NavMesh

P2 不新增：

```text
triangulation
NavMesh
portal
funnel
```

因为 P2 已有：

```text
BCD
```

再维护第二套空间分解没有必要。

---

# 58. Static Route

建议：

```cpp
struct StaticRoute
{
    bool feasible;
    std::vector<Point2D> path;
    double lengthM;
};
```

成功：

```text
path.front = start
path.back = goal
```

所有 leg：

```text
inside TrackFeasibleRegion
```

---

# 59. Final Path Assembly

最终：

```text
Cell Coverage Path
→ inter-cell Transit Route
→ Cell Coverage Path
→ ...
```

组装成：

```text
Canonical Path
+
LegRoles
```

要求：

```text
path continuous
no zero-length duplicate leg
legRoles.size == path.size - 1
```

---

# 60. Path Leg 归属

Cell 内：

```text
sweep lane
→ Coverage

lane connector
→ Transit
```

Cell 间：

```text
Visibility Graph route
→ Transit
```

这是 P2 固定语义。

---

# 61. Nominal Coverage Footprint

P2 v0.2 正式定义：

设：

```text
r = swathWidthM / 2
```

所有：

```text
role == Coverage
```

的 path leg 构成：

```text
Pcoverage
```

定义：

```text
CoveredRegion
=
Pcoverage
⊕
Disk(r)
```

即：

> Coverage centerline 与半径 `swath/2` 圆盘的二维 Minkowski Sum。

工程实现等价于：

```text
round-cap symmetric buffer
```

---

# 62. Footprint 只是 Nominal Model

该模型不表示：

```text
真实 Camera FOV
真实声呐波束
姿态变化
水深变化
遮挡
视场畸变
动态 swath
```

这些属于以后：

```text
sensor-aware coverage model
```

---

# 63. No-Go Safety Band Coverage

Coverage centerline：

```text
不得进入 inflated No-Go
```

但 Coverage Footprint：

> 可以延伸进入 safety band。

这正是：

```text
CoverageTarget
!=
TrackFeasibleRegion
```

的意义。

进入：

```text
No-Go 本体
```

的 Coverage buffer 部分：

> 在 Coverage completeness 计算中不产生额外价值，因为 No-Go 本体不属于 CoverageTarget。

---

# 64. Final Coverage Completeness

定义：

```text
UncoveredRegion
=
CoverageTarget
-
CoveredRegion
```

如果：

```text
Area(UncoveredRegion)
<= CoverageAreaTolerance
```

则：

```text
CoverageComplete
```

否则：

```text
CoverageIncomplete
```

---

# 65. CoverageAreaTolerance

采用统一全局值。

建议初始：

```text
max(
    0.01 m²,
    1e-6 × Area(CoverageTarget)
)
```

具体数值允许在 P2-00/P2-02 通过实际 polygon backend 测试微调。

禁止各模块定义不同 tolerance。

---

# 66. Coverage Failure

如果最终：

```text
CoverageIncomplete
```

P2 直接失败。

不自动：

```text
加 boundary pass
局部补线
改变 swath
降低 safety
修改 sweep angle
```

这些只在实际 P2 水试证明必要后，进入 P2.x/P3。

---

# 67. No PartialSuccess

以下任一情况：

```text
某 Cell 无法覆盖
某 Cell 不可达
Static route 不存在
Coverage incomplete
```

都：

```text
PlanningStatus::Failed
```

不允许：

```text
skip cell
partial mission
partial success
```

---

# 68. CoveragePlanningError

建议增加：

```cpp
enum class CoveragePlanningError
{
    None,

    InvalidOuterBoundary,
    InvalidNoGoRegion,
    NoGoOutsideBoundary,
    NoGoBoundaryConflict,
    NoGoOverlapOrTouch,

    InvalidSwathWidth,
    InvalidSafetyMargin,
    InvalidSweepAngle,

    NoNavigableArea,
    DisconnectedFeasibleRegion,
    CoverageImpossibleWithSafetyMargin,

    DecompositionFailed,
    InvalidCoverageCell,
    CellCoverageFailed,

    SafeTransitNotFound,

    CoverageIncomplete,
    InvalidGeneratedPath,

    GeometryFailure,

    UnsupportedNoGoRegion,
    NonMonotoneSweep,
    UnsafeConnector,
};
```

后面三个继续供 P1 Planner 使用。

---

# 69. Error → Status

输入错误：

```text
InvalidOuterBoundary
InvalidNoGoRegion
NoGoOutsideBoundary
NoGoBoundaryConflict
NoGoOverlapOrTouch
InvalidSwathWidth
InvalidSafetyMargin
InvalidSweepAngle
```

映射：

```text
InvalidInput
```

其它：

```text
Failed
```

---

# 70. Failure Message

必须可操作。

例如：

```text
No-Go region 2 intersects No-Go region 3.

Safety inflation disconnects the feasible region.

Coverage cell 4 cannot be connected safely.

The requested swath width and safety margin
cannot nominally cover the complete target region.

The generated coverage path leaves uncovered area.
```

禁止仅：

```text
Planning failed.
```

---

# 71. No-Go UI

P2 正式增加：

```text
Add No-Go
Select No-Go
Edit vertices
Delete No-Go
```

优先复用：

```text
QGCMapPolygon
QGCMapPolygonVisuals
```

MarineTask：

> 始终保持数据真相。

---

# 72. UI 不自动修复 Geometry

UI 负责绘制。

Planner/validation 负责判断：

```text
No-Go outside
No-Go touching outer
No-Go overlap
No-Go self-intersection
```

UI 不修改用户 polygon。

---

# 73. Map Visual

P2 至少显示：

```text
WorkRegion
No-Go
Coverage legs
Transit legs
```

Coverage / Transit：

> 必须视觉上可区分。

可选 debug：

```text
BCD cells
Cell IDs
Visibility Graph
```

不作为正式 UI 必选。

---

# 74. Plan Invalidation

以下修改：

```text
outer boundary
No-Go add/edit/delete
swath
safety
sweep mode
sweep angle
planner ID
```

必须：

```text
Planned
→
Unplanned
```

同时清空 planning artifact。

---

# 75. P2 Planner Internal Pipeline

正式顺序：

```text
1. Validate Input
2. Normalize Sweep
3. Build CoverageTarget
4. Build TrackFeasibleRegion
5. Validate Connectivity
6. Coverage Reachability Precheck
7. Restricted Event-driven Slab BCD
8. Validate Decomposition
9. Generate Cell Coverage
10. Build Forward / Reverse States
11. Build Base Visibility Graph
12. Greedy Oriented-Cell Ordering
13. Compute Inter-cell Transit
14. Assemble Canonical Path + LegRoles
15. Validate Path Safety
16. Nominal Coverage Completeness
17. Return Solution
```

---

# 76. P2 优化层级

冻结：

```text
1. Coverage complete
2. Static safety
3. Avoid unnecessary decomposition
4. Reduce transit distance
5. Reduce coverage path length / turns
```

前三项不可为后两项让步。

---

# 77. 不建设统一 Cost Framework

P2 不增加：

```text
weighted cost manager
objective plugin system
multi-objective framework
```

采用 explicit lexicographic decision。

---

# 78. Future Algorithm Extension

P2 需要预留未来先进算法，但不通过提前建设抽象框架实现。

关键稳定边界：

```text
CoveragePlanningProblem
        ↓
ICoveragePlanner
        ↓
CoveragePlanningSolution
```

以及内部纯数据边界：

```text
TrackFeasibleRegion
→ Decomposition Result

CoverageCell
→ Monotone Coverage Result

Free Space + Start + Goal
→ StaticRoute

Traversal States
→ Ordered Visit Sequence
```

---

# 79. 未来可替换算法

以后可以增加：

```text
Decomposition:
    Optimized BCD
    Morse decomposition
    Trapezoidal decomposition

Routing:
    A*
    Theta*
    turn-aware routing
    Dubins / Hybrid A*

Ordering:
    GTSP
    MILP
    OR-Tools
    RL

Coverage:
    energy-aware
    current-aware
    sensor-aware
    learning-based
```

但当前不引入相应 Registry。

---

# 80. P2 不解决动态障碍

继续冻结：

```text
QGC
→ global/static/offline planning

Companion Computer / ROS 2
→ dynamic avoidance
→ local replanning
```

P2 不增加：

```text
moving obstacles
AIS target
sonar obstacle
camera obstacle
online map
```

---

# 81. Four Representative Cases

## P2-A

```text
L-shaped concave region
no No-Go
```

验证：

```text
BCD
multi-cell
P1 primitive reuse
continuous path
```

## P2-B

```text
C-shaped / strongly concave region
```

验证：

```text
multiple events
cell adjacency
ordering
safe transit
```

## P2-C

```text
Convex outer
+
one No-Go
```

第一优先真实场景。

验证：

```text
No-Go inflation
split/merge
coverage around No-Go
static routing
coverage completeness
```

## P2-D

```text
Concave outer
+
one No-Go
```

综合测试。

---

# 82. 测试矩阵

| ID | 场景 | 预期 |
|---|---|---|
| T01 | Convex, no No-Go | P1-equivalent Success |
| T02 | L-shaped concave | Success |
| T03 | C-shaped concave | Success |
| T04 | Convex + one No-Go | Success |
| T05 | Concave + one No-Go | Success |
| T06 | Two independent No-Go | Success |
| T07 | No-Go outside | InvalidInput |
| T08 | No-Go intersects outer | InvalidInput |
| T09 | No-Go touches outer | InvalidInput |
| T10 | No-Go overlap | InvalidInput |
| T11 | No-Go touch | InvalidInput |
| T12 | Self-intersecting No-Go | InvalidInput |
| T13 | Inflation merges No-Go | Valid geometric processing |
| T14 | Inflation blocks corridor | DisconnectedFeasibleRegion |
| T15 | Inflation removes all free space | NoNavigableArea |
| T16 | Safety/swath unreachable target | CoverageImpossibleWithSafetyMargin |
| T17 | BCD cell union | ≈ TrackFeasibleRegion |
| T18 | BCD cells monotone | True |
| T19 | Cell interiors non-overlap | True |
| T20 | Cell adjacency deterministic | True |
| T21 | P1 primitive on BCD cell | Success |
| T22 | Cell Forward/Reverse | Same geometry / reverse entry-exit |
| T23 | Direct visibility | Direct route |
| T24 | One No-Go detour | Safe route |
| T25 | Multiple No-Go route | Safe route |
| T26 | No route | SafeTransitNotFound |
| T27 | Dijkstra deterministic | Same path |
| T28 | Greedy ordering deterministic | Same sequence |
| T29 | `path.size=N` | `legRoles.size=N-1` |
| T30 | Coverage/Transit annotation | Correct |
| T31 | Transit excluded from coverage | True |
| T32 | Full nominal footprint | Success |
| T33 | Artificial gap | CoverageIncomplete |
| T34 | P2 save/load | Path + roles restored |
| T35 | P1 v1 load | No replanning |
| T36 | MissionAdapter | Same waypoint semantics |
| T37 | Task edit | Planned → Unplanned |
| T38 | SITL P2-A–D | Mission executable |
| T39 | Real USV P2-C | Complete task |

---

# 83. Decomposition Property Tests

成功 BCD 必须满足：

```text
all cells finite
all cells simple
all cells non-degenerate
all cells connected
all cells hole-free
all cells sweep-monotone
cell interiors non-overlapping
cell union ≈ TrackFeasibleRegion
```

---

# 84. Routing Property Tests

成功 route：

```text
start valid
goal valid
all points finite
all legs inside TrackFeasibleRegion
length finite
length >= direct distance
```

若 Start/Goal 可见：

```text
route = direct segment
```

---

# 85. Final Solution Invariants

Success 必须满足：

```text
path.size >= 2
legRoles.size = path.size - 1
all points finite
all centerline legs safe
all Coverage Cells visited once
no required Cell omitted
all inter-cell transit feasible
CoverageTarget nominally covered
```

---

# 86. Determinism

相同：

```text
WorkRegion
No-Go
swath
safety
sweep settings
```

重复执行必须得到：

```text
same event levels
same cell IDs
same cell geometry
same adjacency
same sweep angle
same visit order
same orientation
same transit routes
same canonical path
same leg roles
```

在统一 floating tolerance 下比较。

---

# 87. Regression Gate

P2 不能破坏：

```text
P1 LawnMower Planner
P1 convex coverage
P1 manual/auto angle
P1 save/load
MissionAdapter
normal QGC Plan
Survey
StructureScan
MissionItem
```

---

# 88. P2 明确禁止项

P2 v0.2 禁止：

```text
dynamic avoidance
online replanning

ROS 2 runtime inside QGC

vehicle footprint
turn radius
Dubins
Hybrid A*
curvature planner

Grid A*
NavMesh

TSP / global optimization
per-cell sweep optimization

automatic topology repair

PartialSuccess

dynamic swath

real camera / sonar footprint
sensor occlusion

ExecutionStateMachine
sensor runtime scheduler

CoverageQualityMap
AI revisit

multi-USV
fleet planning
```

---

# 89. 推荐代码边界

在现有 P1 基础上增量增加：

```text
custom/src/Marine/
├── Geometry/
│   ├── GeometryTypes.*
│   ├── MarineGeometry.*
│   └── PolygonRegion.*
│
├── Planning/
│   ├── ICoveragePlanner.*
│   ├── CoveragePlanningProblem.*
│   ├── LawnmowerCoveragePlanner.*
│   ├── MonotoneCoverage.*
│   ├── BoustrophedonCoveragePlanner.*
│   ├── CoverageDecomposition.*
│   ├── StaticSafeRouter.*
│   └── PathLegRole.*
│
├── Mission/
│   └── ArduPilotMissionAdapter.*
│
└── QGC/
```

不为了 P2：

```text
搬迁 P1 已稳定文件
重构全部 test 目录
```

---

# 90. P2-00 — Readiness Audit

本轮只审计。

必须确认：

```text
P1 Final Freeze HEAD
P1 angle bug 已修
P1 Safety/Coverage bug 已修
P1 test gates
marine/main 状态
latest upstream relationship
geometry backend
No-Go schema
P1 artifact persistence
```

输出：

```text
PASS / CONDITIONAL / BLOCKED
```

不得写 P2 production code。

---

# 91. P2-01 — Path Leg Semantics + P1 Primitive Extraction

实现：

```text
PathLegRole
PlanningSolution.legRoles
PlanningResult.legRoles
```

同时把 P1 `generateCandidate()` 中真正的 monotone coverage 核心提取为：

```text
MonotoneCoverage Primitive
```

P1 Lawnmower 继续调用该 Primitive。

DoD：

```text
P1 behavior unchanged
P1 tests pass
MissionAdapter unchanged
```

---

# 92. P2-02 — No-Go Validation + Free-Space Geometry

实现：

```text
strict No-Go validation
PolygonWithHoles2D
CoverageTarget
TrackFeasibleRegion
connectivity
reachability precheck
```

不做 BCD。

---

# 93. P2-03 — Restricted Event-driven Slab BCD

实现：

```text
sweep-frame transform
vertex event levels
epsilon merge
slab intervals
split/merge ownership
cell reconstruction
cell adjacency
```

不做 ordering/routing。

---

# 94. P2-04 — Decomposition Validation

验证：

```text
cell simple
cell monotone
cell union
cell non-overlap
determinism
adjacency correctness
```

P2-A/P2-B/P2-C 必须通过。

---

# 95. P2-05 — Cell Coverage + Traversal States

对每个 Cell：

```text
MonotoneCoverage Primitive
```

生成 coverage。

然后构建：

```text
Forward
Reverse
```

不得再次 safety inset。

---

# 96. P2-06 — Visibility Graph + Dijkstra

实现：

```text
base visibility graph
temporary start/goal
Dijkstra
StaticRoute
```

基于：

```text
TrackFeasibleRegion
```

不做 A*/NavMesh/Grid。

---

# 97. P2-07 — Greedy Oriented-Cell Ordering

实现：

```text
deterministic first state
nearest feasible oriented state
static route distance
tie-break
```

不做 TSP。

---

# 98. P2-08 — Complex Plan Assembly

组装：

```text
Cell Coverage
+
Inter-cell Transit
```

形成：

```text
canonical path
+
legRoles
```

同时计算：

```text
coverageLength
transitLength
pathLength
cellCount
turnCount
```

---

# 99. P2-09 — Nominal Coverage Completeness

实现：

```text
Coverage legs
→ disk buffer(swath/2)
→ union
→ CoverageTarget difference
```

返回：

```text
Success
or
CoverageIncomplete
```

这是 P2 核心 Freeze Gate。

---

# 100. P2-10 — Planner Integration

注册：

```text
marine.coverage.bcd
```

接入：

```text
PlannerRegistry
CoverageInspectionComplexItem
CoverageTaskAdapter
```

保持：

```text
Task != Plan != Mission
```

---

# 101. P2-11 — No-Go UI

实现：

```text
add
select
edit
delete
display
```

Planner 执行严格 validation。

UI 不自动修复。

---

# 102. P2-12 — Persistence + Mission Integration

实现：

```text
planning artifact v2
generatedPath
legRoles
metrics
```

验证：

```text
P2 save/load
P1 v1 compatibility
load does not replan
MissionAdapter output unchanged
```

---

# 103. P2-13 — Integrated + SITL Validation

完整测试：

```text
P2-A
P2-B
P2-C
P2-D
```

并执行 ArduRover SITL。

重点记录：

```text
upload
AUTO progression
mission completion
actual trajectory
No-Go transit
corner cutting
```

如果真实执行因切角侵入 safety region：

> 记录为未来 turn-aware planning 输入，不临时塞进 P2。

---

# 104. P2-14 — Real USV Validation

第一优先：

```text
Convex outer
+
one static No-Go
```

至少完成：

```text
draw
generate
upload
AUTO
safe No-Go transit
full coverage
trajectory export
```

初始 USV：

> 预置在 first planned entry 附近。

---

# 105. P2-15 — Final Freeze

检查：

```text
P1 regression
Task/Plan/Mission
Planner purity
No-Go semantics
CoverageTarget
TrackFeasibleRegion
Event-driven BCD
Cell invariants
P1 primitive reuse
Visibility Graph
Dijkstra
Greedy ordering
Path + LegRoles
Coverage completeness
Persistence
SITL
field test
scope control
```

通过：

```text
P2 FREEZE APPROVED
```

---

# 106. P2 Definition of Done

P2 最终必须打通：

```text
Draw WorkRegion
        ↓
Add No-Go
        ↓
Set swath / safety / sweep
        ↓
Generate
        ↓
Restricted Event-driven BCD
        ↓
All Cells Covered
        ↓
All Cells Safely Connected
        ↓
Canonical Path + Leg Roles
        ↓
Nominal Coverage Complete
        ↓
Map Display
        ↓
Save / Load
        ↓
MAVLink Mission
        ↓
ArduRover SITL
        ↓
Real USV
```

---

# 107. Success 的正式含义

P2 返回：

```text
Success
```

必须同时表示：

```text
1. Input valid
2. TrackFeasibleRegion exists
3. TrackFeasibleRegion connected
4. CoverageTarget is nominally reachable
5. BCD decomposition valid
6. Every required cell has coverage path
7. Every required cell visited exactly once
8. Every inter-cell transit is statically safe
9. Canonical path continuous
10. Every path leg role valid
11. Every centerline leg lies in TrackFeasibleRegion
12. CoverageTarget passes nominal completeness validation
```

任何一项失败：

```text
Failed
```

---

# 108. P2 产品能力表述

P2 完成后可以准确表述：

> QGroundControl Marine 已支持面向静态复杂水域的区域覆盖巡检规划。系统可处理简单凹边界和有限内部 No-Go，通过受约束、事件驱动的 Boustrophedon Cellular Decomposition 将复杂自由空间分解为单调覆盖单元，复用 P1 单调区域 Coverage 能力，并通过 Visibility Graph + Dijkstra 完成单元间静态安全连接，最终生成带有 Coverage/Transit 路径语义且满足中心线安全和名义 Coverage Completeness 要求的连续 USV Mission。

---

# 109. P2 未来演进

P2 后续不推翻当前结构。

可演进为：

```text
Decomposition:
BCD
→ Optimized BCD
→ Morse / other decomposition

Routing:
Dijkstra
→ A*
→ turn-aware

Ordering:
Greedy
→ GTSP / MILP / OR-Tools / RL

Coverage:
fixed swath
→ sensor-aware
→ current-aware
→ adaptive / learning-based

Mission:
no start anchor
→ start/end anchor
→ launch/recovery transit
```

未来先进算法：

> 通过稳定的 `CoveragePlanningProblem → ICoveragePlanner → CoveragePlanningSolution` 插件边界接入，而不是提前建设复杂 Strategy/Registry Framework。

---

# 110. v0.2 架构冻结候选

P2 v0.2 最终推荐主链路：

```text
MarineTask
        ↓
CoverageTaskAdapter
        ↓
CoveragePlanningProblem
        ↓
Strict Polygon / No-Go Validation
        ↓
CoverageTarget
+
TrackFeasibleRegion
        ↓
Coverage Reachability Precheck
        ↓
Restricted Event-driven Slab BCD
        ↓
CoverageCell + Lightweight Adjacency
        ↓
P1 Monotone Coverage Primitive
        ↓
Forward / Reverse Traversal States
        ↓
Visibility Graph + Dijkstra
        ↓
Greedy Oriented-Cell Ordering
        ↓
Canonical Path + LegRoles
        ↓
Nominal Coverage Validation
        ↓
CoveragePlanningSolution
        ↓
CoverageTaskAdapter
        ↓
PlanningResult
        ↓
MissionAdapter
        ↓
MAVLink Mission
```

本版本作为：

> **P2 v0.2 Frozen Design Authority**

在 P1 Final Freeze 完成后，进入：

```text
P2-00 Implementation Readiness Audit
```

P2-00 通过之前：

> 不开始 P2-01 生产代码。
