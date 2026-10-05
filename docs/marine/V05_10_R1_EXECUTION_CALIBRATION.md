# V05-10-R1 执行预留校准协议

状态：测量前声明；2026-10-05。只校准测试 fixture 的 E，不改变产品、H/E 语义或冻结 S10。

## 1. 固定测量集与环境

source HEAD 为 cc70699c25f7f396907cf937c95f41daa9ff64b7。冻结 Docker image、Rover 4.7.0、home、frame、参数与 S10-05 相同：WP_SPEED=5、WP_RADIUS=3、TURN_RADIUS=0.9、ATC_TURN_MAX_G=0.6、WP_ACCEL=0、WP_JERK=0。不得修改这些参数。

测量集预先固定为 CAL20、CAL60、CAL120：C=N 为边长 L=20/60/120m 的矩形，无 O，H=P=E=0，swath=0.75L，Standard，Manual90，Auto→SimpleMonotone。三者均有两条覆盖线及转场，须初始质量 PASS、D0/Ready、no repair。每个测量独占一个全新容器；不将其 NavigationArea 通过/失败作为校准选择条件或正式验收结果。

车辆当前位置锚定首 canonical waypoint（0.001m 冻结容差）。执行前保存 position、heading、速度向量/ground speed、stationary、armed、flight mode、MISSION_CURRENT state/sequence、已有 Mission count、fresh container identity。必须 disarmed、stationary（速度≤0.05m/s）、无 active/已有可执行 Mission。

## 2. 测量及唯一 E 选择规则

完整保存 GLOBAL_POSITION_INT 轨迹、速度向量和 time_boot_ms，以及 Mission Start ACK、ACTIVE、ordered progression、Complete。测量以下值：

- D：所有实际轨迹采样点到完整 canonical polyline 的最小距离的最大值（包含转弯切角，单位 m）。
- V：GLOBAL_POSITION_INT 速度向量的最大水平速度（m/s）。
- T：连续 GLOBAL_POSITION_INT 的最大 time_boot_ms 间隔（s）。

合并三次有效测量的 Dmax/Vmax/Tmax。测量有效条件为完整任务执行、所有数值有限、Tmax≤0.5s、Vmax≤6m/s；环境/任务执行失败或测量集不完整立即阻断，不通过调参补齐。

只计算一次：E = ceil((1.25 × Dmax + Vmax × Tmax + 0.02m) / 0.5m) × 0.5m。

1.25 提供样本集内偏移的 25% 额外预留；V×T 保守覆盖一个未采样间隔内的移动；0.02m 覆盖坐标量化/投影舍入；按 0.5m 向上取整便于审计。该规则是本地测试夹具校准，不是动态车辆认证、Rover 参数自动转 E 或产品默认值。若 E≤0 或 E>6m，停止并返回 OWNER_DESIGN_DECISION_REQUIRED；不得重新定义公式以获得 PASS。

## 3. 测量后机械确定最终 fixture

L = 向上取整到 10m 的 max(40m, 20×E, 4×E²/1m)。若 L>120m，停止；最终几何必须处于测量尺度范围内。swath=0.75L。

M00：C=N=rect(0,0,L,L)，无 O，H=P=0，E 为固定校准值，Standard，Manual90，Auto。两条覆盖线距 C 水平边界 swath/2，执行端点距 N 垂直边界 E。尺寸规则同时保证 Standard critical core 的圆端足迹条件 (E−0.5m)²≤swath/2×1m−0.25m²，并控制角部残余比例；实际 evaluator 仍须证明初始 PASS、D0/Ready、no repair。

M04：C=rect(0,0,L,L)，N=rect(−(E+1),−(E+1),L+E+1,L+E+1)，无 O，H=0，P=8m，E 为同一固定值，Strict，Manual90，Auto。HardExecutionTrackRegion 包含 C 且留 1m 余量；preferred track 相对 C 内缩 7m，不能完整覆盖角部，而 hard 候选可完整覆盖，须证明 Strict PASS、D1/ReadyWithWarning、PreferredSafetyViolated。

最终 canonical string/hash、Task/fingerprint、Artifact/result hash、source HEAD、工作树差异和 binary SHA 在正式验收前一次固定。固定后重跑全部要求的 focused/Mission gate、fresh M00/M04 和 connected reject 场景。正式验收 FAIL 时停止，不再调整 E、L、swath、N 或 Rover 参数。

## 4. 隔离、证据与停止门

runner 以唯一命名的 fresh Docker container 运行每个 ALLOW/measurement；使用 try/finally 收集 inspect/log 并停止容器，即使 QTest 提前失败也不会复用失败车辆。保留退出的容器身份及日志，后续场景新建实例。C++ fixture 校验 fresh identity、执行前状态和冻结参数；reject 保持既有 zero-mutation oracle。

原始测量、失败、容器记录及正式 acceptance 不覆盖，写入 build/v05-10-r1/。GUI manual acceptance 本轮 NOT EXECUTED。冻结规范/合同/技术设计、生产代码不修改；不 stage/commit/push；不自我批准。

## 5. 测量工具修正记录（最终测量集开始前）

首轮启动就绪日志位置错误，以及内部日志尚未创建的启动竞态，均未进入车辆测量。calibration-3 的 CAL20/CAL60 执行完成，但 CAL120 在执行前出现 0.00106168707977m 的入口锚定误差，超过冻结 0.001m 容差。根因是 fixture 在车辆原点锚定后，真实 CoverageTaskAdapter 以区域中心再次投影。此轮不选 E，原始证据完整保留。

测量工具改为：按真实 Task→CoverageTaskAdapter→Auto 管线计算入口位置，固定两次数值平移修正，再独立验证冻结入口容差；不调整矩形尺寸、swath、E、轨迹或 Rover 参数。将从零重跑 CAL20/CAL60/CAL120，不将前轮有效点拼入新测量集。§2 的 E 公式、尺度选择和有效性门保持原值。完成工具修正后，任何任务执行失败或测量集不完整按 §2 阻断。
