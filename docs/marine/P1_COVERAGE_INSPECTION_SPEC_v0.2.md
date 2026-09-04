# P1_COVERAGE_INSPECTION_SPEC_v0.2

**状态**：Frozen for Implementation

**阶段**：P1 — USV Coverage Inspection V1

**设计原则**：可落地闭环优先于复杂区域泛化

**前置条件**：P0 已 Freeze；P1-00 Readiness Audit 已完成；P1-01 前 `marine/main` 必须包含已验证的 upstream 同步基线

---

## 1. P1 目标

P1 的唯一核心目标是：

> 在 QGroundControl Marine 架构上，实现适用于真实 USV 的第一版可执行区域覆盖巡检功能。

P1 必须打通：

```text
自由绘制 Work Region
        ↓
MarineTask
        ↓
任务/能力检查
        ↓
WGS84 → Local ENU
        ↓
Safety Margin
        ↓
Sweep Angle
        ↓
Lawnmower Coverage
        ↓
安全折线路径
        ↓
PlanningResult
        ↓
ArduPilotMissionAdapter
        ↓
MAVLink Mission
        ↓
ArduRover SITL
        ↓
真实 USV 水试
```

P1 优先解决：

```text
能画
→ 能规划
→ 路径安全
→ 能保存
→ 能上传
→ 能执行
→ 能实际跑
```

而不是：

```text
任意复杂区域
→ 全局最优
→ 运动学最优
→ 多机器人
→ AI闭环
```

---

## 2. P1 首要真实场景

P1 首个真实应用场景冻结为：

> **一般凸多边形水域。**

例如：

```text
四边形
不规则五边形
六边形
一般凸边界
```

这是 P1 必须稳定支持并作为最终验收对象的区域类型。

P1 可以自然支持部分：

> sweep-monotone 简单凹多边形。

但：

> 凹多边形不是 P1 Freeze 的主要验收门槛。

---

## 3. P1 支持能力边界

| 区域类型 | P1 |
| --- | --- |
| Rectangle | 必须支持 |
| Arbitrary convex polygon | 必须支持 |
| Sweep-monotone simple concave polygon | 支持 |
| Non-monotone concave polygon | 不保证，P2 |
| Polygon with internal holes | P2 |
| Coverage routing around No-Go | P2 |
| Multiple disconnected free-space cells | P2 |
| BCD / Cell decomposition | P2 |

因此，P1 不宣称：

> “支持任意凹多边形覆盖”。

正式能力表述为：

> **P1 支持一般凸多边形以及满足当前 sweep direction 单调性要求的简单单连通区域。**

---

## 4. UI 与 Planner 能力分离

QGC UI：

> 允许用户自由绘制 polygon。

UI 不主动限制用户：

```text
只能画矩形
只能画凸多边形
只能画 monotone polygon
```

Planner 负责：

```text
Validate
→ Capability Check
→ Plan / Reject
```

如果输入超出 P1 能力范围，应明确提示，例如：

```text
The selected sweep direction is not feasible
for P1 simple coverage planning.
```

或者：

```text
This work region requires complex-area
coverage planning, which is not supported in P1.
```

禁止：

```text
静默产生危险路径
自动忽略部分区域
偷偷切换到另一套未定义算法
```

---

## 5. P1 冻结架构

正式数据流：

```text
MarineTask
    │
    ▼
CoverageTaskAdapter
    │
    ├── task validation
    ├── capability validation
    ├── WGS84 → Local ENU
    └── semantic conversion
    │
    ▼
CoveragePlanningProblem
    │
    ▼
PlannerRegistry
    │
    ├── marine.coverage.mock
    └── marine.coverage.lawnmower
             │
             ▼
CoveragePlanningSolution
             │
             ▼
CoverageTaskAdapter
             │
             └── ENU → WGS84
             ▼
PlanningResult
             │
             ▼
CoverageInspectionComplexItem
             │
             ▼
ArduPilotMissionAdapter
             │
             ▼
MAVLink MissionItem[]
```

继续冻结：

```text
Task != Plan != Mission
```

---

## 6. 强制架构边界

### 6.1 Planner Core 禁止依赖

`ICoveragePlanner` 及具体 Planner 不得依赖：

```text
QObject
QML
Fact
QGeoCoordinate
Vehicle
MissionItem
PlanMasterController
MissionController
```

Planner Core 只能使用：

```text
Marine-owned STL / geometry types
```

---

### 6.2 CoverageInspectionComplexItem

只负责：

```text
taskId
planner invocation
planning state
PlanningResult
QML-facing properties
.plan persistence
MissionAdapter handoff
```

禁止实现：

```text
scanline algorithm
polygon inset
sweep-angle optimization
geometry intersection
coverage algorithm
```

