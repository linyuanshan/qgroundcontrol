# P0 海洋机器人智能作业平台工程底座——开发设计规格 v0.2 精简定稿版

## 1. 阶段定位

### 1.1 P0 的唯一目标

P0 不做真实 USV 覆盖算法，不做 ROS 2 集成，也不做完整任务执行平台。

P0 只完成一个最小但完整的架构闭环：

```text
Coverage Task
    ↓
ICoveragePlanner
    ↓
MockCoveragePlanner
    ↓
PlanningResult
    ↓
CoverageInspectionComplexItem
    ↓
ArduPilotMissionAdapter
    ↓
MAVLink MissionItem
    ↓
.plan 保存 / 加载
```

完成 P0 后，应证明以下架构成立：

> Task ≠ Plan ≠ Mission

即：

- Task：机器人要完成什么作业；
- Plan：规划器准备如何完成任务；
- Mission：ArduPilot 实际执行什么命令。

P0 的价值在于为 P1～P6 建立稳定接口，而不是提前实现未来所有能力。

---

## 2. P0 设计原则

## 2.1 最小实现原则

只实现 P1 马上需要的架构。

如果一个机制：

> P1 不实现它也能继续开发，

原则上不放入 P0。

---

## 2.2 合理预留，不提前实现

允许：

```text
WorkRegion
├── outerBoundary
└── noGoRegions[]
```

因为 P1 马上需要禁航区。

允许：

```text
PlannerRegistry
```

因为 P1/P2 很快会有多个 Coverage Planner。

不允许提前实现：

```text
多机器人任务分配系统
完整ROS Bridge
完整执行状态机
3D规划框架
复杂JSON迁移系统
```

---

## 2.3 QGC 与算法解耦

Coverage Planner 核心层禁止依赖：

```text
QML
QObject
Fact
QGeoCoordinate
MissionItem
Vehicle
```

Planner 工作于自己的纯数据结构。

---

## 2.4 不修改 QGC 核心规划算法

P0 应优先使用 QGroundControl Custom Build / CustomPlugin 扩展机制。

原则：

> 新功能尽量全部放入 custom build。

除非后续确认存在无法通过扩展机制解决的问题，否则不修改：

```text
src/MissionManager/SurveyComplexItem.*
src/MissionManager/TransectStyleComplexItem.*
src/API/QGCCorePlugin.*
```

---

## 3. P0 明确要做什么

P0 只完成以下 10 项。

## P0-1 Marine Task 最小数据模型

建立：

```text
MarineTask
```

能够表达：

- Task ID；
- Task Name；
- Task Type；
- 单车辆 ID；
- 工作区域；
- No-Go 区域；
- Coverage 基础参数；
- Planner ID；
- Camera 基础配置；
- Sonar 基础配置。

---

## P0-2 Coverage Planner 接口

建立：

```text
ICoveragePlanner
```

使 Coverage 算法与 QGC 解耦。

---

## P0-3 PlannerRegistry

支持通过稳定 Planner ID 查找 Coverage Planner。

P0 只有：

```text
marine.coverage.mock
```

P1 增加：

```text
marine.coverage.lawnmower
```

---

## P0-4 MockCoveragePlanner

建立确定性的测试 Planner。

它只用于验证架构，不用于实船。

---

## P0-5 PlanningResult

统一表达 Planner 输出路径和最少量统计信息。

---

## P0-6 CoverageInspectionComplexItem

建立 Marine Coverage Task 与 QGC PlanView 的适配层。

---

## P0-7 ArduPilotMissionAdapter

将 PlanningResult 转换成：

```text
MAV_CMD_NAV_WAYPOINT
```

P0 只支持 waypoint。

---

## P0-8 `.plan` Marine 扩展

Task Definition 保存于：

```json
"marine"
```

顶层对象。

Planning Path 保存在对应 ComplexMissionItem 中。

---

## P0-9 最小 QML

能够：

- 创建 Coverage Inspection；
- 显示 Task；
- 调用 Mock Planner；
- 显示规划路径；
- 显示 Planned / Unplanned 状态。

---

## P0-10 自动测试与回归

完成 Core、Planner、JSON、Mission、Plan Save/Load 的最小测试闭环。

---

## 4. P0 明确不做什么

以下功能全部推迟。

## 4.1 推迟到 P1

不做：

