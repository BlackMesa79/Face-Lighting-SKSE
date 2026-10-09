# Face Lighting SKSE API V2 — 中文接入说明

更新日期：2026-10-09。API V2 纳入 0.9.7；用户确认开发构建游戏内测试未发现问题，接口与逻辑有自动测试。CCC 调用方接入后的联测仍待进行，不将本地回归视为 CCC 已完成联动。旧 0.9.6 下载包不含 V2。英文发送版见 [public-api-v2-en.md](public-api-v2-en.md)。

CCC 决定场景参与者、开关时机和颜色/亮度；面光模组拥有光源，负责去重、过渡、释放及恢复。CCC 可以直接调用可选接口，不要求它先提供自己的 API。

## 获取接口与 V1 兼容

复制 `include/FaceLightingAPI.h` 和 `include/FaceLightingAPIV2.h`。头文件只依赖标准固定宽度类型和类型特征，不依赖 CommonLib、SKSE 或面光导入库。

```cpp
auto module = GetModuleHandleW(L"FaceLighting.dll");
auto getAPI = module ? reinterpret_cast<FaceLightingAPI::V2::GetAPI>(
    GetProcAddress(module, FaceLightingAPI::exportName)) : nullptr;
const auto api = getAPI ? getAPI(FaceLightingAPI::V2::version) : nullptr;
if (!api || api->apiVersion != 2 ||
    api->structSize < sizeof(FaceLightingAPI::V2::Interface)) {
    // 模组未安装或接口不可用时，照常运行，不接管面光。
}
```

SKSE PostPostLoad 后发现接口；不要自行 LoadLibrary/FreeLibrary。`FaceLighting_GetAPI(1)` 继续返回原 64 字节 V1 表，布局与调用方式不变，收藏轮盘无需修改。用 **V2::GetAPI** 请求版本 2，取得独立的 80 字节表，其中 `v1` 指向原 V1。未知版本返回空指针。

获取函数表可在任意线程；所有表内函数必须在游戏主线程调用，例如 SKSE 主线程任务。绘制线程只读取自己的缓存，不传递 Actor 裸指针。结构按默认值初始化，保留字段为零，调用期间缓冲区必须有效。输入目标立即复制；目标数组始终使用头文件中 `Target` 的固定步长，较大 structSize 不改变步长。没有跨 DLL STL 对象或回调。

## 会话接口

| 接口 | 含义 |
| --- | --- |
| `v1->GetContext` | 查询当前存档/世界代次，必须检查 ready。 |
| `ResolveActor` | 用运行时角色引用 FormID 得到 token，支持玩家 0x14。不传 NPC 基础对象 ID，不加载远处 cell。 |
| `BeginSession` | world 填 context.session，创建空会话；成功前不改光源。当前最多一个外部会话，重复占用返回 Blocked。 |
| `UpdateSession` | 完整替换 0～4 个目标（包含玩家名额），全部验证后提交；成功同时续租。 |
| `RenewSession` | 保留目标与暂停状态，只续租。 |
| `EndSession` | 撤销临时请求并立即释放临时光源，下一次更新恢复正常来源需求。 |
| `QuerySession` | 返回暂停状态、目标数量和剩余租期，不续租；失效角色被清理后数量可能减少。 |
| `GetEnvironment` | 获取玩家位置的可用环境读数与有效标志，不进行世界扫描或强制刷新缓存。 |

Begin 默认租期 5 秒，可设 1～30 秒。建议每秒续租一次，交易或暂停时也要续租；按单调墙钟时间计时，不按游戏时间。超时后返回 StaleSession，重新 Begin 并解析目标。读档、新游戏、返回主菜单、加载界面会清理会话。死亡、禁用、卸载、头骨无效的角色从快照中移除，不强制加载或无限重试。

暂停用完整 Update 快照加 paused=1；恢复时重新解析参与者再发送 paused=0。暂停、空快照、End 立即释放临时光源，按原规则恢复普通来源。暂停快照仍检查目标，旧目标失效时可发送空暂停快照，恢复时重建。配置菜单/预览暂时压制外部渲染但保留租约；Begin 与未暂停 Update 返回 BusyPreview。游戏暂停/控制台下未暂停写入返回 Blocked；暂停、续租、结束、查询仍可在主线程执行。

CCC 必须在对话结束、场景中断、镜头所有权丢失和禁用联动时调用 End；超时仅是漏释放兜底，场景清理依赖游戏主线程再次更新。不要把 token 保存进存档。Ok 只代表请求提交，不代表引擎已显示光照。

