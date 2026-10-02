# V05-08 实施报告

状态：**IMPLEMENTED V05-08 R2；F1–F4 已由独立审核关闭，F5 已返修，等待新的独立复核。未暂存、未提交。**

本报告后续原 R1 验证段落属于历史证据，绑定旧二进制；不得视作 R2 新二进制的结果。
最新 R2 范围、验证与限制见末尾 “R2 F5 来源证书绑定” 段落及 `build/v05-08-evidence.json`。

## 1. 授权、基线和验证结论

本包按项目所有者的执行请求及冻结合同 `V05-08-DC-v1` 实施。开始时已核验 workspace 为
`F:\Projects\qgroundcontrol`，分支为 `feature/marine-p2-complex-coverage`，工作树 clean，HEAD 为
`45ce75a3320e76ab31ac94bf583828948ffe9136`。结束时 HEAD 不变。

磁盘 AGENTS.md 的阶段表仍停留在 V05-06；当前明确的 V05-08 owner 授权按 authority chain 优先适用。
本包没有修改 AGENTS.md 或冻结规范正文。编辑前已读取 CODING_STYLE.md、.github/CONTRIBUTING.md、
test/README.md、.github/ci-overview.md、tools/README.md、活动 v0.5 规范相关条款及本次合同，并检查实际 API。

最终 Windows build PASS；21 个 focused suites 全部 PASS；T1–T20 及 3 个新增返修测试全部 PASS。
57 个 established regression suites 中 56 PASS，MissionManagerTest 有一项已独立证明为基线既有、无关的
zh-CN placeholder failure。9 个变更生产 .cc 的 clang-tidy、33 个适用 C++ 格式检查、vehicle_null_check、
qt_translate_noop_check 和 git diff --check 均 PASS。
Clazy 不可用，记录 supplemental SKIP。正常 pre-commit 在任何 hook 执行前因本地只读数据库/缓存权限失败，
记录 ENVIRONMENT_BLOCKED_BEFORE_HOOKS，不能解释为 hook PASS。

变更为 27 个 tracked modifications 与 9 个 untracked 新文件，共 36 个文件；staged=0，conflicted=0。
build 下日志、XML、脚本与 JSON 为 ignored 验证证据，另由机器证据列出和绑定 SHA-256。
这些结论表示本包实现与可用本地核心验证完成；独立审核尚未完成，也没有 all-green regression 或完整 hook PASS 声明。

## 2. 实际 API 审计及最小架构

审计发现原 CoveragePlanningSolution 与 PlanningResult 的结果信息不对称；hard-safe Insufficient/AssessmentError
会丢失最终 canonical path；Artifact v3 仍采用 InfrastructureOnly；ComplexItem 的 Planned/Unplanned 与 Success
混用；MissionAdapter 原签名只接收 PlanningResult，缺少 current Task/identity 证据。
V05-07 已有 repairCandidates 和 step facts，可直接保留选中候选关联，无需新 repair truth。

按 A→B→C→D→E→F 完成共享结果领域模型、诊断、建议/选中 repair 事实、持久化、后端 gate 与验证。
数据流为纯规划领域结果 → CoverageTaskAdapter 地理坐标转换 → Artifact v3 IntegratedV05 → MissionAdapter。

新增 PlanningOutcome 模板在 local/geographic 结果中共用 readiness、tier、issues、suggestions、selected repair
及 diagnostics 的结构；CoverageEvaluation 同样共用 local/geographic 类型。CoverageQuality 只有
`outcome.coverageQuality` 一个字段。提取现有 CoveragePlanningError enum 仅解决头文件依赖，没有更改错误语义。
没有增加通用持久化框架、另一套评价器、另一套排序或 repair engine。

## 3. MissionReadiness、D0–D3 和 publication

| 条件 | PlanningStatus | Tier | MissionReadiness | Canonical path | Upload |
| --- | --- | --- | --- | --- | --- |
| hard-safe、preferred 满足、policy PASS | Success | D0 | Ready | 保留 | 允许 |
| hard-safe、preferred 未全满足、policy PASS | Success | D1 | ReadyWithWarning | 保留 | 允许 |
| hard-safe、可靠 Insufficient | Success | D0/D1 | ReviewRequired | 保留，含 residual | 拒绝 |
| hard-safe、AssessmentError | Success | D0/D1 | ReviewRequired | 保留 | 拒绝 |
| 无 hard-safe canonical，存在真实 raw-space 路径 | Failed | D2 | DiagnosticOnly | 空 | 拒绝 |
| 无可用连续路径或能力/连接未解决 | Failed | D3 | DiagnosticOnly | 空，仅 overlay | 拒绝 |
| InvalidInput | InvalidInput | 无可执行证书 | None | 空 | 拒绝 |