```text
真实 LawnMower Coverage
Polygon Offset
Safety Margin Geometry
No-Go绕行
自动 Sweep Angle
Coverage Ratio 精确计算
起终点优化
最小转弯半径
Camera/Sonar真实启停
数据采集控制
ArduRover实船执行策略
inputDigest
Stale Hash检测
完整ExecutionState
```

---

## 4.2 推迟到 P2/P3

不做：

```text
BCD
复杂Cell Decomposition
USV运动学路径
局部避障路径可视化
海流优化
能耗优化
动态Swath Width
Coverage Quality Map
自动重访
```

---

## 4.3 推迟到 P4/P5

不做：

```text
ROV
Point3D规划体系
2.5D
3D Coverage
Viewpoint Planning
三维重建轨迹
```

---

## 4.4 推迟到 P6

不做：

```text
VehicleAssignments[]
FleetManager
TaskDispatcher
Multi-USV
USV-ROV协同
```

P0 暂时只有：

```text
vehicleId
```

---

## 4.5 P0 不设计 MarineTaskBridge

暂不定义：

```text
IMarineTaskBridge
NullMarineTaskBridge
ROS 2 Bridge
MAVLink custom message
UDP/WebSocket protocol
```

原因：

Camera/Sonar 真实控制方案尚未确定。

待 P1 明确真实机器人端硬件和 ROS 2 节点后再设计通信接口。

---

## 5. 推荐目录结构

P0 建议控制新增核心源码数量，不建立过大的平台目录。

```text
custom/
│
├── CMakeLists.txt
│
├── src/
│   │
│   ├── CustomPlugin.h
│   ├── CustomPlugin.cc
│   │
│   ├── Marine/
│   │   ├── MarineTypes.h
│   │   ├── MarineTask.h
│   │   ├── MarineTask.cc
│   │   ├── MarineTaskJsonCodec.h
│   │   ├── MarineTaskJsonCodec.cc
│   │   │
│   │   ├── Planning/
│   │   │   ├── ICoveragePlanner.h
│   │   │   ├── PlannerRegistry.h
│   │   │   ├── PlannerRegistry.cc
│   │   │   ├── MockCoveragePlanner.h
│   │   │   └── MockCoveragePlanner.cc
│   │   │
│   │   ├── Mission/
│   │   │   ├── ArduPilotMissionAdapter.h
│   │   │   └── ArduPilotMissionAdapter.cc
│   │   │
│   │   └── QGC/
│   │       ├── MarinePlanContext.h
│   │       └── MarinePlanContext.cc
│   │
│   ├── MissionManager/
│   │   ├── CoverageInspectionComplexItem.h
│   │   ├── CoverageInspectionComplexItem.cc
│   │   ├── CoverageInspectionPlanCreator.h
│   │   └── CoverageInspectionPlanCreator.cc
│   │
│   └── PlanView/
│       ├── CoverageInspectionEditor.qml
│       └── CoverageInspectionMapVisual.qml
│
└── test/
    ├── MarineTaskModelTest.cc
    ├── MarineTaskJsonTest.cc
    ├── CoveragePlannerTest.cc
    ├── CoverageComplexItemTest.cc
    └── MarinePlanIntegrationTest.cc
```

P0 不建立额外的：

```text
Application/
Bridge/
Fleet/
Data/
Vehicle/
3D/
```

目录。

需要时再增加。

---

## 6. MarineTypes.h

核心数据结构集中在：

```text
MarineTypes.h
```

减少 P0 文件数量。

---

## 6.1 GeoPoint

```cpp
struct GeoPoint {
    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;
    double altitudeM = 0.0;
};
```

P0 Planner 暂时可直接使用 GeoPoint。

P1 实现真正 Coverage 时，再引入 Local NED / ENU 转换。

---

## 6.2 GeoPolygon

```cpp
struct GeoPolygon {
    std::vector<GeoPoint> vertices;
};
```

---

## 6.3 WorkRegion

```cpp
struct WorkRegion {
    GeoPolygon outerBoundary;

    std::vector<GeoPolygon> noGoRegions;
};
```

这里从 P0 就保留：

```text
noGoRegions[]
```

因为 P1 直接使用。

---

## 7. MarineTask

建议 P0 不建立复杂继承层次。

不使用：

```text
MarineTaskPackage
    ↓
CoverageInspectionTask
    ↓
...
```

P0 直接采用：

