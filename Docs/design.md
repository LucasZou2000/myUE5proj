# 《MYPROJ2》(暂名) — 策划案 + 技术方案

> 本文档为当前完整方案，可直接作为后续技术文档（网络协议细化、接口定义等）扩展的基础输入。

---

## 0. 项目基本参数

| 项 | 值 |
|---|---|
| 开发者 | 单人；目标是交付一个可验证的联机灰盒切片，不追求 8 人团队完整度 |
| 周期 | 约 2 个月全职；M0-M8 是方向性路线，实际首个可玩版本以 M0-M4 为止 |
| 引擎 | UE5.8（Top Down Template + 内建 Unreal MCP） |
| 美术资源 | AI 生成为主，作为占位资源接入骨骼 Socket 部件堆叠系统（详见第 4 节）；Mod 制作工具链暂不做，不影响底层渲染方案 |
| 数值策划 | 本阶段不做，先用直觉数值占位 |
| 关卡美术 | 功能优先，不追求商业级美术完成度 |
| 项目目的 | 验证个人编程能力、学习 UE5 工程实践；优先证明网络、战斗、搜刮闭环 |
| 呈现方向 | 地图：3D diorama 场景（HD-2D 式镜头/景深）；角色：骨骼 Socket 驱动的部件堆叠 sprite（详见第 4 节），Mod 制作工具链暂不做 |
| 联机 | Co-op（无 PVP），主机权威的简化同步模型（非帧同步，详见 3.1） |
| 存档 | 本地 SaveGame，无需网络后端 |
| 战斗操作 | 双摇杆式：WASD 移动 + 鼠标自由瞄准 |
| 怪物 AI | UE 内建 Behavior Tree，完整节点（巡逻/索敌/警觉/攻击/呼叫增援） |
| 地图结构 | 一张固定大地图（手搭，参考塔科夫），核心区域固定，空地区域随机刷新营地/兴趣点 |

---

## 1. 核心玩法设计

### 1.1 局内循环
```
匹配/组队(1-4人)
   → 载入固定大地图，选择插入点(Insertion Point)
   → 搜刮(Loot) + 遭遇战(Combat) + 环境事件(Events)
   → 决策：继续深入 or 前往撤离点(Extraction Point)
   → 撤离成功 → 战利品带回仓库(Stash)
   → 撤离失败(死亡/超时) → 携带物品全部丢失
```

### 1.2 战斗
- 双摇杆式实时战斗：WASD 移动 + 鼠标自由瞄准，PVE 定位，敌人强度通过数值曲线（后续再细化）单独调控。
- 武器数值可通过装配部件修改（后坐力/精度/噪音半径），本阶段**不做外观切换**，只做数值层。

### 1.3 搜刮与仓库
- 局内容器随机权重刷新战利品，权重表按地图分区区分。
- 局外仓库为网格背包，跨局持久化（本地 SaveGame）。

### 1.4 撤离机制
- 撤离点分固定/条件触发/呼叫倒计时三类，配置化摆点，不用改代码。
- 团队允许部分玩家提前撤离、部分玩家继续。

### 1.5 局外成长
- 仓库升级、基地功能模块解锁（工作台/维修/制作等，具体清单可后补，先跑通框架）。

### 1.6 怪物设计
- 分层：巡逻 → 索敌 → 索敌升级(呼叫增援) → 强化个体(精英/Boss)。
- 差异化优先靠数值和行为树参数区分，不依赖美术区分（呼应"美术尽量简单"的原则）。

---

## 2. 开发优先级与里程碑

原则：**先跑通"能联机玩的灰盒游戏"，最后一步才接入美术表现。**

| 里程碑 | 目标 |
|---|---|
| M0 | 工程框架 + 网络骨架（Listen Server，第 3.1 节的同步模型从这里开始搭） |
| M1 | 角色移动与交互（占位几何体，双摇杆移动+瞄准，交互检测） |
| M2 | 战斗系统（开火、命中判定、伤害、受击反馈） |
| M3 | 背包与道具系统（网格背包、堆叠、使用） |
| M4 | 搜刮/容器系统（DataTable 驱动的随机战利品） |
| M5 | 基地与局外成长（仓库、基地功能占位） |
| M6 | 武器装配系统（数值层，不接外观） |
| M7 | 怪物生成与 AI（Spawner + 完整 Behavior Tree） |
| M8 | 撤离流程串联（插入→闭环→结算→写回存档，单人+联机各验证一遍） |
| M9 | 简单美术表现接入（详见第 4 节） |
| M10 | 打磨（视剩余时间投入） |