---

### 6.3 MissionAdapter

唯一转换关系继续为：

```text
PlanningResult
        ↓
ArduPilotMissionAdapter
        ↓
MissionItem[]
```

Planner 不得产生 MAVLink 命令。

---

## 7. Task 层数据模型

现有 `MarineTask` 继续保留。

P1 将：

```cpp
struct CoverageConfig {
    double swathWidthM = 0.0;
    double safetyMarginM = 0.0;
};
```

扩展为：

```cpp
enum class SweepAngleMode
{
    Manual,
    Auto,
};

struct CoverageConfig
{
    double swathWidthM = 0.0;
    double safetyMarginM = 0.0;

    SweepAngleMode sweepAngleMode = SweepAngleMode::Auto;
    double sweepAngleDeg = 0.0;
};
```

---

## 8. Sweep Angle 统一定义

Sweep Angle 按导航习惯定义：

```text
0°   = North
90°  = East
180° = North equivalent
```

角度：

```text
clockwise positive
```

内部统一归一化为：

```text
[0°, 180°)
```

因为：

```text
θ
```

与：

```text
θ + 180°
```

表示相同 sweep orientation。

Local ENU 中：

```text
X = East
Y = North
```

---

## 9. CoveragePlanningProblem

P1 正式引入纯规划输入。

推荐接口：

```cpp
struct Point2D
{
    double xM = 0.0;
    double yM = 0.0;
};

struct Polygon2D
{
    std::vector<Point2D> vertices;
};

struct Region2D
{
    Polygon2D outerBoundary;
    std::vector<Polygon2D> noGoRegions;
};

struct CoveragePlanningProblem
{
    Region2D region;

    double swathWidthM = 0.0;
    double safetyMarginM = 0.0;

    SweepAngleMode sweepAngleMode = SweepAngleMode::Auto;
    double requestedSweepAngleDeg = 0.0;
};
```

禁止添加：

```text
taskId
taskName
camera config
sonar config
Vehicle*
MissionItem
QGeoCoordinate
```

---

## 10. ICoveragePlanner

P1 将 P0 的：

```cpp
plan(const MarineTask&)
```

正式演进为：

```cpp
class ICoveragePlanner
{
public:
    virtual ~ICoveragePlanner() = default;

    virtual std::string id() const = 0;
    virtual std::string displayName() const = 0;

    virtual CoveragePlanningSolution plan(
        const CoveragePlanningProblem& problem) const = 0;
};
```

这是 P1 的正式长期 Planner 边界。

P0：

```text
plan(MarineTask)
```

视为架构验证阶段接口，不继续固化。

---

## 11. Planner Registry

继续沿用 P0：

```text
PlannerRegistry
```

Planner ID：

```text
marine.coverage.mock
marine.coverage.lawnmower
```

其中：

```text
marine.coverage.mock
```

保留用于：

```text
unit test
architecture regression
```

正式 P1 Planner：

```text
marine.coverage.lawnmower
```

不建立：

```text
lawnmower-v1
lawnmower-safe
lawnmower-auto
```

等多余 Planner ID。

---

## 12. LocalFrame / GeoReference

P1 必须使用：

```text
WGS84
→ ECEF
→ Local ENU
```

进行规划坐标转换。

建议新增非常薄的：

```cpp
class GeoReference
{
public:
    static std::optional<GeoReference> create(
        const GeoPolygon& region);

    Point2D toLocal(const GeoPoint& point) const;
    GeoPoint toGeo(const Point2D& point) const;
};
```

实际签名可根据现有工程风格调整。

核心要求：

```text
Planner 不知道 WGS84
Planner 只看到 meter
```

不要建立：

```text
CoordinateSystemService
ProjectionManager
GeoFramework
```

等通用框架。

---

## 13. GeoReference 原点

P1 采用工作区域附近局部原点。

推荐：

```text
outerBoundary 所有顶点的平均 latitude / longitude
altitude = 0
```

作为 ENU reference origin。

P1 服务的是局部 USV 作业区域，不针对：

```text
跨城市
跨国家
跨日期变更线
极区
```

等全球规划。

---

## 14. CoverageTaskAdapter

建议新增：

```text
CoverageTaskAdapter
```

职责只有：

```text
MarineTask
→ validate
→ GeoReference
→ CoveragePlanningProblem
```

和：

```text
CoveragePlanningSolution
→ GeoReference
→ PlanningResult
```

概念接口：

```cpp
class CoverageTaskAdapter
{
public:
    static bool buildProblem(
        const MarineTask& task,
        CoveragePlanningProblem& problem,
        GeoReference& geoReference,
        CoveragePlanningError& error);

    static PlanningResult toPlanningResult(
        const CoveragePlanningSolution& solution,
        const GeoReference& geoReference);
};
```