如果希望临时对话不触发玩家已有“进入对话自动开灯”写入，要在 DialogueMenu 开启边沿前 Begin。已发生的自动写入无法由 V2 撤销。已由租约控制的对话会抑制玩家自动开关写入；即便会话提前结束，本次对话随后关闭也不会改写玩家开关，下一次普通对话恢复原逻辑。

## 参数与优先级

每个目标传完整 `LightParameters`：

| 字段 | 范围与解释 |
| --- | --- |
| enabled | 0/1；关闭仍压制当前临时场景中的普通 NPC 来源。 |
| intensity | 0～5，0 表示关闭。 |
| radius | 10～500 游戏单位，普通衰减和 CS 逆平方均使用手动范围。 |
| colorMode | Temperature 或 SRGB。 |
| temperature | 2000～10000 K。 |
| red/green/blue | 0～1 sRGB，按用户 CS 线性设置转换。 |
| offsetSpace | ActorHeading（角色朝向）或 HeadBone（头骨局部）。 |
| offsetX/Y/Z | −150～150 游戏单位，锚定标准头骨；角色朝向空间为右/前/上，骨骼空间排除骨架缩放。 |
| transitionSeconds | 0～3 秒，默认 0.2；只控制发光淡入淡出，颜色/强度/位置变化立即应用。 |

所有数值都要有限且在范围内，包括当前颜色模式未使用的字段；未知枚举、重复角色、失效 token 或目标不合法导致整组失败，不部分提交，也不续租。

未暂停且有目标的会话接管 NPC 场景：临时来源优先于默认对话、随从和指定来源，其他普通 NPC 灯临时关闭。不依赖 DialogueMenu，因此可处理 NPC↔NPC。每名角色仍只创建一盏灯。最多四名参与者不占随从/指定预算，但仍消耗引擎光源资源。玩家在快照中时临时覆盖玩家参数与开关；未列玩家则沿用普通玩家需求。

临时操作不写 INI、指定名单或随从偏好，结束/暂停/空快照/超时后恢复正常需求。显式 enabled=0 会使用最后发光参数淡出；从快照删除目标则在独占场景中立即释放。End 和安全清理也立即释放，如需柔和结束，先将目标关灯、继续保活等待过渡，再 End。

潜行隐藏、死亡/模型安全及第一人称许可始终优先。临时灯不受普通玩家/对话环境门控限制，由 CCC 自己判断是否提交。CS 全局适配沿用用户设置；NPC 继承对话 CS 参数，玩家继承玩家 CS 参数。没有引擎丢灯检测。仍为头部全向点光源，相机空间、任意骨骼和单向灯不在 V2 范围内。

## 环境读数边界

GetEnvironment 返回玩家位置，而不是 NPC 脸部或镜头位置。分别检查 RawValid/FilteredValid；Ok 但没有有效标志是允许的。raw 包含模组灯光，缓存年龄未知。filtered 仅在启动时已启用现有实时排除 hook、正在采集且有有效样本时提供，目前仅 1.6.1170。接口不修改配置、不临时安装 hook、不悄悄回退到 raw。数据不足时保留判断或使用 CCC 自己的环境逻辑，不能视为真实物理亮度或直射阳光测量。

V1 ActorState 的 Enabled 等标志仍描述保存的偏好，不代表 V2 临时开关；Registered/Fading 表示模组管理的运行时光源，不能证明引擎实际照亮。

## CCC 需要做的工作与本地验收

1. 可选发现 V2，未安装时正常工作，并在 CCC 中提供联动开关。
2. 场景开始 Begin，按引用 FormID 解析参与者，整组提交。玩家↔NPC 包含玩家，NPC↔NPC 可只传 NPC。
3. 发言者/镜头变化时替换完整快照，可自行结合环境调颜色和亮度。
4. 主线程定时保活，交易/预览时暂停，恢复时重新解析。避免每帧强行重试 BusyPreview/Blocked/NotLoaded。
5. 正常/异常退出与 CCC 禁用都 End；换档丢弃 token。

可编译示例：[ccc-face-lighting-v2.cpp](examples/ccc-face-lighting-v2.cpp)。应联测双方对话、两名 NPC 对话、快速切镜、RGB、第一人称、潜行、死亡、卸载、读档、超时、交易恢复，以及已有个人灯/对话灯的恢复；普通功能和收藏轮盘 V1 也要回归。