Simple 与 BCD 沿用原最终 candidate comparator，通过明确的 selectedCandidateIndex 关联安全 assessment、
coverage quality、repair runtime 与原始 metrics。最终选中后 publishCanonicalOutcome 产生完整结果，
不通过浮点路径相等关系追溯 provenance。
各 canonical leg 的整段安全认证沿用现有 CoverageSafety，硬安全与 preferred fallback 不变。

可靠 Insufficient 产生 CoverageBelowRequirement；AssessmentError 只产生 CoverageAssessmentFailed，
不会被写成 policy failure，也不会基于该失败 assessment 触发 repair。
D0/D1 产生 IngressNotAssessed；这不是对 launch/ingress/recovery 的认证。

## 4. D2/D3 diagnostics

只有找不到 hard-safe canonical candidate 才进入诊断。复用现有 Simple lane 或 BCD decomposition/cell ordering/
assembly/routing primitives，在 RawNavigationFreeSpace 生成真实连续候选，保持 target、resolved strategy、
sweep/decomposition 与 Swath 语义。每段完整检查 raw space，避免跨越 No-Go，再分类为 PreferredSafe、
HardSafeWarning、ExecutionUnsafe 或 HardUnsafe。D2 至少有一个 unsafe leg；canonical path 始终为空。

H/P/E 不变；诊断不运行 Coverage Repair、coverage certification、参数搜索或第二次 Auto strategy escalation。
当 hard track 为空而请求 Auto sweep 时，原 GlobalSweepSelector 在 raw space 只运行一次用于诊断 sweep，
不会更改非空 hard-region 的 canonical search。若 raw 生成发现完全 hard-safe 候选，则回到既有 certification、
repair opportunity 与最终 ranking；不会把它隐藏在 D2。

D3 保存结构化 unsupported/disconnected/unresolved overlay 与 explanation，不制造连接或假的路径。
diagnostic candidate/overlay 与 canonical path 分开存储。T7 有 direct Simple 对带孔 target 的真实 unsupported
端到端案例；T9 的 disconnected/unresolved 是 publication 单元案例，以结构化区域集与失败原因验证结果模型，
没有把它们宣称为任意拓扑的端到端 planner 支持。

## 5. Coverage availability

保留既有 evaluation 算法及 comparator，增加六个 metric 和五个 residual/core geometry 的 availability flags。
flag 只在相应可靠计算实际成功后设置。部分 AssessmentError 可保留已建立事实，例如 target area/tolerance；
失败后的未建立事实仍 unavailable。

JSON 每字段显式表示 availability：可用字段保存真实 value；不可用字段为
`{"available": false, "value": null}`。不可用 metric 的数值 0 或不可用 geometry 的空数组会被拒绝。
在内存中默认初始化的数字不是有效测量事实；save 由 flag 决定是否写值。
restore 原样恢复 availability，无评价器重算；AssessmentError 不伪造 Complete/Acceptable/Insufficient。

## 6. Issues、suggestions、selected repair provenance

实现合同要求的 15 个 stable issue codes 与 6 个 stable suggestion codes，severity 为 Info/Warning/Blocking。
geometry references 使用 CanonicalPathLegRange、DiagnosticCandidateLegRange、DiagnosticOverlay、CoverageResidual
类型，codec 检查 bounds 与目标 availability。issues 按 enum code 确定性排序，suggestions 按固定 code 顺序产生，
同时保留其 causal issue。Advice 不修改 Task，也不要求 UI 解析 human-readable message。

repair 仅保存最终选中结果的 attempted/applied/reason 与实际 applied component facts：component ID、component
path、entry index、direction、route-aware transition cost、before/after quality、path length 与 turn count。
初始 metrics 在 repair 前捕获；step facts 从 V05-07 runtime 直接提取。未应用 component 时 applied=false。
所有内部拒绝候选不进入 Artifact。原 CoverageRepair.cc 和 comparator 未改变。

## 7. Artifact v3 codec、校验和 lifecycle