不要求机械使用以上签名，但职责必须保持。

禁止在 Adapter 实现：

```text
scanline
offset
angle selection
lawnmower
path ordering
```

---

## 15. CoveragePlanningSolution

Planner Core 返回：

```cpp
struct CoveragePlanningSolution
{
    PlanningStatus status = PlanningStatus::Failed;

    std::vector<Point2D> path;

    double pathLengthM = 0.0;
    double selectedSweepAngleDeg = 0.0;

    int turnCount = 0;

    CoveragePlanningError error =
        CoveragePlanningError::None;

    std::string message;
};
```

P1 暂时：

> 不引入 Coverage/Transit semantic segments。

所有结果仍作为一个连续 polyline。

这可以显著减少：

```text
Mission persistence
QML
MissionAdapter
sensor execution
```

的连锁改动。

---

## 16. PlanningResult

QGC/Task Domain 继续使用现有：

```cpp
struct PlanningResult
{
    PlanningStatus status;
    std::vector<GeoPoint> path;
    double pathLengthM;
    std::string message;
};
```

P1 建议增加：

```cpp
double selectedSweepAngleDeg;
int turnCount;
```

以支持：

```text
Auto Angle 结果显示
save/load
调试
SITL/水试比较
```

不增加复杂统计数据。

---

## 17. Geometry Backend

P1 正式建议采用：

> **Clipper2**

负责 Marine 层中的：

```text
polygon inset
polygon validity辅助处理
line/polygon clipping
```

约束：

```text
Clipper2 只能位于 custom/Marine
```

不得因为 P1：

```text
修改 QGC Core geometry dependencies
```

建议仅增加非常薄的：

```text
MarineGeometry
```

utilities。

不要建设完整 Geometry Framework。

---

## 18. Input Validation

Planner 开始前必须验证：

### Outer Boundary

必须：

```text
>= 3 distinct points
finite coordinates
valid WGS84 coordinates
non-zero area
non-self-intersecting
```

Local ENU 后再次验证：

```text
finite
non-degenerate
```

---

### Swath

必须：

```text
swathWidthM > 0
finite
```

---

### Safety Margin

必须：

```text
safetyMarginM >= 0
finite
```

---

### Sweep Angle

Manual：

```text
finite
```

然后 normalize：

```text
[0,180)
```

---

## 19. No-Go 在 P1 的最终边界

数据层继续支持：

```text
WorkRegion.noGoRegions
```

QML 继续允许：

```text
draw
display
save
load
```

但是正式 P1 Lawnmower Planner：

> **不承诺内部 No-Go Coverage Routing。**

v0.2 建议采用最清晰规则：

```text
if noGoRegions is not empty:
    return UnsupportedGeometry
```

错误：

```text
Internal no-go coverage routing is reserved for P2.
```

这样：

```text
Task schema
UI
.plan
```

提前稳定。

而：

```text
holes
safe routing
BCD
```

不会偷偷进入 P1。

---

## 20. Safety Margin 定义

P1 正式定义：

> `safetyMarginM` 是 **USV 规划中心线** 与 Work Region 外边界之间要求保持的最小几何距离。

不表示：

```text
船体边缘安全距离
真实轨迹包络安全距离
动态避障安全距离
```

因此 P1 不需要：

```text
boat width
boat footprint
turn radius
velocity-dependent margin
```

这些进入 P2。

---

## 21. Navigable Region

P1：

```text
NavigableRegion
=
Inset(outerBoundary, safetyMarginM)
```

由于 P1 Planner 不支持 No-Go：

```text
不做 no-go subtraction
```

若：

```text
Inset result empty
```

失败。

若：

```text
Inset result > 1 disconnected polygon
```

P1 失败。

错误：

```text
Safety margin makes the work region
unsupported or disconnected.
```

---

## 22. P1 Coverage 的正式含义

P1 使用固定 swath strip 模型。

Planner 必须保证：

```text
adjacent lane spacing <= swathWidthM
```

并合理布置首、尾 lane，避免明显 Coverage Gap。

P1 不在 production planner 中实现：

```text
full footprint union
covered-area boolean union
CoverageQualityMap
```

严格 coverage footprint 校验：

> 可用于算法单元测试，但不是 Planner 运行时必需模块。

---

## 23. Sweep Monotonicity

对于给定 sweep direction：

> 与 sweep direction 垂直的任意扫描线与 NavigableRegion 相交时，最多产生一个连续 interval。

则称：

```text
sweep-monotone
```

P1 应实现明确：

```text
PolygonMonotonicity
```

检查。

推荐采用 polygon 两链 monotonicity 判定，而不是仅随机抽几条 scanline 判断。

凸多边形：

