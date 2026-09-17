# 面部光照 / FaceLighting

开发进度：统一 NPC 光源管理已通过用户测试。指定 NPC 面光及新版菜单已完成开发，等待游戏测试；使用与测试说明见 [指定 NPC 面光](docs/selected-npc-lighting.md)。随从和附近 NPC 自动模块尚未实现，正式 0.8.2 发布包保持不变。

以智能弹药管理（SmartAmmoManager）为基础建立的独立 Skyrim SE / AE SKSE 插件项目，当前版本 0.8.2。

## 渲染适配基准

优先以用户的 Jiaye Community Shaders 实验版整合适配和验收，同时参考 N 网公版，ENB 留到后期。本机 `35 - CommunityShaders_AIO` 的 DLL 显示 1.6.0.0，但用户确认这是带 PostProcessing、SSRT 的 Jiaye build，不能据版本号将其等同于公版 1.6。

0.3.0 已按公版 1.6.0、N 网公版 1.8.4 和 Jiaye dev 固定提交核对共用光源协议，并实现普通/逆平方衰减与线性光照标记。保留自有实时菜单和光源管理，不增加 Light Placer 依赖。源码对照结果与本地实验构建的差别见 [CS 适配记录](docs/CS-compatibility.md)。

修复了逐帧覆盖 CS 扩展数据的行为：初始化后仅在设置变化时提交光照参数，保留 CS 的其他标记，正确区分作用半径与光源尺寸。位置继续逐帧跟随。运行时读取已加载 DLL 版本及 ISL 元数据，识别 1.6.0/1.8.4 配合 ISL 1-3-0；其他组合退回普通补光。PostProcessing 文件存在时显示实验版提示。此检测不证明子功能已启用，也不证明二进制对应某个源码提交。

沿用 xmake、C++23、CommonLibVR（commonlibsse-ng）及 SKSE Menu Framework API。提供一个跟随玩家头部的白色、无阴影动态点光源，可调整半径、强度及三轴位置，不需要 ESP 或 Papyrus 脚本。

光源在第三人称下启用，第一人称下关闭。以 `NPC Head [Head]` 为锚点，偏移方向按玩家朝向计算：X 正值向右，Y 正值向前，Z 正值向上。偏移和半径使用游戏单位，位置不随骨骼缩放倍增。缺少标准头部节点的形态暂不显示光源。

玩家更新时刷新位置；菜单通过 SKSE 主线程任务应用预览。读档、加载画面、返回主菜单、切换 cell 或玩家模型更换时清理并按需重建光源。运行时光源不写入存档。它是常规点光源，范围内的附近物体也可能被照亮；支持色温与对话 NPC 补光，不投射阴影。

## 目录

```text
src/                         插件入口、配置与菜单实现
include/                     对应头文件
extern/CommonLibVR/          从原项目复制的依赖源码
extern/SKSEMenuFrameworkAPI/  从原项目复制的官方 API 头文件及许可证
tests/                       配置持久化、预览回退及位置换算检查
FaceLighting.ini             默认配置
xmake.lua                    构建入口
build/                       构建及打包输出，不提交版本库
```

依赖保留原项目的本地版本及许可证，不依赖原项目的绝对路径。未继承弹药事件、装备逻辑及存档序列化 ID。

## 构建

使用与智能弹药管理相同的 Windows x64、Visual Studio 2022 C++ 工具链及 xmake 环境：

```powershell
xmake f -p windows -a x64 -m releasedbg --toolchain=msvc --vs=2022
xmake build FaceLighting
xmake build SettingsTests
.\build\windows\x64\releasedbg\SettingsTests.exe
xmake build LightPlacementTests
.\build\windows\x64\releasedbg\LightPlacementTests.exe
xmake build CSLightingTests
.\build\windows\x64\releasedbg\CSLightingTests.exe
```