新增 PlanningArtifactCodec，把 integrated encode/decode、structural validity、identity/current 判断与 backend
upload gate 集中在 Marine 域层。Artifact 仍为 v3，冻结 resultContract spelling 为 `IntegratedV05`。
Marine extension v2、Task v3 以及普通 QGC .plan 格式不变。

codec 校验 status/readiness/tier、enum、有限坐标、path/roles/assessments 数量、非零 legs、path metrics consistency、
coverage availability/status、当前 Task 的 coverage requirement、source identity、repair evaluation context/链/final facts、
issues/suggestions/reference bounds 与组合约束。
至少拒绝合同 T16 的 11 种 malformed combinations。结构校验不执行 planning、coverage evaluation、routing 或 repair。

load 先解析 inputIdentity，再判断是否与当前 Task 匹配。
匹配且 supported：使用当前 Task GeoReference 校验实际路径/metrics 一致性，并精确恢复完整 outcome；
不调用 planner、evaluator、router 或 repair。T5/T13 在 integration load 安装 forbidden planner 并确认调用次数 0，
完整 JSON round-trip 相等。
不匹配或 unsupported：stale=true，current result absent、Unplanned/None、upload blocked，原 artifact/identity
可原样重新保存。不会用改变后的当前 GeoReference 重新认证旧路径，也不会自动重规划。
unknown policy semantics 的 stale 数据仅做结构检查，不把当前 CAL-01 判据应用到未知历史 policy。

InfrastructureOnly 可作为过渡数据加载，保持 Unplanned/None/不可执行；直接 resave 保留原 spelling/data，
显式 replan 才能产生 IntegratedV05。历史 Mock/Lawnmower fresh fixtures 仍保留 InfrastructureOnly，
不会被误标为 v0.5 certified；生产 Auto/Simple/BCD 使用完整 integrated result。

ComplexItem 的 current lifecycle 与 status 分开：fresh Failed/DiagnosticOnly 也是 current outcome；
stale、no-plan 或 InfrastructureOnly 恢复没有 current integrated outcome。
schema-valid 但 topology InvalidInput 可以保存 current InvalidInput/None outcome，使用受支持的 unresolved identity；
有效输入在 geometry/safety/sweep 求解早期失败、尚无 resolved source 时，Failed/DiagnosticOnly/D3 也以
精确当前 PlanningSemantics{} 的 unresolved identity 恢复。此例外不能认证 Success 或上传。
schema-invalid Task 仍不能保存为有效 Task v3。

## 8. Semantic identity 决策

审计实际 PlanningInputIdentity::fromTask、matchesSupported、Auto resolved identity 与 artifact matching 后，
仅将全局 planning semantic 从 `p2.v0.5.planning.1` 升为 `p2.v0.5.planning.2`。
本包改变 integrated publication/certification/persistence semantics，因此旧 V05-07 identity 必须 stale。

Simple `simple-monotone.v2`、BCD `bcd.v0.5.v2`、Auto `auto.v1` 和 policy `coverage-quality.v1` 不变，
因为既有 canonical strategy、最终 ranking、安全/repair算法及 CAL-01/CAL-02 未改变。
PlanningInputIdentityTest golden fingerprints 随全局 bump 更新；T14 覆盖 Task 输入、大幅移动/reference 改变、
旧 planning、unsupported strategy 与 unsupported policy 的 stale/raw-resave 行为。

## 9. MissionAdapter gate 与原子性

主接口接收 PlanningArtifact 与 current MarineTask，复用 codec 的单一 validation/current truth。
必须为 current、non-stale、IntegratedV05、Success 且 Ready/ReadyWithWarning 才转换 canonical Geo path。
ReviewRequired、AssessmentError、DiagnosticOnly、None、Failed、InvalidInput、stale、InfrastructureOnly、malformed
均先拒绝；items 与 sequence number 保持原样。旧裸 PlanningResult overload fail-closed，避免绕过 identity gate。

MissionAdapter 不规划、不评价、不决定新 readiness、不路由、不修复、不改 geometry、不选 fallback 或 diagnostic。
ComplexItem.appendMissionItems 只调用该接口，没有另一套 upload truth，也不回退到 diagnostic path。
T17 直接覆盖 backend gate matrix 与原子性；T18 提供真实 Geo diagnostic candidate 并证明无 waypoint 被编码。

## 10. T1–T20 证据