```text
对任意方向均 monotone
```

因此 P1 首要真实场景稳定可用。

---

## 24. Manual Sweep Angle 流程

Manual 模式：

```text
requested angle
        ↓
normalize
        ↓
monotonicity check
        ↓
generate candidate
```

如果：

```text
not monotone
```

直接：

```text
NonMonotoneSweep
```

不进行：

```text
BCD
safe router
automatic angle replacement
```

---

## 25. Auto Sweep Angle 候选

Auto 不做连续角度优化。

候选角度来源：

```text
outerBoundary edge orientations
```

所有方向：

```text
normalize to [0,180)
```

并使用几何容差去重。

对于凸多边形：

```text
所有候选均可行
```

对于简单凹多边形：

```text
先过滤 non-monotone candidates
```

---

## 26. Auto Sweep Angle 评分

冻结优先级：

```text
1. 可行 monotone
2. turnCount 最少
3. pathLengthM 最短
4. sweepAngleDeg 最小
```

第 4 项只用于：

```text
deterministic tie-break
```

Auto 不需要：

```text
PSO
GA
RL
gradient optimization
1° exhaustive search
```

允许对有限 edge-angle candidates 生成轻量 candidate path，用于：

```text
turnCount
pathLength
```

比较。

这不视为全局优化框架。

---

## 27. Scanline 坐标变换

给定 sweep angle `θ`：

将 polygon 转到 sweep-aligned frame。

目标：

```text
Sweep direction = +X
Lane progression = Y
```

之后所有 scanline 为：

```text
y = constant
```

这样可以简化：

```text
intersection
ordering
lane generation
```

完成后再旋转回 Local ENU。

---

## 28. Lane Generation

对于旋转后的 NavigableRegion：

```text
minY
maxY
H = maxY - minY
```

若：

```text
H <= swathWidthM
```

则：

```text
N = 1
```

否则：

```text
N = ceil(H / swathWidthM)
```

实际 spacing：

```text
spacing = H / N
```

保证：

```text
spacing <= swathWidthM
```

lane positions：

```text
minY + spacing/2
minY + 3*spacing/2
...
```

最后一条同样保持边缘对称。

---

## 29. Scanline Intersection

P1 的 scanline intersection 仍必须：

> 正确求出所有 inside intervals。

即使 P1 最终只接受 monotone candidate。

原因是：

```text
interval count
```

本身就是非常重要的：

```text
geometry sanity check
```

规则：

```text
0 interval:
    skip

1 interval:
    valid

>1 intervals:
    candidate unsupported in P1
```

禁止使用：

```text
furthest left intersection
→
furthest right intersection
```

把 polygon 外部错误连接起来。

---

## 30. LawnMower Ordering

有效 lane：

```text
lane 0: left → right
lane 1: right → left
lane 2: left → right
...
```

形成基础 Boustrophedon-style：

```text
←→
→←
←→
```

这里的：

```text
boustrophedon traversal
```

不等于：

```text
Boustrophedon Cellular Decomposition
```

P1 不做后者。

---

## 31. Connector Safety

相邻 lane 之间只允许：

> straight connector。

对于：

```text
lane_i.end
→
lane_i+1.start
```

必须验证整个 line segment：

```text
contained in NavigableRegion
```

如果安全：

```text
accept
```

如果不安全：

```text
candidate invalid
```

禁止：

```text
Visibility Graph
Dijkstra
A*
boundary following
```

这些统一进入 P2。

---

## 32. 为什么这样仍然支持 P1 主场景

对于：

```text
convex polygon
```

任意两个 polygon 内部点之间的直线仍位于 polygon 内。

因此：

> 相邻 lane 的 straight connector 对 P1 主场景天然安全。

这也是把：

```text
general convex polygon
```

作为 P1 真实验收区域的重要原因。

---

## 33. Path Validation

Planner 输出 Success 前必须执行最终 validation：

```text
path.size >= 2
all points finite
all points inside/on NavigableRegion
all segments safe
pathLength finite
pathLength > 0
spacing <= swathWidth
```

若任一失败：

```text
PlanningStatus::Failed
```

不得传入 MissionAdapter。

---

## 34. CoveragePlanningError

建议 P1 引入明确的内部失败类型：

```cpp
enum class CoveragePlanningError
{
    None,

    InvalidOuterBoundary,
    InvalidSwathWidth,
    InvalidSafetyMargin,
    InvalidSweepAngle,

    UnsupportedNoGoRegion,
    SafetyInsetEmpty,
    SafetyInsetDisconnected,

    NonMonotoneSweep,
    UnsafeConnector,

    InvalidGeneratedPath,
    GeometryFailure,
};
```

不要用：

```text
Planning failed
```

覆盖全部错误。

---