```cpp
struct MarineTask
```

---

## 7.1 Task Type

```cpp
enum class MarineTaskType {
    CoverageInspection
};
```

不要提前加入尚未实现的多个 task type。

P4/P5 需要时扩展 enum。

---

## 7.2 SensorConfig

P0 简化为：

```cpp
struct SensorConfig {
    bool cameraEnabled = true;
    bool cameraRecord = true;

    bool sonarEnabled = true;
    bool sonarRecord = true;
};
```

P0 不定义：

```text
StartTrigger
StopTrigger
SensorRole
RecordingSession
AI Policy
```

---

## 7.3 CoverageConfig

P0 建立最少两个参数：

```cpp
struct CoverageConfig {
    double swathWidthM = 0.0;

    double safetyMarginM = 0.0;
};
```

Mock Planner 可以暂时忽略。

它们存在的目的只是让 P0 Task 从一开始就是：

> Coverage Inspection Task

而不是完全空的抽象对象。

P1 再增加：

```text
overlapRatio
sweepAngleMode
manualSweepAngle
startStrategy
...
```

---

## 7.4 PlannerConfig

```cpp
struct PlannerConfig {
    std::string plannerId;
};
```

P0 不加入：

```text
plannerVersion
capabilities
configSchemaVersion
```

---

## 7.5 MarineTask

最终：

```cpp
struct MarineTask {
    std::string id;

    std::string name;

    MarineTaskType type =
        MarineTaskType::CoverageInspection;

    std::string vehicleId;

    WorkRegion region;

    CoverageConfig coverage;

    SensorConfig sensors;

    PlannerConfig planner;
};
```

这就是 P0 Task 模型全部内容。

---

## 8. Task ID

Task ID 使用 UUID。

要求：

```text
创建 Task
→ 自动生成 UUID
```

一旦生成不得因：

- 修改 polygon；
- 修改 planner；
- 保存；
- 加载；

而变化。

---

## 9. ICoveragePlanner

P0 不建立：

```text
IPathPlanner
```

只建立当前实际需要的：

```cpp
class ICoveragePlanner
```

接口：

```cpp
class ICoveragePlanner {
public:
    virtual ~ICoveragePlanner() = default;

    virtual std::string id() const = 0;

    virtual std::string displayName() const = 0;

    virtual PlanningResult plan(
        const MarineTask& task) const = 0;
};
```

P1 如果需要更加干净的：

```text
CoveragePlanningProblem
```

再在 P1 中从 MarineTask 转换。

P0 不提前建立过多中间类型。

---

## 10. PlanningResult

定义：

```cpp
enum class PlanningStatus {
    Success,
    InvalidInput,
    Failed
};
```

结果：

```cpp
struct PlanningResult {
    PlanningStatus status =
        PlanningStatus::Failed;

    std::vector<GeoPoint> path;

    double pathLengthM = 0.0;

    std::string message;
};
```

P0 不加入：

```text
turnCount
coverageRatio
issues[]
plannerVersion
inputDigest
```

需要时在 P1 增加。

---

## 11. PlannerRegistry

职责非常单一：

> Planner ID → Planner Instance

建议：

```cpp
class PlannerRegistry {
public:
    bool registerPlanner(
        std::shared_ptr<ICoveragePlanner> planner);

    std::shared_ptr<ICoveragePlanner>
    planner(const std::string& id) const;
};
```

P0 注册：

```text
marine.coverage.mock
```

P1 注册：

```text
marine.coverage.lawnmower
```

---

## 12. MockCoveragePlanner

Planner ID：

```text
marine.coverage.mock
```

显示名称：

```text
Architecture Test Planner
```

要求：

1. 输入合法 polygon；
2. 输出固定且确定性的测试路径；
3. 相同输入必须得到相同输出；
4. 不声称实现 Coverage；
5. UI 明确显示：

```text
Architecture Test Planner
Not for field operation
```

推荐简单输出：

```text
Polygon 第一个顶点
        ↓
Polygon 中心
        ↓
Polygon 对角方向顶点
```

或者其他稳定测试路径。

禁止在这里开始写 LawnMower。

---

## 13. MarinePlanContext

P0 保留 Plan 级上下文，但做最小实现。

其职责只有：

```text
当前Plan拥有的MarineTask
+
PlannerRegistry
```

建议：