全部对应 IntegratedPlanningResultTest::_testT01–_testT20。最终 XML 为
`build/v05-08-r1-focused-IntegratedPlanningResultTest-IntegratedPlanningResultTest.xml`，
tests=25（20 个合同测试、3 个返修测试加 init/cleanup），failures=0、errors=0、skipped=0。

| 合同 | 测试内容 | 结果 |
| --- | --- | --- |
| T1 | Ready/D0、全段 assessment、IngressNotAssessed、backend allow | PASS |
| T2 | ReadyWithWarning/D1、preferred issue、allow、P/E 不变 | PASS |
| T3 | M08 hard-safe Insufficient 保留 canonical/residual，ReviewRequired、reject | PASS |
| T4 | 实际 evaluator huge-swath AssessmentError、issue 互斥、no repair | PASS |
| T5 | AssessmentError partial availability、精确 Artifact/ComplexItem restore、planner 0 调用 | PASS |
| T6 | D2 raw 连续路径/整段分类、ExecutionUnsafe/HardUnsafe、Auto sweep、BCD No-Go | PASS |
| T7 | D3/unsupported，无假 route、overlay 非执行 | PASS |
| T8 | 非法输入及 schema-valid topology reject、current InvalidInput restore | PASS |
| T9 | 全 15 issue codes、typed refs、确定性及 disconnected/unresolved publication | PASS |
| T10 | suggestions 固定顺序、causal issue、重复一致、Task 不变 | PASS |
| T11 | M07 选中 repair provenance 与真实 runtime step facts 一致、round-trip | PASS |
| T12 | M00 initial Standard PASS 无 repair、初始 canonical 不变 | PASS |
| T13 | Simple/BCD/Auto 实际 planner 身份的 Ready/D1/Review/D2/D3 精确 restore，spy 0 调用 | PASS |
| T14 | input/reference/planning/strategy/policy mismatch stale，原样 resave | PASS |
| T15 | InfrastructureOnly 不升级、Unplanned/None、reject、原样 resave | PASS |
| T16 | 11 种 malformed artifact combinations reject | PASS |
| T17 | backend 全 gate matrix、bare-result reject、items/sequence 原子性 | PASS |
| T18 | diagnostics 无法进入 Mission 编码 | PASS |
| T19 | Auto→Simple/BCD source/sweep/escalation/identity 精确 round-trip | PASS |
| T20 | Ready/Review/D2/D3 重复、ring rotate/reverse 的 serialized semantic data 一致 | PASS |

## 11. Build、tests 和 checks 的实际命令

最终构建使用现有配置与 MSVC environment：

```powershell
cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >NUL && "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build/P0-01-marine-debug --target QGroundControl --parallel 8 > build/v05-08-r1-build.log 2>&1'
```

exit=0，日志含最终 QGroundControl.exe link。测试使用同一最终 binary，SHA-256 为
`FA23AC5D3FB5E6AF1AA29D131CE15BC58B1AFD639E67A32D2B36BB9DC407EB88`。
阶段性首次 layout/header 更新曾因现有 MSVC localized dependency cache 没重编消费者造成 stale-object link failure；
仅刷新已验证 custom object 路径中的 .obj 后构建成功，没有更改 build framework。
早期 focused fixture 的 controller/null-context 和测试前提问题已经修正，最终全量复跑结果取代早期失败证据。

```powershell
& build/v05-08-run-tests.ps1 -Selection build/v05-08-focused-selection.txt -Label r1-focused
& build/v05-08-run-tests.ps1 -Selection build/v05-08-regression-selection.txt -Label r1-regression
& build/v05-08-r1-run-tidy.ps1
git diff --check
```

runner 逐 suite 使用 Hidden/Wait 启动现有 Debug/QGroundControl.exe，参数为
`--unittest:<suite> --allow-multiple --unittest-output:<absolute output>.xml`，
环境为 offscreen/software、Windows Fonts、`*.debug=false`；测试前后 binary hash 相同。
focused 21/21 PASS；regression 56/57 PASS，runner exit=1 如实保留。
完整 suite、命令、exit、XML/log SHA-256 和测试 case 计数在 `build/v05-08-evidence.json`。

clang-tidy 命令形态为：