## 35. PlanningStatus 映射

现有：

```text
Success
InvalidInput
Failed
```

继续保留。

例如：

```text
InvalidOuterBoundary
InvalidSwathWidth
InvalidSafetyMargin
InvalidSweepAngle
```

映射：

```text
InvalidInput
```

而：

```text
UnsupportedNoGoRegion
NonMonotoneSweep
SafetyInsetDisconnected
UnsafeConnector
```

映射：

```text
Failed
```

具体原因通过：

```text
message
```

返回给 UI。

---

## 36. Task Change → Plan Invalidation

继续冻结 P0 语义。

以下修改必须：

```text
Planned
→
Unplanned
```

并清空旧 path：

```text
outerBoundary
noGoRegions
swathWidthM
safetyMarginM
sweepAngleMode
sweepAngleDeg
plannerId
```

P1 不增加：

```text
input hash
digest
version graph
```

---

## 37. QGC UI

Coverage Inspection Editor P1 最低功能：

```text
Work Region
No-Go Region
Swath Width
Safety Margin

Sweep Angle:
    Auto
    Manual
        angle input

Planner:
    marine.coverage.lawnmower

Generate / Replan

Planning status
Selected sweep angle
Path length
```

---

## 38. Work Region 编辑

优先复用现有 QGC：

```text
map polygon interaction
handles
polygon editing patterns
```

可以复用：

```text
UI interaction
QML controls
visual components
```

不能复用或拷贝：

```text
Survey transect algorithm
camera footprint planning
terrain logic
UAV altitude planning
```

---

## 39. No-Go UI

P1 可以允许绘制 No-Go。

但 UI 应明确提示：

> No-Go editing is preserved for future complex-area planning; P1 Lawnmower does not yet route around internal No-Go regions.

若用户存在 No-Go 后点击 Generate：

```text
Planner 明确拒绝
```

不静默忽略 No-Go。

---

## 40. MissionAdapter

继续：

```text
MAV_CMD_NAV_WAYPOINT
```

P1 USV waypoint：

```text
altitude = 0.0
```

P1 必须在正式进入水试前重新验证：

```text
ArduRover / Boat
MAV_FRAME
waypoint semantics
WP_RADIUS
AUTO completion behavior
```

但是：

> Mission semantics 的调整属于 MissionAdapter，不得反馈污染 Coverage Planner。

---

## 41. P1 不自动 RTL

Mission 结束：

> 默认停留在最后一个 Coverage waypoint，由 ArduRover 当前任务配置决定后续行为。

P1 不自动增加：

```text
RTL
```

原因：

> 返回路径没有经过 Marine Coverage safety planning。

RTL 是否加入任务模板以后另行决定。

---

## 42. P1 不做运动学规划

P1 明确接受：

> Planner 只保证 polyline 几何安全。

不保证：

```text
real vessel envelope
turning-radius envelope
dynamic trajectory
```

P1 不实现：

```text
Dubins
minimum turn radius
curvature constraint
dynamic speed
current-aware path
```

实际行为通过：

```text
ArduRover SITL
+
field test
```

验证。

P2 再引入：

```text
vehicle footprint
turning radius
kinematic safety
```

---

## 43. Sensor Scope

P1 Freeze 必须保证：

```text
cameraEnabled
cameraRecord
sonarEnabled
sonarRecord
```

继续：

```text
UI editable
Task persistence
.plan round-trip
```

Coverage Planner：

> 完全不知道 Camera / Oculus M750d 的存在。

---

## 44. Sensor Runtime Control

P1 runtime sensor control 不是 Freeze blocker。

允许在 P1 后期做非常轻量 PoC：

```text
Task start
→ sensor start

Task finish/abort
→ sensor stop
```

但不要求建设：

```text
AcquisitionSession
database
pose/data synchronization framework
coverage/transit segment switching
AI pipeline
```

如传感器集成影响 P1 Coverage 进度：

> 立即下放到 P1.5/P2。

---

## 45. `.plan` Persistence

继续保持：

```text
top-level marine
    ↓
MarineTask definition
```

和：

```text
CoverageInspectionComplexItem
    ↓
planning artifact
```

分离。

P1 Task JSON 新增：

```text
sweepAngleMode
sweepAngleDeg
```

ComplexItem planning artifact 建议新增：

```text
selectedSweepAngleDeg
turnCount
```

继续保存：

```text
planningStatus
generatedPath
pathLengthM
planningMessage
```

---

## 46. Reload 原则

加载 `.plan`：

```text
restore MarineTask
restore PlanningResult
restore map path
restore MissionItems
```

不得：

```text
load
→ automatically run planner again
```

P0 原则继续冻结：

> `appendMissionItems()` 使用已保存的 PlanningResult，不重新规划。

