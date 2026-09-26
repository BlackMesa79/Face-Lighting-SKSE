# 直接排除面光的受光查询：研究记录

日期：2026-09-20。结论：找到明确候选函数和公开实现先例，值得进入独立诊断原型阶段；尚未证明在用户当前 CS 构建中可以安全取得完整的过滤后环境值。

本轮没有修改运行时代码、重新编译或部署，也没有改变用户补偿设置。原有 AmbientMode 0/1/2 与固定补偿方案保留。用户确认补偿约 50 时基本能按早晨、中午、夜晚、火把及室内环境切换，但跨场景仍需调整，不能据此断言 raw 必然使用某一种非线性聚合公式。

## 已保留的基线

实际部署 DLL 和 MO overwrite 的有效 INI 已复制至 `build/ambient-research/baseline-compensation/`。

- DLL SHA-256：`840377C315E874AEDC7EA5BD94D7E49C43ABAE2A38E2501CD27C25A2F7654803`，与部署文件一致。
- 实际 INI：AmbientMode=2，AmbientCompensation=50.200001，开灯阈值 30，关灯阈值 50，延时 2 秒。
- 原项目默认补偿值仍为 80，用户 INI 中的 50.2 未被覆盖。本次记录不把个人校准值升级为通用默认值。

## 证据与可信范围

### 1. 单盏灯贡献计算入口

