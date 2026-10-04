# Face Lighting public API V1

实现日期：2026-10-01。供收藏轮盘及其他 SKSE 插件调用；当前为本地开发构建，尚未随新的公开安装包发布。接口头文件为 `include/FaceLightingAPI.h`，不依赖 CommonLib、SKSE 头文件或链接库。

## 获取接口

Windows x64，使用默认 8 字节结构对齐。不要在 `#pragma pack(1)` 等自定义布局下包含头文件。跨 DLL 只传固定宽度整数、POD、调用方缓冲区；不传引擎对象、STL 容器、分配所有权或异常。

```cpp
#include <Windows.h>
#include "FaceLightingAPI.h"

const FaceLightingAPI::Interface* FindFaceLighting() noexcept {
    const auto module = GetModuleHandleW(L"FaceLighting.dll");
    if (!module) return nullptr;
    const auto getAPI = reinterpret_cast<FaceLightingAPI::GetAPI>(
        GetProcAddress(module, FaceLightingAPI::exportName));
    if (!getAPI) return nullptr;
    const auto api = getAPI(FaceLightingAPI::version);
    return api && api->apiVersion == FaceLightingAPI::version &&
        api->structSize >= sizeof(FaceLightingAPI::Interface) ? api : nullptr;
}
```

在 SKSE PostPostLoad 后发现接口；若模块或接口缺失，正常禁用对应功能。不要用 LoadLibrary 加载游戏插件，也不要 FreeLibrary 卸载它。返回的只读函数表有效至进程退出。GetAPI 可以在任意线程调用，不支持的版本返回 nullptr；V1 的其他函数全部要求游戏主线程，例如 SKSE AddTask 回调。绘制线程只读取调用方缓存。

插件通过玩家更新 hook 确认实际游戏线程；确认前调用返回 WrongThread，之后尚未进入存档或正在加载时查询角色返回 NotReady。GetContext 在游戏线程上仍可返回 ready=false。每次读档、新游戏、退出至主菜单改变 session，旧目标与命令失效；不能持久化 token 或跨存档复用。

## 函数与命令

| 函数 | 行为 |
| --- | --- |
| GetContext | 会话号、就绪状态、配置 revision、玩家与随从组开关 |
| CaptureTarget | 读取当前准星上的有效非玩家 NPC，返回目标 token、姓名和状态；没有控制台/旧目标回退 |
| QueryPlayer | 查询玩家主开关与运行状态 |
| QueryActor | 重新解析 token 并核对角色类型与 FormID；死亡或失效模型可作为 Unsafe/未加载状态返回 |
| EnumerateFollowers | 查询现有随从名单，支持调用方缓冲区分页 |
| Execute | 同步提交 SetPlayer、SetActor、SetFollowerGroup 显式开关命令 |

`structSize` 使用头文件默认值，每个输出行也要初始化。保留字段必须为零，enabled 只接受 0/1；输出和请求内存由调用方维护。失败不发布部分输出；调用方应在非 Ok 时丢弃原输出。

SetPlayer 改保存的玩家主开关，SetFollowerGroup 改随从来源总开关。二者写 INI，保留其他已保存配置。SetActor 不移除名单：普通 NPC 开启时添加/启用指定行，队友开启时使用随从个人偏好。已有指定记录和随从偏好同时更新；新指定角色也同步旧随从偏好。关闭未登记普通 NPC 是 NoChange，不新增记录。

开启个人灯不会开启整个来源组。队友要求随从组开启，普通 NPC 要求指定组开启，否则返回 SourceDisabled。关闭允许在组关闭时执行。名单/偏好容量不足时整个个人操作不提交，返回 ListFull；个人偏好仍通过保存游戏的 SKSE co-save 持久化。API 不额外显示通知，调用方负责反馈。

Execute 返回 Ok 表示偏好已提交，NoChange 表示已是所需状态。实际场景更新通过现有游戏线程任务发生，不保证该帧已经创建光源，更不证明引擎最终照明贡献。死亡、潜行、第一人称、自动环境控制、对话独占和次要灯预算继续生效；个人关闭也不否决当前对话来源。

## 目标与 revision

目标由 session、引擎句柄、FormID 组成。调用时重新解析并验证身份，不凭 FormID 重新选择其他角色。SetActor 还要求角色存活、启用、3D/所属 cell 已加载；查询可以报告不满足这些条件的角色状态。

同时亮灯预算现可通过菜单/INI 调整为 1～32，默认 4；通用页环境检测频率为 1/0.5/0.2 秒三档下拉框，玩家与对话共用。V1 函数表布局保持不变，配置 revision 包含预算和频率设置。

revision 是当前配置/偏好的不透明指纹，不是递增计数。只比较相等，不排序、不持久化。配置恢复相同值后指纹可以相同；它不是历史操作编号。SetPlayer/SetFollowerGroup 使用 GetContext 的 revision；SetActor 使用对应 ActorState 的 revision。角色指纹包含其来源身份和双来源偏好，不随过渡动画或临时隐藏每帧改变。