```text
clang-tidy.exe -p build/P0-01-marine-debug --extra-arg=/Y- --extra-arg=-Wno-unused-command-line-argument <changed-production.cc>
clang-format.exe --dry-run --Werror [--lines=<changed-range> ...] <changed-file>
.venv/Scripts/python.exe tools/analyzers/vehicle_null_check.py <changed-production-files>
.venv/Scripts/python.exe tools/analyzers/qt_translate_noop_check.py <changed-production-files>
.venv/Scripts/pre-commit.exe run --files <changed-files>
```

9 个 production .cc tidy exit=0；LLVM 22.1.8 为当前可用工具，不宣称等于 pre-commit 的工具 pin。
33 个 C++ 格式检查 exit=0：新增文件全文件检查、既有文件仅 changed ranges；纯删除且无新增行的头文件无适用 range。
没有扩大到历史未修改代码的全文件格式重写。两项仓库 analyzer exit=0。
Clazy supplemental SKIP；pre-commit 实际命令/完整输出保存在 result JSON 与 log，失败发生在 hook 之前。

## 12. 独立证明的无关回归与环境阻塞

MissionManagerTest 唯一失败为 `_testErrorAckFailureStrings`，strict logging 捕获
`QString::arg: Argument missing`。从基线 HEAD 的 `translations/qgc_source_zh_CN.ts` 读取到 context=PlanManager、
source=`Frame: %1`、translation=`框架1`，location 为 PlanManager.cc:704；translation 本身缺少 `%1`。
翻译、src/MissionManager/PlanManager.cc、test/MissionManager/MissionManagerTest.cc 三份文件的 HEAD/working
Git blob 相同，diff 为空。实际失败 XML 与该条目、blob/hash 证据在 `build/v05-08-baseline-proofs.json`。
据此分类 PRE_EXISTING_UNRELATED；没有修改 translation、相关 QGC production 或该测试来获得绿色结果。

pre-commit 捕获 `sqlite3.OperationalError: attempt to write a readonly database`，随后写
`C:\Users\Lin\.cache\pre-commit\pre-commit.log` 触发 PermissionError。
这是 ENVIRONMENT_BLOCKED_BEFORE_HOOKS；未执行 bypass、stage 或 commit。

## 13. 范围、偏离与停止门禁

架构偏离：0；需要 owner 扩大架构才能完成的冲突：0。
geometry backend、C/N/O、CoverageSafety.cc、CoverageRepair.cc、CoverageQualityPolicy.h、GlobalSweepSelector.cc
没有 diff；Simple/BCD 最终排序函数与 CoverageQuality comparator 的规范化函数体保持基线一致，证据在 scope-proofs。
CAL-01=0.99、CAL-02=0.50 m、H/P/E offset、repair 机会/条件、原 canonical ranking 均未改变。

没有 V05-09 QML/可视化、V05-10 SITL/manual acceptance/freeze、车辆配置系统或无关 QGC core cleanup。
MarineSITLValidationTest 的 unit regression PASS 不表示本次执行了真实 SITL。
后续 UI 呈现、完整 M00–M09 验收/SITL/software freeze/field acceptance 属于后续单独授权与证据。

本轮停止于 V05-08 独立审核门禁。下一步为独立 diff/code/evidence review；V05-09 需项目所有者另行授权。

## 14. 独立审核后的 R1 返修

本轮输入为 `build/v05-08-independent-review/REVIEW.md` 的 REQUEST_CHANGES；该独立报告与其证据原样保留。
审核 F1–F4 成立。本节记录修正后的实现和验证，不把原 T1–T20 PASS 解释为原实现已充分满足合同。
原机器证据另存为 `build/v05-08-evidence-before-r1.json`，当前 evidence.json 绑定 R1 最终 binary/源文件/XML/log。