M0-M8 完成即为"整个游戏流程跑起来"的验收基准。

### 2.1 两个月范围控制

首个可玩版本必须按以下顺序交付，不能并行扩张系统：

1. **Vertical Slice A（M0-M2）**：2 人 Listen Server，移动、瞄准、交互占位、单把 Hitscan 武器、单个可受伤目标。
2. **Vertical Slice B（M3-M4）**：一个背包、一个容器、一次权威随机掉落和拾取。
3. **Vertical Slice C（M7-M8 的最小子集）**：一种敌人、一个生成点、一个固定撤离点、成功/失败结算。

以下内容默认延期，只有 Vertical Slice C 稳定后才能进入：基地模块树、武器多槽装配、条件/呼叫类撤离点、Boss、复杂随机营地、完整部件堆叠美术和 Mod 工具链。M5/M6/M9 应视为扩展里程碑，而不是两个月交付承诺。

禁止在 M0-M2 引入：GAS、大型事件总线、自研移动复制、回滚/延迟补偿、Steam/EOS 登录、无缝旅行、专用服务器、复杂 UObject 网络子对象。它们都不能直接提高当前灰盒闭环的验收价值。

---

## 3. 核心系统架构

### 3.1 网络同步模型（主机权威的简化同步，非帧同步）

**角色分工（本段为修订后的权威定义，优先于旧版描述）：**
- Host = Server（Listen Server，2-4 人局无需独立服务器）。
- **Server 权威实体**：所有会改变他人结果或持久化结果的状态，包括玩家最终位置/碰撞、玩家生命值、武器弹药、伤害、怪物、容器内容和撤离结算。
- **客户端预测实体**：拥有 Pawn 的 Client 可以即时预测自己的移动和本地表现，但预测结果必须通过 `CharacterMovementComponent` 的标准 ServerMove 链路被 Server 确认或纠正。客户端不能自行决定最终 Transform、伤害或掉落。

**同步内容一览：**

| 数据类别 | 权威方 | 同步方式 | 同步内容 |
|---|---|---|---|
| 怪物状态 | Server | 持续 Replication | 位置、朝向、血量、当前 AI 状态（用于表现，如警觉/攻击动画） |
| 怪物受击 | Server | 属性复制 + 表现事件 | 血量/死亡是可迟加入恢复的 Replicated Property；命中特效可用 Unreliable Multicast |
| 玩家自身移动 | Server 权威，Autonomous Proxy 预测 | `ACharacter` 原生移动复制 | 输入由客户端产生；位置/速度由 Server 校验并复制，禁止另写 Transform 上报协议 |
| 玩家血量 | Server（伤害来源是怪物攻击判定，天然在 Server 结算） | Replication | 当前/最大血量，供本地 UI 和队友状态显示 |
| 玩家攻击行为 | 触发信息由本地 Client 发起，效果判定在 Server | 事件 RPC（Client→Server→Multicast） | Client 只提交序列号和瞄准方向；Server 派生发射位置/武器/伤害并判定，Multicast 仅播放表现 |
| 战利品/容器 | Server | 状态变更时属性复制 | 首次打开由 Server 生成；容器状态和内容必须支持迟加入恢复，M3/M4 再选 Fast Array 等实现 |
| 撤离/关卡流程状态 | Server | 属性复制 + 表现事件 | 阶段/结束时间是 Replicated Property；开始/结束特效可广播 |

**设计原则：**
1. 会影响当前或未来玩法结果的持久状态使用 Replicated Property；短暂且丢失后不影响一致性的表现使用 RPC/Multicast。
2. 玩家移动采用 UE 原生预测和 Server 校正。PVE 不代表可以放弃碰撞与权威，否则会产生穿墙、位置分歧、主机与客户端看到不同交互目标等基础问题。
3. 玩家之间只同步"渲染需要的最少信息"（位置、朝向、简化动画状态枚举、血量），不同步具体输入或精确路径。
4. 开火/命中特效走事件，弹药/血量/死亡走属性复制。事件可以丢失，玩法状态必须最终收敛并支持迟加入。