---

## 47. P1 Geometry 数值策略

所有 Local Geometry：

```text
meter
double
```

几何操作采用统一 tolerance。

推荐初始：

```text
geometry epsilon ≈ millimeter / centimeter level
```

但具体 Clipper2 scaling/precision 在 P1-00 Readiness Audit 中根据：

```text
Clipper2 integration
expected polygon size
Windows/MSVC behavior
```

最终确认。

禁止在各模块各自定义不同 epsilon。

---

## 48. 测试矩阵

| ID | 场景 | 预期 |
| --- | --- | --- |
| T01 | Rectangle, Manual 0° | Success |
| T02 | Rectangle, Manual 90° | Success |
| T03 | General convex pentagon | Success |
| T04 | Rotated convex polygon | Success |
| T05 | Narrow polygon, width < swath | One lane |
| T06 | Safety margin = 0 | Success |
| T07 | Positive safety margin | All path inside inset |
| T08 | Safety margin removes region | Fail |
| T09 | Invalid swath = 0 | InvalidInput |
| T10 | Negative safety margin | InvalidInput |
| T11 | Self-intersecting polygon | InvalidInput |
| T12 | Manual sweep non-monotone concave | Fail clearly |
| T13 | Sweep-monotone concave polygon | Success where connector safe |
| T14 | Concave polygon with unsafe connector | Candidate Fail |
| T15 | No-Go exists | UnsupportedGeometry |
| T16 | Auto angle on rectangle | Deterministic |
| T17 | Auto angle on convex irregular polygon | Deterministic |
| T18 | Task edited after plan | Planned → Unplanned |
| T19 | Save/load | Path/angle/metrics restored |
| T20 | PlanningResult → MissionAdapter | MissionItems valid |
| T21 | `.plan` load | No replanning |
| T22 | ArduRover SITL | Mission executable |
| T23 | Real USV convex region | Coverage mission completes |

---

## 49. Geometry Property Tests

除固定 fixture 外，应增加核心 property checks。

成功路径必须满足：

```text
all path points finite
all path points within NavigableRegion
all path segments within NavigableRegion
pathLength > 0
spacing <= swath
```

Auto 模式重复运行：

```text
same input
→ same angle
→ same path
```

保证 deterministic。

---

## 50. Regression Tests

P1 不得破坏：

```text
normal QGC Plan
MissionItem
Survey
StructureScan
MissionController
PlanMasterController
```

对于已知 upstream baseline failure：

> 继续记录，不为 Marine 修改 QGC native expected behavior。

---

## 51. P1 Definition of Done

P1 Freeze Gate：

```text
Draw arbitrary convex Work Region
        ↓
Set swathWidth
        ↓
Set safetyMargin
        ↓
Manual / Auto sweep
        ↓
Generate
        ↓
Safe LawnMower Path
        ↓
Map display
        ↓
MAVLink Mission
        ↓
Save
        ↓
Reload
        ↓
Mission restored
        ↓
ArduRover SITL
        ↓
Real USV field execution
```

正式水试必须至少证明：

```text
任务区域可绘制
路径可生成
Mission 可上传
AUTO 可执行
USV 可完成覆盖航线
实际轨迹可导出/复核
```

---

## 52. P1 Freeze 不以 Sensor Runtime 为前提

P1 Freeze：

> 不以 Oculus M750d / Camera 自动启停成功作为阻塞项。

只要求：

```text
Task sensor configuration
UI
JSON persistence
```

不被破坏。

真正：

```text
sensor automatic execution
data synchronization
```

可以独立进入：

```text
P1.5
```

---

## 53. P1 明确禁止项

P1 禁止实现：

```text
BCD
cell decomposition

Visibility Graph
Dijkstra
A*
safe router

holes coverage
No-Go routing

turn radius
Dubins
kinematic planner

vehicle footprint

current-aware planning
energy optimization

dynamic swath

CoverageQualityMap

AI feedback
revisit planning

ROS2 runtime inside QGC

multi-USV
fleet planning

ROV 2.5D / 3D
```

---

## 54. 防止过度设计规则

P1 特别禁止新增：

```text
PlanningService
PlanningManager
CoverageEngine
GeometryService
StrategyFactory
AlgorithmPipeline
ExecutionStateMachine
```

除非后续 Code Review 能证明现有简单接口无法满足需求。

优先：

```text
small data types
small adapters
pure functions
explicit planner
unit tests
```

---

## 55. P1-00 — Implementation Readiness Audit

### 目标

正式写生产代码前确认：

```text
P0 merge state
QGC current APIs
polygon editing components
Clipper2 integration method
ArduRover Mission semantics
build/test baseline
```

### 必查