| Finding | 最小修正 | 新证据 |
| --- | --- | --- |
| F1 / P1 Strict gate bypass | matching Task 时比较 canonical quality.requirement 与 Task requirement；repair evaluations 共用 canonical requirement/strategy/policy/target/tolerance | `_testCoverageRequirementBinding`：保留 Strict identity、伪造内部自洽 Standard Acceptable/PASS，validate/save/load/backend 均拒绝；backend items/sequence 不变 |
| F2 / P2 valid failure lifecycle | Simple/BCD/Auto 有效输入后的 geometry/safety/global-sweep failure 均发布 D3；Simple lane-schedule failure 同样发布；ComplexItem 按实际 planner id/version 识别 production contract | `_testFailedCurrentOutcome`：3 个 production planners × manual tiny Swath、Auto tiny Swath、finite safety backend failure 共 9 组，fresh IntegratedV05/current、精确 restore、spy 0、upload 拒绝 |
| F3 / P2 repair facts contradiction | context 绑定所有 before/after；相邻 after→before 全部持久化 facts 一致；最终 after 与 canonical quality facts 一致 | `_testRepairEvaluationConsistency`：target 400/800、before target、requirement/strategy/policy、residual、scalar、fallback 8 种损坏及真实两步 repair 的 chain 损坏，validate/save/load/backend 均拒绝 |
| F4 / P2 no-replan spy gap | ForbiddenPlanner 接收实际 requested planner id/version；T13 分别覆盖 Simple、BCD、Auto，并确认 registry lookup 返回该 spy | T13 对三策略的全部 5 个既有夹具 restore/resave 精确且 calls=0 |

repair facts 比较复用现有 quality JSON 表示，覆盖 availability、可靠 metrics、status/error、strategy/requirement/policy、
core/residual/fallback geometry 和 metadata；human-readable message 不属于认证事实，不参与关联相等判断。
这只是保存事实的关系校验，没有新增评价、修改 comparator 或重算 residual。

早期 solver failure 不伪造 resolved strategy/sweep。无 source 的 D3 使用已有 unresolved identity，codec 对 exact
current global/policy/input 才恢复 current，stale 仍清 current、upload 仍拒绝；未改 identity encoding、版本或算法。
geometry 失败之前未可靠生成的 overlay geometry 保持空，表示没有可提供的几何，解释保留原失败原因，不编造 route。

无 evaluator/router/repair 重算由明确的静态调用依赖验证支持，而非声称已有这些算法的 runtime spies：
ComplexItem.load → PlanningArtifactCodec.load → JSON/identity/structure helpers/validateResult；所有 codec helpers 与
integration load body 中无 evaluateCoverageQuality、routeStatic、repairCoverageCandidate 或 planner->plan 调用。
其函数体哈希与 forbidden call counts 在 `build/v05-08-r1-restore-dependency-proof.json`。
path metrics consistency 与冻结 comparator 的 stored scalar 检查仍按原结构校验语义执行。

本次 R1 实际修改 8 个文件：PlanningArtifactCodec.cc、SimpleMonotoneCoveragePlanner.cc、
BoustrophedonCoveragePlanner.cc、AutoCoveragePlanner.cc、CoverageInspectionComplexItem.cc、
IntegratedPlanningResultTest.cc/.h、本实施报告。完整 V05-08 集合仍为 36 个文件。
没有修改 frozen contract、规范、AGENTS、translations、geometry/safety/repair 算法、最终排序或 semantic versions。

R1 先跑 `-Label r1-targeted` 的 IntegratedPlanningResultTest，25 cases 全 PASS，再复跑完整 21 focused 与
57 established regressions。旧独立审核的 MissionControllerTest/PlanMasterControllerTest 缓存权限失败仍保留为
当时的 FAIL，不追溯改写；本轮正常 Windows Qt 缓存环境的实际结果单独记录在 R1 XML/log。本轮 MissionControllerTest 与 PlanMasterControllerTest 均 PASS；
完整 57 suites 为 56 PASS、1 个既有 MissionManager placeholder failure。
正常 pre-commit 环境阻塞证据沿用先前已执行记录，本轮未重复尝试、未 bypass。

本轮停于独立复核门禁。findings 是否可关闭由新的独立 review 决定；未暂存、未提交、未进入 V05-09。

## 15. 修改文件