**在 UE5 中的落地方式：**
- 保留 `ACharacter` 的 `CharacterMovementComponent`、`bReplicates=true`、`SetReplicateMovement(true)` 和标准 Autonomous Proxy 预测；只通过配置限制速度、平面移动和碰撞通道，不重写 Transform 同步。
- 远端玩家由引擎处理平滑复制；项目代码只负责朝向/动画状态等表现变量，不能自己再实现一套位置广播和插值协议。
- 怪物则完全相反：只在 Server 上真实模拟（AI、移动、碰撞），Client 只接收 Replicated 状态做插值渲染，任何 Client 都不本地模拟怪物的真实行为。

### 3.2 角色与移动（M1）
- `ABaseCharacter : public ACharacter`，关闭 Orient Rotation to Movement，改为面向鼠标方向（双摇杆射击标准做法）。
- M1 阶段用占位几何体（Capsule/Cube），先验证移动/交互逻辑。
- `UInteractionComponent`：朝向鼠标方向做短距离检测（Sphere/Line Trace），检测到 `IInteractable` 时显示交互提示，按键触发 Server RPC。

### 3.3 战斗系统（M2）
- `AWeaponBase`：`Fire()` 先在本地 Client 播放可撤销的表现（枪口特效/后坐力抖动），同时发起 `ServerFire(FireRequest)`；Server 校验射速、弹药、武器身份和发射者位置后做 LineTrace。Server 结果通过复制事件/Multicast 广播，客户端表现不得反过来决定伤害。
- Server 侧对怪物做 LineTrace/Hitscan 命中判定（怪物位置是 Server 权威，判定天然准确），命中后应用伤害并 Multicast 广播命中效果（见 3.1 表格）。
- 弹道模型先用 Hitscan 实现，抛射体作为后续可选加项。

### 3.4 背包与道具系统（M3）
- `FInventoryItem`（运行时状态）+ `UItemDataAsset`（静态配置：图标/体积/类型/基础数值）分离。
- `UInventoryComponent`：增删/堆叠/校验容量全部由 Server 执行；M3 再引入 Fast Array 或受控属性复制，不在 M0-M2 预埋背包协议。

### 3.5 搜刮/容器系统（M4）
- `ALootContainer`：持有 `LootTableID`，**首次被打开时**在 Server 侧按 `DT_LootTable` 生成内容并写入自身 `UInventoryComponent`，避免多人同时开箱看到不同结果的同步问题。

### 3.6 基地与局外成长（M5）
- 基地是独立 Level/Sublevel，承载：
  - 仓库（Stash）：跨局持久化的 `UInventoryComponent`，数据来源于 SaveGame。
  - 基地功能模块：`FHideoutModule`（等级/解锁条件/解锁效果），先定 2-3 个（如仓库扩容、武器台）跑通框架，其余后补。
  - 升级判定用"消耗仓库物品 + 立即完成/简单等待"模型即可。

### 3.7 武器装配系统（M6）
- `UWeaponBase` 持有 `TMap<EAttachSlot, FWeaponPartData>`（枪管/瞄具/枪托/弹匣等槽位）。
- `FWeaponPartData` 来自 `DT_WeaponParts`：对后坐力、精度、噪音半径、弹匣容量做数值修正，**只做数值层，不接外观**。

### 3.8 怪物生成与 AI（M7）
- `AEnemySpawnDirector`：按玩家位置/区域 Tag，依据 `DT_EnemySpawnTable` 触发生成，控制同屏数量上限。
- 每只怪物：`AEnemyBase` + `AAIController` + `UBehaviorTree`：
```
Root
 ├─ Selector: 状态优先级
 │   ├─ Sequence: 已警觉 → 索敌/追击 → 进入攻击范围 → 攻击
 │   ├─ Sequence: 听到/看到玩家 → 切换为"警觉" → 呼叫附近同类
 │   └─ Sequence: 默认 → 按 PatrolPoints 巡逻
```
- 感知用 `UAIPerceptionComponent`（视觉+听觉），呼叫增援用 `UAISense_Hearing::ReportNoiseEvent` + 半径内同 Tag Actor 查询即可，不需要额外事件总线框架。

### 3.9 撤离与地图流程（M8）
- 一张手搭的固定大地图（参考塔科夫）：核心区域完全手工摆放固定；外围空地放置若干 `ACampSpawnPoint`，Server 在每局开始时从预设模板池中随机选取一部分启用，实现"同一张地图每局体验略有不同"，且不需要做真随机地形生成。
- 插入点/撤离点用 Actor 标记 + `DT_ExtractionConfig`（类型：固定/条件触发/呼叫倒计时）配置，策划侧可直接在关卡里摆点。
- `AExtractionRaidGameMode` 驱动倒计时/撤离成功判定/团队部分撤离规则，判定全部在 Server。

