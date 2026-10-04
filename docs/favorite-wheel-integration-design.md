# 收藏轮盘对接方案

研究日期：2026-10-01；2026-10-03 状态补充：用户确认收藏轮盘接入后测试正常。下面保留接口设计与接入检查要点作为参考，轮盘侧当前实现以其项目代码为准。接口契约与示例见 [public-api.md](public-api.md)。

## 当前代码基础

面光本地开发 DLL 仍标记 0.9.3，已增加公共 DLL 接口，不能与早先打包的 0.9.3 发布包混淆。游戏 DLL 与本地构建一致；旧公开安装包未重新制作。

收藏轮盘路径：`D:/MyFiles/Documents/ChatGPT/SKSE模组 收藏轮盘`。
当前 xmake 和 src/main.cpp 为 0.2.7，README 标题仍为 0.2.1，以代码为准。仅允许 Skyrim 1.6.1170。该项目工作目录目前全部显示为未跟踪文件，实施时应保留所有已有内容。

- `src/Wheel.cpp` 已有功能模式，但 RefreshFunctions 仅取 Outfits::Entries。
- `include/Favorites.h` 已有 ActionKind，区分收藏和套装命令，适合扩展为面光命令。
- `PendingAction / PumpAction` 支持关闭轮盘、等待恢复游戏、校验 generation 和截止时间，然后通过 SKSE 主线程执行。
- `include/TextBridgeClient.h` 已用 GetModuleHandle / GetProcAddress 查询可选插件，无需硬链接。
- `src/Draw.cpp` 的功能标题、状态文本、数量统计与提示仍专门针对套装，不能只添加命令而不调整显示。

## 首版范围与操作

复用现有功能轮盘，保留套装功能，加入面光分组与随从子列表：

| 项目 | 行为 |
| --- | --- |
| 玩家面光 | 切换保存的主开关；自动环境控制、潜行与第一人称规则继续生效 |
| 指向角色面光 | 打开轮盘前捕获对象；未登记的普通 NPC 添加到指定名单并开启；已登记者切换个人偏好 |
| 随从面光 | 进入按角色显示的分页子列表，逐人切换，不关闭轮盘来切换分页 |
| 随从总开关 | 控制 follower.enabled，不清空或重置个人偏好 |

无需新增一套轮盘输入 hook。选择改变照明的命令后沿用关闭轮盘再执行的流程。列表导航应在轮盘内完成。

没有面光插件、旧插件不提供接口或版本不匹配时，不显示可执行面光命令；可以显示简短不可用状态，套装和收藏仍照常工作。不可调用 LoadLibrary 强行加载 SKSE 插件。

## 个人开关与角色重叠

需区分存储偏好、来源总开关及暂时隐藏：

- 两种名单重叠时，关闭个人面光要同时关闭该角色已存在的 follower preference 和 selected row；不移除角色。
- 开启时：队友优先启用随从个人偏好；普通 NPC 启用或添加指定行。存在两种记录时更新保持一致，防止离队后另一记录突然恢复。处理前先校验容量和写入条件，避免操作一半失败。
- 个人操作不偷偷打开全组总开关，否则可能让其他几十名角色一起恢复。组关闭时应返回 SourceDisabled 或显示“总开关关闭”，用户通过独立总开关恢复。
- 对话照明是临时来源，按现有对话设置覆盖个人来源。轮盘本身禁止对话期间打开，接口仍应明确：个人偏好不是阻止当前对话灯的永久否决权。
- 暗环境自动控制只控制玩家；轮盘“玩家开”不强行覆盖明亮环境关闭、潜行等暂时抑制。

应让面光管理这些规则，轮盘只发语义命令。不能在轮盘直接编辑 FaceLighting.ini、模仿快捷键或依赖 ConfigMenu 捕获的陈旧准星。

## 公共接口 V1

以下设计已在面光端落实。确切的结构、返回值和状态标志以 `include/FaceLightingAPI.h`、`docs/public-api.md` 为准，轮盘端待接入。

沿用轮盘已使用的可选 DLL 导出模式，在 PostPostLoad 或首次游戏就绪查询：

`extern "C" FaceLighting_GetAPI(requestedVersion)` 返回带 structSize、apiVersion、capabilities 的函数表。