[CS dev InverseSquareLighting.cpp](https://github.com/community-shaders/skyrim-community-shaders/blob/dev/src/Features/InverseSquareLighting.cpp) 安装 BSLight_GetLuminance 钩子，地址库 ID 为 SE 101303／AE 108292。参数包含 BSLight、目标位置和参考 NiLight。代码能跳过与参考灯相同的对象；逆平方分支根据距离、范围、光源大小、颜色与 fade 计算，并写入 BSLight.luminance。普通灯则委托原函数。

这证明存在逐灯计算和单灯排除的切入点，也说明直接调用该函数不一定无副作用。它不证明只在角色侦测中被调用，也不证明最终 raw 是这些返回值的简单求和。CS 的 Disabled 标志同时被渲染处理使用，不能作为“只排除检测、保留照明”的开关。refLight 只容纳一个对象，不能自然覆盖玩家、随从、指定和对话的全部面光。

本地 `build/cs-reference/jiaye-dev/InverseSquareLighting.cpp`、`public-1.8.4/InverseSquareLighting.cpp` 也有该函数形态；这些是研究参考源码，不是用户所装二进制与该提交完全一致的证明。

### 2. 上层查询和角色计算

[Open Shaders 固定提交 46ecbdd](https://github.com/alandtse/open-shaders/blob/46ecbdd1b449d8b81d2bc1825f7972adc75149a8/src/Features/LightLimitFix/ShadowCasterManager.cpp) 的 3220–3281、3663–3690 行提供进一步依据：

- 角色计算候选：AIProcess::CalculateLightValue，SE 38900／AE 39946。
- 整体位置查询：ShadowSceneNode::GetLuminanceAtPoint，SE 99725／AE 106362。
- 源码为阴影调度适配重新检查影响玩家的阴影灯，并在整体查询内逐灯筛选。
- IsLightAffectingActor 的 SE 41661／AE 42744 是另一处相关调用，不能直接当作一般环境亮度查询。

这是一份历史实现先例，不应把它的寄存器、偏移、全局临时集合或字节补丁照搬到当前插件。其过滤对象是阴影灯，当前面光使用非阴影点光，所以还必须确认非阴影灯实际经过哪个分支。该历史代码也不等于用户当前启用了 Open Shaders 或同一阴影调度器。

下载的固定提交源码 SHA-256：`48B9F01386F9B16035B5406D487EBE280140E7537DC382D96905D33BA808C41D`，存于 `build/ambient-research/ShadowCasterManager-46ecbdd.cpp`。当前 main 的结构已不同，不能用 main 页面替代该历史证据。

### 3. 本机地址核对

本机 SkyrimSE.exe 版本为 1.6.1170.0。解读其对应地址库后得到下表，并用本地 Skyrim PDBs 的公开符号名称交叉参考：

| 候选 | AE ID | 1.6.1170 RVA | 本地符号名称 |
|---|---:|---|---|
| Actor 取值入口 | 37775 | 0x6914C0 | Actor::GetLightLevel |
| ActorProcess 取值入口 | 39509 | 0x6EC8B0 | ActorProcess::GetLightLevel |
| 角色计算候选 | 39946 | 0x713710 | FUN_140713710 |
| 整体位置查询 | 106362 | 0x14A3A60 | ShadowSceneNode::GetLuminanceAtPoint |
| 单灯贡献 | 108292 | 0x1509E30 | FUN_141509e30 |

公开符号名称不是完整 ABI 或调用关系证明。本次未完成 PDB GUID 身份核验。磁盘 EXE 对应代码区未得到有效的直接反汇编，工具没有找到调用者不能解释为“没有调用者”。没有读取运行中的游戏代码（研究时游戏未运行），未修改或启动游戏。SE 与其他 AE 版本的具体调用点还未核验。

本地研究脚本和结果在 `build/ambient-research/`，Python 分析依赖仅装入 `build/research-python/`；它们不进入模组发布包。

## 推荐方案：独立的过滤后读数

目标是在完整环境检测路径中按对象身份排除本插件的全部灯光，并把结果存入插件自己的采样状态。保留游戏原始缓存值，用于对照及既有游戏行为。不能仅因找到单灯函数就全局返回零，否则可能影响潜行、其他模组读数或灯光排序。

推荐分两步：

1. 先观测调用链。针对核验过的运行时，记录角色计算／整体查询／单灯计算的线程、调用来源、目标位置、是否属于本插件、调用次数与结果。必须确认光照缓存的实际写入时点、昼光／环境项、阴影筛选以及 CS 钩子顺序。限制采样量，正常处理路径仍委托原函数。
2. 确认边界后，再研究在同一采样位置与同一场景状态下计算原始／过滤两个结果。只在插件自己的受控查询作用域内排除灯光；不能覆盖 HighProcessData.lightLevel，不能修改 scene activeLights、fade、颜色或渲染 Disabled 标志。重复查询若会改写 luminance、侦测状态或其他共享缓存，需要先隔离副作用；无法隔离时应在经过验证的聚合环节维护并行结果，而不是直接重入整个 AIProcess 更新。

光源身份以本插件持有的 NiLight 对象登记为准，覆盖玩家和全部 NPC 来源。不要只匹配名字，因为当前所有 NPC 灯复用 DialogueLight 名称；不要只保存 FormID，因为每盏合成灯没有独立的 TESObjectLIGH。登记必须配合引用生命周期与线程同步，移除时不能留下悬空指针，也不能长期持有导致灯永不释放。

若使用线程局部查询作用域，必须先确认计算在同一线程同步完成；跨线程任务不能靠 thread_local 自动传播。不要在多线程可共享的灯光列表上临时删灯或将灯强度归零。非面光计算继续通过已有 CS／引擎链路，不能另写普通衰减公式取代 CS 的逆平方规则。

## 新原型的验收门槛

- 第一阶段只展示原 raw 与 filtered 值，旧补偿模式不使用 filtered，模式编号与保存字段继续兼容。
- 同一暗处开关玩家灯、改变强度／位置：raw 可以变，filtered 应保持接近。
- 逐步启用随从、指定和对话灯：这些灯也被排除，不能仅排除玩家自身。
- 走近火把、白天／夜晚、室内明暗变化：filtered 仍响应外部照明。
- 验证原始缓存／潜行侦测／画面照明与基线一致，读档和移除灯光没有残留。
- 查询不支持、钩子冲突、线程或数据无效时明确显示不可用，不返回零，不自动把它当成黑暗。
- 先固定 1.6.1170 + 用户当前 CS 构建验证，其他版本单独核验。成功排除本模组光源也不代表 filtered 等同于曝光、间接光及后处理后的视觉亮度。

## 本轮结论

可行性从“泛泛寻找引擎接口”推进到“有地址库 ID、符号交叉参考及逐灯过滤先例的具体候选方案”。尚未闭合的关键点是完整调用链、非阴影灯分支、共享状态副作用和当前 CS 的兼容性。因此本轮保留既有已测版本，不部署未经验证的全局灯光过滤钩子。下一实现阶段应是上述调用链诊断，而不是直接替换现有自动开关模式。