### 3.10 存档系统
- `UExtractionSaveGame : public USaveGame`：持久化仓库内容、基地升级状态、已解锁装备/部件。
- 只在"回到基地场景"时读写一次，局内数据走内存态/Replication，撤离结算成功后 merge 进 SaveGame 落盘。

---

## 4. 美术呈现方案（M9 落地）

- **原则：底层渲染方案维持骨骼 Socket 部件堆叠，暂不做的是 Mod 制作工具链（面向第三方/社区的资源导入校验、文档等），这部分不影响本项目自己使用这套系统。**
- 角色渲染：用一套**极简共享骨架**（只做骨骼不做蒙皮网格）驱动动画；每个部位是挂在对应 Socket 上的面片（Plane Mesh），材质接一张贴图（法线贴图/Lumen 精细光照作为有余力再做的加分项，不是必做项）。
  - 骨架规范：
    ```
    Root → Pelvis → Spine → { Chest_Socket, Head_Socket, Backpack_Socket, WeaponHand_Socket, Shoulder_L/R_Socket }
    Pelvis → Leg_L / Leg_R
    ```
  - 换装数据结构（本项目内部使用，不做面向 Mod 作者的工具链）：
    ```
    DataAsset: DA_CharacterPartSet
      - BaseSkeleton: SK_Humanoid_Master        // 固定引用共享骨架
      - AnimBlueprint: ABP_Humanoid_Master       // 固定引用共享 AnimBP
      - Parts: Map<SocketName, Texture>          // 先只挂 Diffuse 贴图，法线贴图后补
    ```
  - 动画（走/跑/攻击/受击/翻滚）只做一次，所有角色/怪物外观差异都是"换贴图"，符合"AI 批量生成一批贴图直接怼上去当占位"的用法——先把整套系统跑通、贴图质量粗糙也没关系，游戏整体做完后再决定是否精修贴图或补法线贴图/光照细节。
- 地图：3D diorama 场景（简单几何体 + AI 生成贴图），保留倾斜镜头/浅景深做出"HD-2D 式"的空间感。
- **本阶段不做**：面向社区/第三方的 Mod 导入校验工具、Mod 制作文档 —— 这属于第 6 节里 M10（可选打磨阶段）之后的事，视时间决定是否投入。

---

## 5. 关键数据结构一览

```
DT_LootTable        : ItemID, Weight, MinMax数量, 区域Tag
DT_EnemySpawnTable  : EnemyClass, 区域Tag, 生成权重, 强化系数
DT_ExtractionConfig : ExtractID, 类型(固定/条件/呼叫), 触发条件, 持续时间
DT_WeaponParts      : PartID, 挂载槽位(EAttachSlot), 数值修正
DA_ItemData         : 单个物品的静态配置（体积/类型/图标/基础数值）
DA_CharacterPartSet : 角色部件贴图配置（见第4节，内部使用，不含Mod工具链）
FHideoutModule      : 基地功能模块（等级/解锁条件/解锁效果）
UExtractionSaveGame : 仓库内容、基地状态、已解锁内容
```

---

## 6. UE MCP 协同开发工作流

- UE5.8 内建实验性 Unreal MCP 插件（`ModelContextProtocol` + `AllToolsets`，控制台 `ModelContextProtocol.StartServer` 启动），可在编辑器内直接批量创建 C++ 类骨架/蓝图、按 DataTable 结构灌测试数据、摆放关卡标记点。
- 建议流程：按 M0→M8 逐条拆成任务 → 落地实现 → 每个里程碑跑一遍本地双开联机测试，避免网络问题堆到后期一起爆发。
- 官方标注该插件为实验性功能，遇到"连接成功但无响应"优先检查 Toolset Registry 是否手动启用。

---

## 7. 交给后续技术文档细化的接口点

以下内容本文档只给出方向性设计，具体协议/参数建议留给后续技术文档（如网络协议定义）阶段细化：
- 玩家状态低频广播的具体频率（建议起点 10-15Hz）、字段的具体位宽/编码方式。
- 简化动画状态枚举的完整清单（走/跑/开火/受击/翻滚/死亡等，及其互斥/优先级关系）。
- 各 RPC 的具体函数签名、Reliable/Unreliable 选择（建议：位置广播用 Unreliable，攻击事件/伤害结算用 Reliable）。
- `DT_EnemySpawnTable`、`DT_LootTable` 等表格的具体字段类型与取值范围。