轮盘应在打开前捕获准星；玩家或角色命令应携带显示时的状态和 revision。在关闭轮盘、恢复游戏后执行明确的新状态。状态已被其他操作改变时返回 StaleRevision，刷新并交由用户重新选择，不自动重试旧开关。

```cpp
// Game thread, immediately before opening the wheel.
FaceLightingAPI::ActorState shown;
const auto captured = api->CaptureTarget(&shown);

// Keep shown in the pending action. Game thread, after closing the wheel.
if (captured == FaceLightingAPI::Result::Ok) {
    FaceLightingAPI::Request request;
    request.command = FaceLightingAPI::Command::SetActor;
    request.target = shown.target;
    request.revision = shown.revision;
    request.enabled = (shown.flags & FaceLightingAPI::Enabled) ? 0u : 1u;
    const auto result = api->Execute(&request);
    // Display the result in the client's own notification/status UI.
}
```

这个示例的两个阶段必须分开运行；执行前还需由轮盘保留其原有会话、截止时间和暂停恢复检查。API 会拒绝主菜单、加载、控制台和暂停中的操作。面光配置菜单打开或存在预览时返回 BusyPreview，避免外部写入覆盖未保存设置。

## 随从分页

先 GetContext 获得 session。FollowerPage 的 revision=0 表示新查询；capacity=0、rows=nullptr 可以查询 total/revision。再分配默认初始化的 ActorState 数组，设置 rows/capacity/offset 和返回的 revision。每页最大 capacity=4096，offset 可以等于 total；此时 count=0。没有足够容量时正常返回部分行，用 offset+count 继续。

分页 revision 覆盖所有行的身份、姓名、加载/安全状态、个人偏好与配置，任何变化返回 StaleRevision，调用方丢弃已收集页面后重新查询。暂时隐藏、过渡与场景注册不冻结，分页只能保证名单/偏好的一致性。随从名单沿用现有约 500ms 扫描，不强制扫描世界或加载 cell；刚离队/死亡的行可能短暂保留，需检查当前 Follower/Unsafe/Loaded。失去对象的行保留名字与 FormID，handle=0，不能执行角色命令。

## 状态语义

| 标志 | 含义 |
| --- | --- |
| Enabled | 玩家主开关，或 NPC 任一已存在个人来源偏好开启；不表示当前可见 |
| GroupEnabled | SetActor 使用的主来源组：队友为随从组，其他 NPC 为指定组；玩家始终有此位 |
| Follower / Selected | 当前队友身份 / 存在指定记录 |
| FollowerEnabled / SelectedEnabled | 当前队友个人偏好 / 指定行分别开启 |
| FollowerGroupEnabled / SelectedGroupEnabled | 两种来源总开关；用于解释重叠角色的状态 |
| Loaded | 当前角色有 3D，所属 cell 已附加 |
| Registered | 本模组持有该角色场景光对象；不表示已确认照亮 |
| Fading | 已注册光源的过渡透明度在 0 与 1 之间 |
| HiddenSneak / HiddenDialogue / HiddenView / HiddenAmbient | 当前规则抑制原因，可以多项同时出现；HiddenAmbient 也用于开启环境控制后被抑制的对话对象 |
| Dialogue | 当前有效对话灯来源 |
| NotAllocated | 当前应有来源但没有 NPC 管理项，可能是预算、候选上限或更新尚未发生；不是引擎丢灯检测 |
| MissingModel | 已加载但所用头节点/第一人称相机节点缺失，或缩放非法 |
| Unsafe | 角色不满足安全照明条件 |

状态不提供单一“成功照亮”位。强度为零、总开关关闭等也可能让 Registered=false；不要把所有未注册状态都解释为创建失败。名字为最多 255 字节的 UTF-8，以完整字符边界截断并以零结尾。

## 返回值与验证

Ok/NoChange 为已提交/无需修改；InvalidArgument 为 ABI 参数错误；WrongThread/NotReady 为线程或时机错误；StaleSession/StaleRevision 为过期操作；InvalidTarget/NotLoaded 为对象不可操作；SourceDisabled/ListFull 为来源关闭或容量不足；SaveFailed 为 INI 保存失败；BusyPreview/Blocked 为当前菜单或游戏状态不允许操作；InternalError 为捕获的内部异常。

已完成 DLL 导出/版本协商/ABI 缓冲区与线程入口检查、UTF-8 截断、会话 token 校验及双来源原子提交测试。既有 ActorRuntime 回归覆盖 1.5.97、1.6.353、1.6.629、1.6.1170 的原生生命状态布局。尚未在实际游戏客户端里验证 API 调用、暂停恢复、跨存档 token 和随从分页；轮盘接入后按 `docs/favorite-wheel-integration-design.md` 联测，不据此宣称全部运行时已实测。