首次构建需要 xmake 的 DirectXMath、DirectXTK、spdlog 依赖（已有缓存可复用）。构建后安装目录为 `build/package/FaceLighting/SKSE/Plugins`，包含 DLL、PDB 及默认 INI。仅为 SE / AE 启用编译支持，VR 关闭。

当前自动部署通过本地 xmake deploy_dir 选项配置，详见仓库根目录 README。

## 安装与菜单

将 `build/package/FaceLighting` 作为模组目录安装，或将其中的 `SKSE` 文件夹放入游戏 `Data`。需要与游戏版本对应的 SKSE、Address Library，以及用于菜单的 SKSE Menu Framework（沿用原项目的 3.14.1 API 接入方式）。

菜单入口：**FaceLighting → 基本 / 玩家面光 / NPC 面光**。页面支持跟随 Windows 界面语言，以及手动选择英文或简体中文：

| 控件 / INI 键 | 默认值 | 范围或作用 |
| --- | --- | --- |
| Enable player face light / Enabled | 0 | 启用玩家补光 |
| Light radius / Radius | 100 | 10–500 |
| Intensity | 1 | 0–5，0 不发光 |
| Left / Right / OffsetX | 0 | -150–150，正值向右 |
| Back / Front / OffsetY | 40 | -150–150，正值向前 |
| Down / Up / OffsetZ | 5 | -150–150，正值向上 |
| Debug logging / DebugLogging | 0 | 保存后应用日志等级 |
| Inverse-square falloff (CS) / CSInverseSquare | 0 | 使用 CS 逆平方衰减 |
| Linear light intensity (CS) / CSLinear | 0 | CS 线性光照开启时跳过传统点光源转换/倍率 |

两个 CS 模式默认关闭，旧配置维持普通补光。普通模式的 Radius 仍独立可调；逆平方模式隐藏半径滑块，显示由 CS 公式计算的范围，原 Radius 值仍保留供切回使用。逆平方模式固定光源尺寸为 sqrt(2)、截断阈值为 0.05，强度为 0 时移除光源。范围可以超过普通模式的 500 上限；0.3.1 校准后强度 1 时约为 298.2 游戏单位。CS 的线性光照、后处理和实验特性会影响观感，应从当前强度逐步调整。

建议只使用 FaceLighting 菜单调整此光源，避免同时用 CS Light Editor 覆盖同一光源的参数。

滑块和启用开关即时预览。**Save settings** 将当前值写入 INI 并保留；**Restore defaults** 预览默认值，仍需保存；**Discard changes** 或关闭框架会回到已保存值。保存失败也撤回预览，保留上次成功配置。原 0.1.0 INI 缺少的新字段自动使用默认值；无效、非有限数值回退默认值，越界值限制在上述范围内。

配置路径为 `Data/SKSE/Plugins/FaceLighting.ini`，日志为 SKSE 日志目录下的 `FaceLighting.log`。框架缺失或缺少所需 API 时记录日志并跳过菜单，插件仍可加载。MO2 下游戏内保存的 INI 可能进入 Overwrite 或指定输出模组。

## 游戏内验收

1. 重启游戏加载存档，在较暗位置切第三人称，从正面观察脸部。日志应出现 `Player face lighting update hook installed` 和菜单注册成功信息。
2. 在 FaceLighting / Settings 反复开关补光、把强度调到 0 再调高，确认效果变化且无光源堆叠。
3. 调整半径与三轴位置，确认方向；转身、走动、蹲下，确认补光跟随头部。
4. 保持框架打开调整，确认暂停时预览也更新。验证保存、恢复默认值、丢弃更改、关闭未保存编辑及重启后保留。
5. 切第一/第三人称、室内外、快速旅行、读档、死亡自动读档、返回主菜单后重进，确认光源正常消失/恢复，无残留或重复。

2026-09-12 已通过 releasedbg x64 构建；配置全部字段保存重读、即时预览与撤销、保存失败回退、旧配置兼容、异常数字及边界处理检查；四个朝向和三种旋转/缩放头部变换的位置检查。已核对本机菜单框架含新增滑块所需导出。