```cpp
class MarinePlanContext : public QObject {
    Q_OBJECT

public:
    explicit MarinePlanContext(
        PlanMasterController* controller);

    MarineTask* task(
        const std::string& taskId);

    void addTask(const MarineTask& task);

    void removeTask(
        const std::string& taskId);

    void clearTasks();

    PlannerRegistry& plannerRegistry();
};
```

内部：

```cpp
std::unordered_map<
    std::string,
    MarineTask
> _tasks;
```

---

## 14. 不建立 MarinePlanContextRegistry

P0 不实现：

```text
MarinePlanContextRegistry
```

当前 Context 直接与对应：

```text
PlanMasterController
```

绑定。

推荐生命周期：

```text
PlanMasterController
        │
        └── owns MarinePlanContext
```

如果后续实际出现多个 Context 查询问题，再引入 Registry。

---

## 15. CoverageInspectionComplexItem

它是：

> Marine Task 与 QGC PlanView 的适配器。

不是 Planner。

---

## 15.1 核心职责

只负责：

- taskId；
- 调用 Planner；
- 保存 PlanningResult；
- 给 QML 提供路径；
- 将 path 交给 MissionAdapter；
- save/load。

---

## 15.2 核心成员

建议：

```cpp
std::string _taskId;

PlanningResult _planningResult;

MarinePlanContext* _marineContext;
```

---

## 15.3 规划调用

流程：

```text
CoverageInspectionComplexItem
        ↓
MarinePlanContext
        ↓
MarineTask
        ↓
PlannerRegistry
        ↓
ICoveragePlanner
        ↓
PlanningResult
```

---

## 15.4 明确禁止

禁止：

```text
CoverageInspectionComplexItem
直接实现：
polygon clipping
sweep line
BCD
turn planning
coverage calculation
```

这些必须在 Planner 中实现。

---

## 16. P0 Planning State

P0 不建立复杂状态机。

只定义：

```cpp
enum class PlanningState {
    Unplanned,
    Planned,
    Failed
};
```

规则：

### 创建 Task

```text
Unplanned
```

### Planner 成功

```text
Planned
```

### Planner 失败

```text
Failed
```

### 修改任意影响规划的 Task 参数

直接：

```text
Unplanned
```

即可。

P1 再增加：

```text
Stale
inputDigest
```

---

## 17. ArduPilotMissionAdapter

职责：

```text
PlanningResult.path
        ↓
MAVLink MissionItem[]
```

P0 仅支持：

```text
MAV_CMD_NAV_WAYPOINT
```

---

## 17.1 推荐接口

```cpp
class ArduPilotMissionAdapter {
public:
    static bool appendWaypoints(
        const PlanningResult& result,
        QList<MissionItem*>& items,
        QObject* parent,
        int& sequenceNumber,
        QString& errorString);
};
```

---

## 17.2 P0 不处理

暂不处理：

```text
DO_CHANGE_SPEED
Camera commands
Sonar commands
ReturnToLaunch
Hold
Turn radius
Condition commands
ROS messages
```

全部留到 P1。

---

## 18. CoverageInspectionComplexItem::appendMissionItems

必须遵循：

```text
PlanningResult
        ↓
ArduPilotMissionAdapter
        ↓
MissionItem[]
```

禁止：

```text
appendMissionItems()
再次调用 Planner
```

Mission 生成只能使用已经存在的 PlanningResult。

---

## 19. CoverageInspectionPlanCreator

建立：

```text
CoverageInspectionPlanCreator
```

流程：

```text
removeAll()
    ↓
clear Marine tasks
    ↓
create MarineTask
    ↓
insert CoverageInspectionComplexItem
    ↓
bind taskId
```

---

## 19.1 不插入航空语义

USV PlanCreator 不允许自动加入：

```text
Takeoff
Land
```

第一版只创建：

```text
Mission Settings
+
Coverage Inspection
```

---

## 20. JSON 总体策略

继续坚持：

> Task Definition 与 Mission 分开保存。

`.plan`：

```text
root
│
├── mission
├── geoFence
├── rallyPoints
│
└── marine
```

---

## 21. 顶层 Marine JSON

P0 格式：

```json
{
  "marine": {
    "version": 1,
    "tasks": []
  }
}
```

P0 只使用一个顶层 version。

暂不建立复杂三层 schemaVersion。

---

## 22. MarineTask JSON

建议定稿：