```text
marine/main contains P0
working tree clean enough for P1
current upstream baseline
existing QGC polygon editor APIs
existing Survey/GeoFence UI only as reference
third-party dependency policy
```

### 输出

只输出：

```text
Readiness Report
recommended file layout
API deviations
dependency decision
test baseline
```

### DoD

> 不修改 P1 生产逻辑。

---

## 56. P1-01 — Local Geometry + GeoReference

实现：

```text
Point2D
Polygon2D
Region2D
GeoReference
WGS84 ↔ ENU
```

测试：

```text
round-trip
known distance
finite validation
different headings
```

禁止：

```text
Coverage algorithm
```

DoD：

> Marine Planner Core 可以完全脱离 QGeoCoordinate 使用 meter geometry。

---

## 57. P1-02 — CoveragePlanningProblem + TaskAdapter

实现：

```text
CoveragePlanningProblem
CoveragePlanningSolution
CoveragePlanningError
CoverageTaskAdapter
```

将：

```text
MarineTask
```

适配为：

```text
CoveragePlanningProblem
```

同时更新：

```text
ICoveragePlanner
MockCoveragePlanner
PlannerRegistry tests
```

DoD：

```text
MarineTask
→ adapter
→ pure planner problem
```

闭环。

---

## 58. P1-03 — Input Validation + Capability Gate

实现：

```text
polygon validity
swath validation
safety validation
angle normalization
No-Go P1 rejection
```

错误必须明确。

DoD：

所有：

```text
unsupported input
```

在进入 LawnMower algorithm 前被识别。

---

## 59. P1-04 — Safety Inset

集成：

```text
Clipper2
```

实现：

```text
outerBoundary
→ inward offset
→ NavigableRegion
```

检测：

```text
empty
disconnected
invalid
```

DoD：

> 所有后续路径只在 NavigableRegion 内规划。

---

## 60. P1-05 — Monotonicity + Scanline

实现：

```text
sweep-aligned coordinate transform
polygon monotonicity
scanline intersection
inside interval extraction
```

重点测试：

```text
convex
L-shape
C-shape
multiple intersections
```

DoD：

> 不再存在“最远两交点跨越 polygon 外部”的风险。

---

## 61. P1-06 — Manual LawnMower

实现：

```text
manual sweep angle
lane count
lane placement
alternating direction
polyline assembly
```

仅处理：

```text
supported monotone region
```

DoD：

```text
Rectangle
General Convex Polygon
```

稳定生成路径。

---

## 62. P1-07 — Connector + Path Validation

实现：

```text
straight connector containment
final path validation
path length
turn count
```

如果 connector 不安全：

```text
candidate fail
```

不绕行。

DoD：

成功结果满足全部 Planner invariants。

---

## 63. P1-08 — Auto Sweep Angle

实现：

```text
edge-angle candidates
normalize/deduplicate
monotone filter
candidate generation
ranking
```

评分：

```text
turn count
→ path length
→ angle
```

DoD：

```text
deterministic
no continuous search
no optimization framework
```

---

## 64. P1-09 — ComplexItem + PlannerRegistry Integration

将：

```text
CoverageInspectionComplexItem
```

正式从：

```text
MarineTask → Planner
```

更新成：

```text
MarineTask
→ CoverageTaskAdapter
→ CoveragePlanningProblem
→ Planner
→ Solution
→ PlanningResult
```

保持：

```text
Task change invalidates Plan
```

DoD：

Mock 和 Lawnmower 都可通过 Registry 使用。

---

## 65. P1-10 — Work Region / No-Go UI

实现真实 polygon editing：

```text
outerBoundary
No-Go visual/edit
swath
safety
angle mode
manual angle
planner status
```

优先复用 QGC map editing UX。

DoD：

用户无需测试代码手工注入 polygon。

---

## 66. P1-11 — MissionAdapter + ArduRover Semantics

复核并实现：

```text
PlanningResult
→ MAV_CMD_NAV_WAYPOINT
```

确认：

```text
frame
altitude=0
sequence
AUTO behavior
WP_RADIUS interaction
```

禁止：

```text
Planner read ArduRover params
```

DoD：

生成的 Mission 可被 ArduRover SITL 接受。

---

## 67. P1-12 — Persistence + Sensor Configuration Baseline

实现/更新：

```text
SweepAngleMode
sweepAngleDeg
selectedSweepAngleDeg
turnCount
```

`.plan` round-trip。

确认：

```text
camera/sonar flags
```

继续正常保存。

不实施完整 Sensor Runtime。

DoD：

```text
save
→ close
→ load
→ Task + Path + Angle + Mission restored
```

---

## 68. P1-13 — ArduRover SITL

至少测试：

```text
rectangle
general convex polygon
manual angle
auto angle
safety margin
```

验证：