- `custom/CMakeLists.txt`
- `custom/src/Marine/MarineTypes.h`
- `custom/src/Marine/Mission/ArduPilotMissionAdapter.cc`
- `custom/src/Marine/Mission/ArduPilotMissionAdapter.h`
- `custom/src/Marine/Planning/AutoCoveragePlanner.cc`
- `custom/src/Marine/Planning/BoustrophedonCoveragePlanner.cc`
- `custom/src/Marine/Planning/CoveragePlanningError.h`
- `custom/src/Marine/Planning/CoveragePlanningProblem.h`
- `custom/src/Marine/Planning/CoverageQualityEvaluator.cc`
- `custom/src/Marine/Planning/CoverageQualityEvaluator.h`
- `custom/src/Marine/Planning/CoverageSafety.h`
- `custom/src/Marine/Planning/CoverageStrategySemantics.h`
- `custom/src/Marine/Planning/CoverageTaskAdapter.cc`
- `custom/src/Marine/Planning/IntegratedPlanningResult.cc`
- `custom/src/Marine/Planning/IntegratedPlanningResult.h`
- `custom/src/Marine/Planning/PlanningOutcome.h`
- `custom/src/Marine/Planning/PlanningResult.h`
- `custom/src/Marine/Planning/SimpleMonotoneCoveragePlanner.cc`
- `custom/src/Marine/PlanningArtifact.h`
- `custom/src/Marine/PlanningArtifactCodec.cc`
- `custom/src/Marine/PlanningArtifactCodec.h`
- `custom/src/MissionManager/CoverageInspectionComplexItem.cc`
- `custom/src/MissionManager/CoverageInspectionComplexItem.h`
- `custom/test/Marine/ArduPilotMissionAdapterTest.cc`
- `custom/test/Marine/AutoCoveragePlannerTest.cc`
- `custom/test/Marine/BoustrophedonCoveragePlannerTest.cc`
- `custom/test/Marine/CoverageComplexItemTest.cc`
- `custom/test/Marine/CoverageInspectionPlanCreatorTest.cc`
- `custom/test/Marine/CoverageRepairTest.cc`
- `custom/test/Marine/IntegratedPlanningResultTest.cc`
- `custom/test/Marine/IntegratedPlanningResultTest.h`
- `custom/test/Marine/MarinePlanIntegrationTest.cc`
- `custom/test/Marine/PlanningInputIdentityTest.cc`
- `custom/test/Marine/SimpleMonotoneCoveragePlannerTest.cc`
- `custom/test/Marine/SimpleMonotoneCoveragePlannerTest.h`
- `docs/marine/V05_08_IMPLEMENTATION_REPORT.md`

## Long-run owner authorization — V05-08 commit gate

Project owner subsequently authorized the bounded P2-V05 long-run task, including fresh separate-role review and local V05-08 commit after `APPROVED FOR COMMIT`. The task explicitly permits `--no-verify` only for the same proven pre-hook readonly cache/database failure after equivalent required checks have independently passed. It does not authorize pushing, weakening tests, changing the unrelated zh-CN defect, or treating blocked checks as PASS.

The prior independent REQUEST_CHANGES evidence in `build/v05-08-independent-review/` and the R1 review evidence in `build/v05-08-independent-review-r1/` remain preserved. R1 source behavior is unchanged. A new separate GPT-6.1 Sol reviewer is reviewing the complete 36-file worktree against the original contract and baseline; this report does not assert that review's outcome in advance.

The normal `.venv/Scripts/pre-commit.exe run --files <all 36 changed files>` was retried during the owner-authorized task. Exit=1 before hooks: `sqlite3.OperationalError: attempt to write a readonly database`, followed by permission denied writing the pre-commit log. Full output: `build/v05-08-longrun/precommit.log`. This is ENVIRONMENT_BLOCKED_BEFORE_HOOKS, not PASS.

The latest root independent verification remains: current configured Windows build PASS; 21 focused suites PASS including T1–T20 and the three repair tests; 9 production clang-tidy checks and 36 format/repository/diff checks PASS. Three additional Mission suites failed in the sandbox: MissionManager has two cache-permission failures and the unchanged zh-CN placeholder failure; MissionController and PlanMasterController have cache-permission failures. These failures are preserved in raw XML and are not recorded as PASS. Supplied R1 regression evidence remains 56/57 PASS with the one unchanged MissionManager translation failure; its raw XML/hash records were audited separately.

V05-08 commit and closure remain pending the new independent `APPROVED FOR COMMIT` result. No V05-09 work has begun at this checkpoint.

## R2 F5 来源证书绑定（最新实施证据）

最新真正独立 commit review 返回 REQUEST_CHANGES：原 F1–F4 已 CLOSED，新增 F5/P1。
本轮依据 owner 长任务 F 段仅修 F5，保留 `build/v05-08-commit-review/` 与此前所有审查证据。
进入本轮时 36 文件工作树未提交，staged=0；未清理、暂存、提交、push 或进入 V05-09。