```json
{
  "version": 1,

  "id": "9a20e6c8-xxxx-xxxx-xxxx",

  "type": "coverageInspection",

  "name": "养殖区域覆盖巡检",

  "vehicleId": "",

  "region": {
    "outerBoundary": [
      {
        "lat": 38.100000,
        "lon": 121.100000
      },
      {
        "lat": 38.100000,
        "lon": 121.200000
      },
      {
        "lat": 38.200000,
        "lon": 121.200000
      }
    ],

    "noGoRegions": []
  },

  "coverage": {
    "swathWidthM": 10.0,
    "safetyMarginM": 2.0
  },

  "planner": {
    "id": "marine.coverage.mock"
  },

  "sensors": {
    "cameraEnabled": true,
    "cameraRecord": true,
    "sonarEnabled": true,
    "sonarRecord": true
  }
}
```

---

## 23. ComplexMissionItem JSON

P0 建议：

```json
{
  "type": "ComplexItem",

  "complexItemType":
      "marineCoverageInspection",

  "version": 1,

  "taskId":
      "9a20e6c8-xxxx-xxxx-xxxx",

  "planning": {
    "plannerId":
        "marine.coverage.mock",

    "path": [
      {
        "lat": 38.1000,
        "lon": 121.1000
      },
      {
        "lat": 38.1500,
        "lon": 121.1500
      }
    ],

    "pathLengthM": 123.4
  }
}
```

---

## 24. 为什么 Task 与 Planning 分开保存

Task：

```text
工作区域
No-Go
传感器
规划参数
```

属于：

> 作业定义。

Planning：

```text
generated path
plannerId
pathLength
```

属于：

> 某次生成的规划结果。

Mission：

```text
MAV_CMD_NAV_WAYPOINT
```

属于：

> ArduPilot 执行内容。

因此：

```text
Task
≠
PlanningResult
≠
Mission
```

不能混在一个对象中。

---

## 25. MarineTaskJsonCodec

所有 Task JSON 序列化统一集中。

接口：

```cpp
class MarineTaskJsonCodec {
public:
    static bool save(
        const MarineTask& task,
        QJsonObject& json,
        QString& errorString);

    static bool load(
        const QJsonObject& json,
        MarineTask& task,
        QString& errorString);
};
```

禁止 JSON 逻辑散落在：

```text
Planner
QML
MissionAdapter
Task
```

---

## 26. JSON 兼容规则

P0 只要求三条。

## 26.1 缺失必填字段

加载失败。

---

## 26.2 未知字段

忽略。

便于以后扩展。

---

## 26.3 version > 当前支持

明确返回：

```text
Unsupported marine task version
```

P0 不建立 migration framework。

未来真正出现 Version 2 时再增加。

---

## 27. `.plan` 保存流程

推荐：

```text
PlanMasterController::saveToJson()
        ↓
QGC保存Mission
        ↓
QGC保存GeoFence
        ↓
QGC保存RallyPoints
        ↓
CustomPlugin::postSaveToJson()
        ↓
MarinePlanContext
        ↓
MarineTaskJsonCodec
        ↓
planJson["marine"]
```

---

## 28. `.plan` 加载流程

推荐：

```text
读取 .plan
    ↓
CustomPlugin::preLoadFromJson()
    ↓
读取 marine.tasks
    ↓
建立 MarineTask
    ↓
QGC加载 mission
    ↓
CoverageInspectionComplexItem::load()
    ↓
读取 taskId + planning
    ↓
关联 MarineTask
    ↓
加载完成
```

如果：

```text
ComplexItem.taskId
```

找不到对应：

```text
marine.tasks[]
```

必须：

```text
加载失败或明确报错
```

不能静默创建空 Task。

---

## 29. Orphan Task 处理

保存前应保证：

```text
marine.tasks[]
```

只保存当前 Plan 实际引用的 Task。

因此：

```text
删除 CoverageInspectionComplexItem
```

后，对应 MarineTask 也应删除。

P0 不需要复杂 garbage collector。

直接在 ComplexItem 删除或 Plan clear 时同步删除即可。

---

## 30. QML 最小规格

## 30.1 CoverageInspectionEditor.qml

P0 只显示：

```text
Coverage Inspection

Task Name
Task ID

Planner:
Architecture Test Planner

Coverage Width
Safety Margin

Camera:
Enabled / Record

Sonar:
Enabled / Record

Planning State

[Generate Test Plan]
```