已实现的函数表包含以下能力：

| 能力 | 用途与约束 |
| --- | --- |
| GetContext | 返回会话编号和就绪状态，不把插件已加载等同于已进存档 |
| CaptureTarget | 在打开轮盘前捕获句柄、名称和会话号 |
| QueryPlayer / QueryActor | 返回保存开关、组状态、暂时抑制原因、场景注册状态 |
| EnumerateFollowers | caller-owned 缓冲区分页/容量查询，提供稳定快照 revision |
| Execute | 显式 SetPlayer / SetActor / SetFollowerGroup 命令，附会话号与期望 revision |

接口边界使用固定宽度整数、明确 UTF-8 字符缓冲区和字节大小，不跨 DLL 传 std::string、std::vector、异常、CommonLib 类或内存所有权。不直接暴露 NiLight/BSLight 指针。

目标 token 用会话号 + 角色 handle 值 + 可校验的 FormID；handle 仅限当前游戏会话，不能作为存档数据。执行必须重新解析并核对身份、存活和 loaded 状态，避免 FormID 在其他存档/对象中复用。

状态查询和执行第一版限定游戏主线程，在 Open/RefreshFunctions/PumpAction 内调用；Draw 仅读取轮盘复制后的 View。不要在 D3D 线程访问演员或操作骨骼。

既有菜单的 Followers::SetEnabled 与 SelectedNPCs::SetEnabled 保持来源独立、排队更新的语义。API 的 SetPersonalNow 主线程核心直接复用相同名单/偏好存储，同步提交双来源个人开关；Execute 返回偏好提交结果，场景更新仍由 RequestUpdate 调度。

结果区分 Ok、NoChange、NotReady、WrongThread、StaleSession、StaleRevision、InvalidTarget、NotLoaded、SourceDisabled、ListFull、SaveFailed、BusyPreview、Blocked 等。所有函数表回调捕获内部异常并返回错误码。

设置预览期间宜拒绝外部改配置并返回 BusyPreview，不能无意中把菜单尚未保存的设置一起保存，或调用 Save 清除预览。

建议发送期望的新状态（Set）而非盲 Toggle，并检查查询 revision，避免双击或状态变化把开关切回。现有个人偏好需要保存游戏才持久化，设置总开关写 INI，轮盘提示沿用这个区别。

## 显示状态

灯亮图标表达“个人/主开关已开启”，附状态表达：

- 总开关关闭
- 暂时隐藏：潜行、对话、环境/视角；NotAllocated 只说明尚无管理项，不能直接断定预算或引擎丢灯
- 未加载
- 创建失败

rendererLight 非空只能命名为“场景已注册”，不能显示为“已确认照亮”。引擎最终光源丢弃检测仍然暂缓。

新增名字符号需进入轮盘 inventoryGlyphs 的字体字符汇总；长姓名保持现有截断和中心全文查看逻辑。功能类别统计不能继续简单地用 items.size()-2 推算套装数量。

## 实施顺序

1. 已完成面光公共头文件、版本/能力查询及操作核心，沿用当前自动灯光策略。
2. 已覆盖双来源关闭/恢复、组关闭、名单/偏好满、会话 token、UTF-8、实际 DLL 导出与线程入口测试；保存失败由既有设置测试覆盖，游戏内预览冲突等仍需客户端联测。
3. 轮盘增加 FaceLightingClient，复用可选模块发现，增加命令分派、目标 token 和个人列表。
4. 调整图标、状态、功能模式标题和字体字符汇总；渲染层只消费快照。
5. 分别构建/部署两个项目，验证未安装/旧面光的降级、目标走开/死亡/读档、列表分页、暂停恢复，以及套装工作流无回归。

首版联动只能宣称 1.6.1170，因为轮盘目前拒绝其他运行时。面光公共 API 的状态访问仍须保持 SE/AE 兼容，新增检查需覆盖 1.5.97。不要借联动顺带宣布轮盘 SE 支持。

## 与多光源的接口关系

V1 控制角色层“整组光源”开关，语义保持不变。未来 V2 用能力位增加光源预设/灯位操作，不要求老轮盘理解骨骼细节。避免把未来多灯 ID 混入现在的角色 FormID。