本轮仅修改 `PlanningArtifactCodec.cc`、`IntegratedPlanningResultTest.cc/.h` 及本实施报告。
新增 pure 字段一致性检查：当前任务/source 扫描模式相同；Manual 选定角与任务按 180°
归一化结果相同、sweep semantic 为空；Auto 使用 global-sweep.v1；直接 Simple/BCD 请求
只能解析为自身当前 ID/version，Auto 仍允许已批准的 Simple/BCD delegation。
源/结果选定角、coverage/source strategy 与 identity 的原有检查继续适用。
未调用 planner/evaluator/router/repair，未修改指纹格式、版本、calibration、规划算法或 comparator。

按规范 §§29–30、合同 §24，未知 sweep semantic version 为 stale：不认证为 current、清除
当前路径、Unplanned/None、禁止上传、无重规划、原始 artifact exact resave。
Manual 使用已知 global-sweep.v1、Auto 缺少版本，以及匹配身份下的模式/角度/直接策略矛盾
属于 malformed，validate/save/load/backend 均拒绝。不会将已知矛盾降格为 stale 来绕过关联校验。
无 source 的 InvalidInput/早期 D3 精确默认身份与 InfrastructureOnly 保持原有行为；完整测试通过。

新增 8 个命名 data rows：两个扫描模式方向、伪造角度、真实 0° 路线冒认 90° Task、
直接 Simple→BCD、直接 BCD→Simple、Manual 已知 Auto 版本、Auto 缺版本。
逐类证明 standalone 结构有效而 current cert 无效，validate/save/load 拒绝；实际
ArduPilotMissionAdapter 拒绝且既有 MissionItem 不变、序列不变。真实路线 fixture 同时证明
两个方向的路径不同并各自规划成功。正例覆盖负角/180° 整倍数归一化、合法 Auto Simple/BCD
解析、Manual/Auto 未知 sweep version stale 恢复、raw exact resave 与 matching spy 零调用。

Windows Debug Qt 6.11.1/MSVC 增量构建命令：
`cmd.exe /d /c call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build/P0-01-marine-debug --target QGroundControl --parallel 8`。
最终 exit=0，日志 `build/v05-08-r2-build-final.log`。首次测试新增类型名笔误导致编译失败，已修正为
实际 PlannerStrategyIdentity；失败日志 `build/v05-08-r2-build.log` 保留，未把该尝试改记 PASS。

新二进制 focused 21/21 PASS，IntegratedPlanningResultTest 34/34 PASS，含 T1–T20、原 3 修复
及新增 9 cases。补充持久化/Mission 6/6 PASS。授权解除测试 AppData 缓存限制后，
MissionController 20/20 PASS，PlanMasterController 71/71 PASS；MissionManager 6/7，保留
既有 zh_CN `%1` placeholder failure（相关 source/test/translations 无 baseline diff）。
新 binary 的 57-suite 完整回归 **UNEXECUTED**；旧 56/57 仅属于历史 R1，不能认证新 binary。
新 XML/log/result 分别为 `build/v05-08-r2-focused-python-*`、`-extra-python-*`、`-mission-python-*`。

9 个 production TUs clang-tidy PASS（现 compile DB + 实际 MSVC 环境），33 格式检查、2 仓库
analyzers、git diff --check PASS，详见 `build/v05-08-r2-tidy-results.json` 和
`build/v05-08-r2-evidence/checks.json`。Clazy supplemental static-analysis SKIP。
正常 pre-commit 重试 exit=1：只读 DB、before hooks ENVIRONMENT_BLOCKED，不能记为 PASS；
日志 `build/v05-08-r2-precommit.log`。未使用 commit bypass。

早期 PowerShell 启动器在 QGC XML 完成后等待不返回，已仅中止本轮对应 sessions；其 XML/log
均保留，标为 ABORTED_RUNNER_AFTER_XML，不从该尝试猜测进程 exit。最终使用 build 内 Python
subprocess 逐 suite 直接等待进程，CREATE_NO_WINDOW、独立 stdout/stderr、same-binary hash，
600 s timeout（未触发）。最终 runner 的 process exit 和 XML 均记录，不隐藏 warning/failure。

F5 修复完成、可用本轮检查完成，工作包关闭仍 **PENDING INDEPENDENT REVIEW**。
DoD 未获得独立 approval，不能 commit；下一步为新独立复核，批准后才允许 V05-08 commit。
不自动实施 V05-09；未认证 SITL、人工/UI、software freeze 或实艇/外场验收。