并永久显示：

```text
Architecture Test Planner
Not for field operation
```

---

## 31. CoverageInspectionMapVisual.qml

只显示：

```text
Outer Boundary
No-Go Polygons
Generated Path
```

禁止 QML 做几何算法。

---

## 32. P0 CMake

基于当前 custom-example 扩展。

P0 不要求创建额外复杂 library hierarchy。

可以先：

```text
Marine sources
直接加入 Custom Build target
```

如果后续 P1/P2 Core 代码规模明显增加，再拆：

```text
MarineCore STATIC library
```

P0 不强制建立 MarineCore library。

这是对 v0.1 的进一步简化。

---

## 33. QML Module

建议建立：

```text
Marine.Plan
```

包含：

```text
CoverageInspectionEditor.qml
CoverageInspectionMapVisual.qml
```

如果实际接入当前 Custom Build 时建立新 module 会显著增加构建复杂度，则 P0 可以暂时沿用现有：

```text
Custom.Plan
```

P1 前再重命名。

原则是：

> 不为了命名美观拖慢 P0。

---

## 34. CustomPlugin 修改范围

P0 只需要 CustomPlugin 完成：

```text
① complexMissionItemNames()
② createComplexMissionItem()
③ planCreators()
④ preLoadFromJson()
⑤ postSaveToJson()
```

不增加：

```text
mavlinkMessage()
Bridge
Custom telemetry
ROS
```

---

## 35. 测试体系

P0 测试缩减为 5 组。

---

## 36. MarineTaskModelTest

测试：

```text
Task创建
UUID存在
默认TaskType
WorkRegion
No-Go
CoverageConfig
SensorConfig
```

以及：

```text
outerBoundary < 3 points
```

判为非法。

---

## 37. MarineTaskJsonTest

测试：

```text
MarineTask
 ↓
JSON
 ↓
MarineTask
```

要求关键字段一致。

覆盖：

```text
正常round-trip
noGoRegions
sensor config
unknown field
unsupported version
missing id
missing region
```

---

## 38. CoveragePlannerTest

验证：

```text
PlannerRegistry
+
MockCoveragePlanner
```

覆盖：

```text
register
lookup
duplicate id
missing planner
valid polygon
invalid polygon
same input → same path
```

---

## 39. CoverageComplexItemTest

验证：

```text
taskId
PlanningState
generate mock plan
path exposed to UI
appendMissionItems
```

输入：

```text
3 point PlanningResult
```

应生成：

```text
3 × MAV_CMD_NAV_WAYPOINT
```

---

## 40. MarinePlanIntegrationTest

这是 P0 最关键测试。

执行完整：

```text
Create Plan
    ↓
Create CoverageInspectionTask
    ↓
Generate Mock Plan
    ↓
Build MissionItems
    ↓
Save .plan
    ↓
Destroy Plan
    ↓
Reload .plan
```

最终验证：

```text
Task ID一致
Region一致
No-Go一致
Sensor Config一致
Planner ID一致
Generated Path一致
MissionItems一致
```

---

## 41. Task 修改测试

流程：

```text
Generate Plan
    ↓
PlanningState = Planned
    ↓
修改 swathWidth
```

结果：

```text
PlanningState = Unplanned
```

不需要 P0 实现 Hash。

---

## 42. Delete Test

流程：

```text
Create Task A
+
ComplexItem A
```

删除 ComplexItem A。

保存。

确认：

```text
marine.tasks[]
```

不存在 A。

---

## 43. Regression Test

P0 完成后至少执行当前 QGC 与以下模块相关的现有测试：

```text
PlanMasterController
MissionController
MissionItem
SurveyComplexItem
```

目标：

> Marine Custom Build 不破坏 QGC 原生 Mission Planning。

---

## 44. P0 手工 Smoke Test

至少完成：

```text
1. 启动 Custom QGC

2. Create Plan

3. 选择 Coverage Inspection

4. 绘制/设置测试工作区域

5. 显示：
   Architecture Test Planner

6. Generate Test Plan

7. 地图出现路径

8. 保存 .plan

9. 关闭 Plan

10. 重新加载 .plan

11. Task恢复

12. Region恢复

13. Path恢复

14. Mission Items恢复
```

---

## 45. P0 文件级实施顺序