用户已确认 0.2.0 的基础补光和实时预览正常。0.3.0 编译、配置回归、位置换算、CS 标记保留、逆平方范围及低强度边界检查通过。用户已测试 0.3.0 CS 模式并反馈观感改善，但逆平方单开过亮；0.3.1 的联动与强度校准尚待游戏实测；读档、场景切换与第一/第三人称切换按用户要求放在同一轮验收。

光源创建/注册接口对照了本地 CommonLibVR 和 [LightPlacer 的光源实现](https://github.com/powerof3/LightPlacer/blob/master/src/LightData.cpp)，玩家跟随、配置预览及生命周期管理在本项目中实现。

## 0.3.1 亮度校准

逆平方模式自动使用线性光照标记，即使旧 INI 中 CSLinear=0 也会生效；菜单显示自动开启，切回普通模式恢复原有 CSLinear 偏好。此操作不启用 CS 全局 Linear Lighting。

逆平方强度映射为菜单强度乘约 0.2962，范围也按映射后的引擎强度计算。以默认偏移 (0,40,5)、普通半径 100、强度 1 的线性光照作为数学参考，包含 CS 的四倍系数与边缘衰减。固定校准不会随位置变化自动补偿。普通模式的传统光源倍率、肤质和后处理不在此参考内，不能保证两种模式画面等亮。已有 INI 数值保留，但逆平方亮度与范围会降低，可通过 Intensity 实时微调。


## 0.4.0 本地化

Language 默认 auto，使用 Windows 当前用户界面语言（GetUserDefaultUILanguage），中文系统显示简体中文，其他系统回退英文；不是按游戏语言或地区格式判断。菜单顶部可选择 Auto / 跟随系统、English、简体中文，对应 INI 的 auto、en、zh-CN。旧 INI 缺少此键或填写未知值时自动检测。语言切换即时预览，保存、关闭撤销与其他设置一致。

菜单标题、控件、帮助、CS 状态和保存结果集中在 include/Localization.h，ImGui 控件 ID 不随语言改变。侧栏入口随启动时的语言显示为“面部光照 → 设置”或“FaceLighting → Settings”。中文显示使用 SKSE Menu Framework 的字体配置：需要包含中文字形的 PrimaryFont 与 EnableChinese=true。本机已有 MiSans-Regular.ttf 和该开关。

用户已确认 0.3.1 校准后的观感改善，并完成读档、场景切换、第一/第三人称切换测试，均未发现问题。0.4.0 的中文字体显示与语言切换仍待游戏内确认。

NPC 面光接入准备见 docs/NPC-face-light-plan.md，0.5.0 已实现对话 NPC 光源，指定 NPC 常驻面光留待后续。

## 0.5.0 对话 NPC 面光

菜单新增“对话 NPC 面光”，配置与玩家面光独立。默认启用，使用强度 1、半径 100、偏移 (0,40,5)，CS 可用时默认采用已校准的逆平方与线性组合。第一人称下也照亮对话对象；普通模式仍可独立调整半径。参数即时预览，保存和撤销覆盖玩家、对话与语言设置。

读取 Dialogue Menu 打开期间 MenuTopicManager 的当前 speaker，仅接受非玩家 Actor。不是检测附近聊天，也不依赖屏幕准星。光源挂在 NPC 头部节点，偏移按该 NPC 朝向计算。没有标准头部节点的角色不创建面光。自定义对话模组若不使用标准对话菜单/目标管理器，需要另行适配。

默认启用 0.3 秒平滑淡入淡出，可关闭或将时长设为 0 以立即切换，时长范围 0–3 秒。使用真实时间更新，不受游戏时间倍率影响。过渡仅调制光源输出，CS 作用范围按目标强度保持稳定。同一对象重新进入对话时从当前过渡值继续；目标切换最多保留一个进入和一个退出光源，极快速连续切换时清理最旧光源。读取存档、加载画面、返回主菜单、NPC 卸载时直接清理。

INI 的 General 节新增 DialogueEnabled、DialogueTransition、DialogueDuration、DialogueCSInverseSquare、DialogueCSLinear、DialogueRadius、DialogueIntensity、DialogueOffsetX/Y/Z。旧 INI 无需覆盖，缺失字段采用上述默认值。

游戏内待验收：第一及第三人称与 NPC 开始/结束对话；开关过渡并调整时长；短时间退出并重新对话；切换对象；对话期间打开配置实时调参；读档与室内外切换；回归玩家面光。自动检查不能代替对话事件与渲染的游戏实测。

## 0.5.1 菜单层级修复

将注册入口中的 Settings / 设置改为 Settings（设置），避免框架将斜杠解析成额外子菜单。重启游戏后重新注册入口。2026-09-14 用户反馈对话 NPC 面光总体测试未遇到问题；本次仅修正菜单层级。


## 0.5.2 入口本地化

侧栏分组与设置入口统一使用 Localization 文本，默认跟随 Windows 界面语言，也遵循已保存的手动语言选择。当前框架 API 无重命名接口，因此运行中切换语言后，页面内容即时更新，侧栏名称在保存并重启游戏后更新。用户已确认 0.5.1 修复了额外菜单嵌套。


## 0.6.0 菜单分组

“面部光照”下直接提供三个同级页面，无“设置”中间层：

- 基本：语言、调试日志、CS 检测状态及线性光照说明。
- 玩家面光：玩家启用、CS 模式、半径、强度和位置。
- NPC面光：目前的对话面光，包括独立参数与淡入淡出。

共用语言与日志设置集中在基本页，玩家和 NPC 的 CS 开关继续独立控制。三页共享一个编辑草稿，切换页面保留预览。每页的“保存全部设置”“全部恢复默认”“放弃全部修改”统一作用于全部页面；关闭菜单撤销所有未保存修改。原 INI 无需迁移。侧栏标题继续跟随启动时的语言，修改语言后保存并重启更新标题。

发布准备：用户已确认 0.6.0 菜单分组测试无问题。build/releases/FaceLighting-0.6.0.zip 为玩家发布包，使用与已部署版本一致的 DLL；调试符号独立打包，附中英文安装说明与 SHA256 校验文件。


## 0.7.0 快捷键、默认状态与色温

玩家面光在新安装、配置缺失或恢复默认时关闭；已有 Enabled 值继续保留。默认 L 键切换并保存玩家面光状态，菜单支持选用其他字母键或 Off 禁用。INI PlayerHotkey 使用 DirectInput 扫描码，默认 38。长按不重复切换；暂停菜单、控制台、文字输入和本插件菜单期间忽略，不吞掉按键，因此应避免与其他功能绑定冲突。

玩家与对话 NPC 分别新增 Temperature 与 DialogueTemperature，范围 2000–10000 K，默认 6500 K。默认值归一为旧版白光，低色温偏暖，高色温偏冷，实时预览并保存。属于近似色温染色，不是光谱渲染；改变色温可能影响感知亮度。CS 线性标记路径将颜色转入线性空间，再叠加对话过渡。

色温拟合依据：[Tanner Helland 的色温到 RGB 算法](https://tannerhelland.com/2012/09/18/convert-temperature-rgb-algorithm-code.html)。本项目另作 6500 K 中性白归一和线性转换。

## Skyrim 1.5.97 核查

代码和构建目标支持 1.5.97：xmake 同时启用 SE/AE、关闭 VR；插件元数据使用 AddressLibrary 兼容模式；CommonLib 定义 SE 1.5.97，光源创建/衰减函数有 SE/AE 重定位 ID，对话管理器也有双版本 ID，玩家更新钩子使用 Actor::Update 的 0xAD 槽位。光源与输入字段使用 CommonLib 的运行时访问器。

这属于源码与构建兼容性核查，不代表已经在 1.5.97 游戏中实测。安装时使用适配 1.5.97 的 SKSE 和 Address Library；菜单框架、CS 也需各自兼容该游戏版本。发布前仍应在 1.5.97 测试加载、菜单、热键、光源与读档。

## 0.7.1 发布前审查修复

基本页增加“我的 CS 全局 Linear Lighting 已开启”，对应 CSGlobalLinearLighting，默认 0。这是玩家与对话面光共用的颜色空间确认项，不修改 CS 全局配置。CS 检测通过、此项勾选且光源使用线性标记时，才预先线性化色温颜色。若你的 CS 已开启全局 Linear Lighting，请勾选并保存；以后改变 CS 设置时需同步修改。当前没有接入可靠的跨实验分支全局状态读取接口，因此不提供误导性的自动检测。6500 K 中性白不受切换影响。

INI 配置非字母快捷键时，菜单现在显示“自定义扫描码（数值）”，保留原值，并可正常选择禁用。新增颜色空间组合与自定义键映射检查。

用户补充验收：0.7.1 修复测试通过，Skyrim 1.5.97 已通过实测。发布说明以此结果为准。



## 0.7.2 CS 手动覆盖

基本页新增 CS 适配模式：自动（默认）、手动启用、关闭。INI 对应 CSMode=0/1/2；缺失或越界值使用自动。手动跳过版本与 ISL 元数据限制，但仍要求 CommunityShaders.dll 已加载。诊断信息显示 DLL 版本、ISL 元数据及匹配结果；版本资源不可读时给出提示。手动启用不代表已经验证该构建兼容。

模式选择支持预览、保存及撤销。进入或退出 CS 适配时重建光源，避免残留扩展标记。全局 Linear Lighting 的手动确认项仍独立，需与 CS 的实际设置匹配。

Jiaye Build 0816 的名称是发布日期标识，本轮待用户专项实测；不能将未识别直接视作不兼容，也不能将手动可启用视作验证通过。

## 0.7.2 用户实测结果

用户确认：Jiaye build 0516（原使用的 1.6.0 构建）测试正常；Jiaye build 0816 使用“手动启用”后测试正常，菜单检测版本显示正常。日期标识不等于 DLL 版本号，本次结果仅覆盖上述实测构建，不扩展为其他实验版均已验证。继续保留默认自动识别及手动覆盖。

## 0.7.3 菜单文案精简

缩短中英文菜单的 CS、快捷键、位置、语言及预览提示，保留关键操作说明。详细兼容说明继续保留在 README。光照与配置行为不变。


## 0.8.0 逆平方手动范围

玩家和对话面光在逆平方模式下各自新增“范围模式”：自动（默认）或手动。手动显示独立半径滑块，范围 10–500 游戏单位，默认 100；强度仍可独立调节。普通半径、逆平方手动半径分别保存，旧配置仍使用自动范围。

利用已核对 CS 公式反解截断阈值，强度变化时同步调整阈值以保持所选作用半径。避免阈值恰好为 1（CS 将它解释为默认阈值）。范围边缘仍使用 CS 平滑衰减，缩小范围可能影响脸部亮度，尤其当光源离脸较远时。对话淡入淡出保持目标范围。

新增 INI：ManualInverseRange=0、InverseRadius=100、DialogueManualInverseRange=0、DialogueInverseRadius=100。实时预览、保存、撤销均沿用现有行为。手动范围需在实际 CS 构建中复测。

## 潜行影响（实测确认）

用户已确认面光会影响潜行侦测。0.8.0 提供潜行时自动隐藏玩家面光，尚未实现对侦测亮度查询的完全排除。影响程度尚未量化。后续候选方案见 docs/input-and-stealth-plan.md。


## 0.8.0 潜行隐藏与键盘组合键

玩家面光页新增“潜行时隐藏玩家面光”，默认开启（HidePlayerWhileSneaking=1）。进入潜行即移除玩家光源，退出后按当前 Enabled 状态恢复；不修改持久化状态。潜行期间使用快捷键仍可改变启用设置，但开启的光源要等退出潜行才显示。对话面光不受此项影响，也未实现完全隔离潜行侦测。

修饰键可选无（默认）、Shift、Ctrl、Alt，分别对应 PlayerHotkeyModifier=0/1/2/3。主键保持原配置，默认 L。左右同类修饰键均接受，需按住修饰键再按主键，额外按住其他修饰键时不匹配；无修饰模式也不响应 Shift/Ctrl/Alt + 主键。长按仍只触发一次。通过游戏键盘设备当前状态判断修饰键，不在插件中缓存按住状态；游戏失焦时不执行切换。绑定本身不会取消原有游戏按键动作。

主键和修饰键相同的无效配置会将主键回退为 L。菜单内不能触发切换，设置支持保存、撤销和旧配置迁移。潜行隐藏与键盘组合键已通过用户游戏测试。

游戏复测：蹲下/起身；潜行期间切换启用；读档恢复；左右 Shift/Ctrl/Alt + L；仅 L、不正确修饰键、长按、菜单中释放、Alt-Tab 后恢复。

## 0.8.0 手柄支持与版本整合

本次将逆平方手动范围、潜行隐藏、键盘组合键和手柄支持统一为 0.8.0；之前标记为 0.9.0 的包仅为开发测试包。

玩家面光页提供独立的手柄主键与可选修饰键，默认禁用。支持十字键、ABXY、LB/RB、LT/RT、摇杆按下、Start/Back，按 Xbox 键名显示。例如选择主键 D-pad Up、修饰键 LB，按住 LB 再按方向上切换。主键长按仅处理引擎提供的首次按下事件；LT/RT 使用游戏自己的按下/释放判定。不会屏蔽按键原有动作。

INI PlayerGamepadKey / PlayerGamepadModifier 默认均为 0；使用 SKSE 统一键码 266–281，不能填写 XInput 原始掩码。键盘和手柄绑定独立，关闭键盘快捷键不影响手柄。Steam Input 映射为键盘的输入仍走键盘设置。手柄支持已通过用户游戏测试，包括断开重连。


## 独立语言文件（0.8.0 本地化构建）

语言文件位于 languages/en.ini、languages/zh-CN.ini，安装到 SKSE/Plugins/FaceLighting/Languages。新增语言按文件自动发现；默认检测 Windows 界面语言，可手动选择，缺项回退英文。翻译制作说明见 [LOCALIZATION.md](docs/LOCALIZATION.md)。用户制作的繁体中文文件已通过游戏内测试，独立语言文件支持纳入 0.8.0 正式版。




## 0.8.1 跟随骨骼朝向

玩家和对话 NPC 分别提供可选头骨旋转跟随，默认关闭。开启后偏移按头骨局部 X/Y/Z 解释，包含俯仰和侧倾，可能需重新调整位置。用户已确认游戏测试通过，纳入 0.8.1 正式版。实现及角色专属柔光评估见 docs/bone-follow-and-character-lighting.md。



## 0.8.2 对话玩家面光自动开关

玩家页提供进入对话自动开启（默认开启）、结束对话自动关闭（默认关闭）；保存后生效。已有显式配置继续保留。结束关闭包括对话前手动开启的灯。自动切换保存 Enabled 状态，仍遵循第一人称和潜行隐藏。实现与 CCC API 评估见 docs/ccc-integration-assessment.md。用户已确认游戏测试通过。


## 0.8.2 扩展键盘快捷键

菜单支持 F1–F12、数字行、方向/导航键、小键盘、空格/回车/退格/Tab 及常用符号键，保留 A–Z、Shift/Ctrl/Alt 修饰键和自定义扫描码。采用物理扫描码与常用英文键名，不吞掉游戏原动作；F5/F9 等可能与游戏原有快捷键冲突。进入对话自动开灯缺省改为开启，已有灯开启时不重复操作。用户已确认游戏测试通过，纳入 0.8.2 正式版。低光照自动面光评估见 docs/ambient-light-assessment.md。