```text
mission upload
AUTO start
waypoint progression
mission completion
actual trajectory
```

重点记录：

```text
实际拐角
切角情况
WP_RADIUS 影响
```

但：

> 不因此提前实现 P2 turn-radius planner。

---

## 69. P1-14 — Real USV Field Validation

真实 USV：

选择：

> 无内部 No-Go 的一般凸多边形测试水域。

至少完成：

```text
Create task
Generate
Upload
AUTO
Coverage
Mission complete
Trajectory export
```

记录：

```text
planned path
actual path
cross-track behavior
turn behavior
coverage duration
failure/abort behavior
```

传感器：

> 可人工同步启动，不作为 P1 Freeze blocker。

---

## 70. P1-15 — Freeze P1

Final Review 只检查：

```text
1. Task / Plan / Mission 边界是否仍清楚？
2. Planner 是否仍纯 geometry？
3. QGC Core 是否新增 Marine-specific dependency？
4. Convex region 是否稳定可规划？
5. Safety inset 是否生效？
6. Auto angle 是否 deterministic？
7. Unsupported geometry 是否明确失败？
8. No-Go 是否没有被静默忽略？
9. Save/load 是否不重新规划？
10. ArduRover SITL 是否实际执行？
11. Real USV 是否完成至少一次任务？
12. 是否有 P2+ 功能偷偷进入？
```

全部通过：

```text
P1 FREEZE APPROVED
```

然后合并：

```text
feature/marine-p1-coverage
        ↓
marine/main
```

---

## 71. P1 推荐代码边界

建议演进为：

```text
custom/src/Marine/
├── MarineTask.*
├── MarineTypes.*
├── MarineTaskJsonCodec.*
│
├── Geometry/
│   ├── GeometryTypes.h
│   ├── GeoReference.h/.cc
│   └── MarineGeometry.h/.cc
│
├── Planning/
│   ├── ICoveragePlanner.h
│   ├── CoveragePlanningProblem.h
│   ├── CoverageTaskAdapter.h/.cc
│   ├── PlannerRegistry.*
│   ├── MockCoveragePlanner.*
│   └── LawnmowerCoveragePlanner.h/.cc
│
├── Mission/
│   └── ArduPilotMissionAdapter.*
│
└── QGC/
    └── MarinePlanContext.*
```

不要新增：

```text
Services/
Managers/
Engines/
Strategies/
Fleet/
ROS/
```

---

## 72. P1 最终产品能力表述

P1 完成后，可以准确描述为：

> QGroundControl Marine 已具备面向 USV 的第一版区域覆盖巡检能力：用户可自由定义工作区域，系统可针对一般凸多边形自动生成满足固定覆盖宽度和中心线安全距离要求的 LawnMower 航线，支持手动或自动扫掠方向选择，并可转换为 ArduRover MAVLink Mission 完成仿真和真实 USV 执行。

不能描述为：

> 已解决任意复杂水域自主覆盖规划。

后者属于 P2。

---

## 73. P1 → P2 的自然演进

P2 不推翻 P1。

演进关系：

```text
P1
single monotone region
+
straight connector

        ↓

P2
region decomposition
+
multiple cells
+
cell ordering
+
safe routing
+
No-Go / holes
+
vehicle footprint
+
turn-radius-aware planning
```

因此 P1 当前所有关键抽象：

```text
MarineTask
CoveragePlanningProblem
PlannerRegistry
PlanningSolution
MissionAdapter
```

均可继续保留。

---

## 74. v0.2 冻结结论

P1 v0.2 的核心技术路线正式收敛为：

```text
MarineTask
        ↓
CoverageTaskAdapter
        ↓
Local ENU CoveragePlanningProblem
        ↓
Safety Inset
        ↓
Manual / Auto Sweep Direction
        ↓
Monotonicity Check
        ↓
Scanline LawnMower
        ↓
Straight Safe Connector
        ↓
Path Validation
        ↓
PlanningResult
        ↓
ArduPilot Mission
        ↓
SITL
        ↓
Real USV
```

本版本明确避免：

```text
复杂区域泛化
Safe Router
BCD
运动学规划
完整 Sensor Data System
```

因此 P1 保持为：

> **一个可落地、可验证、范围受控的 USV Coverage V1。**

---

## 75. 下一步

在正式编写 P1 生产代码前：

```text
P1_COVERAGE_INSPECTION_SPEC_v0.2
        ↓
Architecture Review
        ↓
Freeze
        ↓
P1-00 Implementation Readiness Audit
        ↓
Codex implementation
```

Codex 第一项工作只能是：

```text
P1-00 Readiness Audit
```

不得直接开始：

```text
LawnmowerCoveragePlanner
```

直到 P1-00 审查通过。