后续桌面版 ChatGPT 实施时，严格按此顺序。

## P0-01 Custom Build 基线确认

确认：

```text
custom build
CustomPlugin
CMake
PerimeterScan示例
```

能够正常编译。

不要一开始删除 PerimeterScan 示例。

---

## P0-02 MarineTypes + MarineTask

实现：

```text
MarineTypes.h
MarineTask.h/.cc
```

同时完成：

```text
MarineTaskModelTest
```

---

## P0-03 MarineTaskJsonCodec

实现：

```text
MarineTaskJsonCodec
```

同时完成：

```text
MarineTaskJsonTest
```

---

## P0-04 Planner Framework

实现：

```text
ICoveragePlanner
PlannerRegistry
MockCoveragePlanner
```

同时完成：

```text
CoveragePlannerTest
```

---

## P0-05 MarinePlanContext

建立：

```text
PlanMasterController
↔
MarinePlanContext
```

实现：

```text
Task add
Task find
Task remove
Task clear
PlannerRegistry
```

---

## P0-06 CoverageInspectionComplexItem

以当前：

```text
PerimeterScanComplexItem
```

为最主要工程参考。

先实现：

```text
taskId
planning state
generated path
save/load
```

---

## P0-07 ArduPilotMissionAdapter

实现：

```text
PlanningResult
→
MAV_CMD_NAV_WAYPOINT
```

随后让：

```text
CoverageInspectionComplexItem::appendMissionItems()
```

调用 Adapter。

---

## P0-08 CoverageInspectionPlanCreator

实现 USV 版本 PlanCreator。

禁止插入：

```text
Takeoff
Land
```

---

## P0-09 CustomPlugin 注册

加入：

```text
CoverageInspection
```

到：

```text
complexMissionItemNames()
createComplexMissionItem()
planCreators()
```

---

## P0-10 QML

实现：

```text
CoverageInspectionEditor.qml
CoverageInspectionMapVisual.qml
```

先做最小 UI。

---

## P0-11 Marine JSON Integration

使用：

```text
postSaveToJson()
preLoadFromJson()
```

实现：

```text
.plan["marine"]
```

---

## P0-12 Integration Test

完成：

```text
MarinePlanIntegrationTest
```

---

## P0-13 Regression

运行 Marine tests + QGC 相关 tests。

---

## P0-14 Smoke Test

人工完成 UI save/load 全流程。

---

## P0-15 P0 Freeze

满足 DoD 后：

> 停止继续扩展 P0。

直接进入 P1。

---

## 46. P0 Definition of Done

只有以下全部完成，P0 才算完成。

## A. Build

- [ ] Custom QGC 可正常编译
- [ ] Windows Debug 构建通过
- [ ] Windows Release 构建通过
- [ ] 不引入 Windows-only 非必要依赖
- [ ] 原生 QGC 核心源码无必要侵入式修改

---

## B. Marine Task

- [ ] MarineTask 建立
- [ ] UUID Task ID
- [ ] WorkRegion 支持 outerBoundary
- [ ] WorkRegion 支持 noGoRegions[]
- [ ] CoverageConfig 存在
- [ ] Camera 配置存在
- [ ] Sonar 配置存在
- [ ] 单 vehicleId 存在

---

## C. Planner

- [ ] ICoveragePlanner 建立
- [ ] PlannerRegistry 建立
- [ ] marine.coverage.mock 注册
- [ ] Mock Planner 可运行
- [ ] PlanningResult 可返回路径
- [ ] 修改 Task 后状态回到 Unplanned

---

## D. QGC Integration

- [ ] CoverageInspectionComplexItem 可创建
- [ ] CoverageInspectionPlanCreator 可创建任务
- [ ] CustomPlugin 正确注册
- [ ] PlanView 可显示 Coverage Inspection
- [ ] MapVisual 可显示区域
- [ ] MapVisual 可显示规划路径

---

## E. Mission

- [ ] ArduPilotMissionAdapter 建立
- [ ] path 可转成 MAV_CMD_NAV_WAYPOINT
- [ ] sequenceNumber 正确
- [ ] ComplexItem 不包含规划算法
- [ ] Planner 不直接生成 MissionItem

---

## F. Persistence

- [ ] `.plan` 顶层包含 marine
- [ ] MarineTask 可保存
- [ ] MarineTask 可加载
- [ ] ComplexItem taskId 可保存/加载
- [ ] generated path 可保存/加载
- [ ] Broken taskId 能明确报错
- [ ] 删除 ComplexItem 不留下 orphan task

---

## G. Tests

- [ ] MarineTaskModelTest
- [ ] MarineTaskJsonTest
- [ ] CoveragePlannerTest
- [ ] CoverageComplexItemTest
- [ ] MarinePlanIntegrationTest
- [ ] QGC 相关 regression tests

全部通过。

---

## 47. P0 最终验收闭环

最终必须实际证明：

```text
Create Coverage Inspection
        ↓
MarineTask
        ↓
MockCoveragePlanner
        ↓
PlanningResult
        ↓
Map Display
        ↓
ArduPilotMissionAdapter
        ↓
MAV_CMD_NAV_WAYPOINT
        ↓
Save .plan
        ↓
Reload .plan
        ↓
Task + Path + Mission
全部恢复
```

这就是 P0 的全部使命。

---

## 48. P0 完成后冻结的核心设计

以下内容进入 P1 后原则上保持稳定。

## ① Task ≠ Plan ≠ Mission

不可破坏。

---

## ② WorkRegion

```text
outerBoundary
noGoRegions[]
```

---

## ③ Planner 插件模式

```text
PlannerRegistry
    ↓
ICoveragePlanner
```

---

## ④ 稳定 Planner ID

例如：

```text
marine.coverage.mock
marine.coverage.lawnmower
```

---

## ⑤ QGC ComplexItem 只做 Adapter

禁止将 Coverage Algorithm 塞回 ComplexItem。

---

## ⑥ MissionAdapter

```text
PlanningResult
↓
MAVLink Mission
```

保持独立。

---

## ⑦ `.plan` 顶层 Marine Task

```text
marine.tasks[]
```

---

## ⑧ ComplexItem 通过 taskId 关联 Task

不复制完整 Task Definition。

---

## 49. P0 明确留给 P1 的设计事项

P1 开始时再正式设计：

```text
CoveragePlanningProblem

Local NED / ENU coordinate frame

Polygon offset

Safety margin

No-Go polygon subtraction

Fixed swath width

Overlap

Manual sweep angle

Auto sweep angle

Sweep line generation

Concave polygon handling

Coverage validation

Start strategy

Path ordering

Turn handling

inputDigest

Stale state

Mission speed

Camera start/stop

Sonar start/stop

Data recording

ROS 2 / Companion communication

ArduRover SITL

Field test
```

这些才是：

> **P1 USV 养殖区域自主覆盖巡检 V1**

的核心内容。

---

## 50. 给桌面版 ChatGPT 的执行约束

后续开始本地实施 P0 时，应遵守以下规则。

## 规则 1

先检查当前仓库实际代码，再修改。

不能假设当前本地 QGC 与设计时 GitHub master 完全一致。

---

## 规则 2

优先使用：

```text
custom build
CustomPlugin
custom QML
```

不得无理由修改 QGC core。

---

## 规则 3

每完成一个工作包：

```text
实现
→
编译
→
测试
→
再进入下一项
```

不能一次修改大量文件后统一排错。

---

## 规则 4

如果实际 QGC API 与本规格不同：

> 优先适配当前 QGC API，同时保持本规格定义的架构边界。

不要为了逐字匹配类名破坏正确架构。

---

## 规则 5

发现需要新增未在 P0 规格中的大型抽象时：

> 先判断是否属于 P1+。

如果不是 P0 完成闭环的必要条件，不实现。

---

## 规则 6

严禁在 P0 顺手实现：

```text
Lawnmower
BCD
ROS 2
multi-robot
real sensor control
```

---

## 规则 7

P0 达到 Definition of Done 后：

> 立即停止 P0 开发并进入 P1。

---

## 51. P0 v0.2 最终原则

P0 不追求：

> “平台看起来已经很完整。”

P0 只追求：

> “后续真实 Coverage 能够在正确架构上开始开发。”

因此 P0 的核心可以浓缩成：

```text
MarineTask
+
ICoveragePlanner
+
PlannerRegistry
+
CoverageInspectionComplexItem
+
ArduPilotMissionAdapter
+
Marine JSON Persistence
+
Tests
```

七个部分。

做到这七件事，并打通：

```text
Task
→
Plan
→
Mission
→
Save
→
Load
```

P0 即正式完成。
