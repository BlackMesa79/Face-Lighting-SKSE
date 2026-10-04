# Face Lighting SKSE 开发交接

更新时间：2026-10-04（Asia/Hong_Kong）。用于切换账号后继续开发。

2026-10-04 GitHub 同步：用户明确要求推送当前代码并补齐此前未提交记录。本轮补全 CHANGELOG.md 中 0.9.0～0.9.4 的变更摘要，将 0.9.2/0.9.3 发布记录、SE 运行时修复和 0.9.4 功能分组提交到 main。未重建、倒签历史源码快照；当前源码才是 0.9.4。下方“未提交/推送”是同步前状态。正式运行包内容与已测 DLL 不变。

2026-10-04 发布准备（以此段为准）：用户确认当前功能测试正常，暂不扩展 1.5.97 实时排除支持，按 0.9.4 发布。版本号已更新，新增 release-materials/0.9.4 简短中英文 changelog。本版包含公开 API V1（收藏轮盘对接已由用户实测正常）、NPC 同时启用预算、可选对话环境控制、三档共享检测频率及菜单精简。实时排除仍仅 1.6.1170；静止时引擎缓存刷新问题未宣称修复。安装 ZIP 不含初始 INI，仅 DLL、两种语言和合并许可的 readme.txt；另提供当前工作目录对应源码 ZIP。下方开发构建版本、哈希和“待实测”说明是历史记录。

0.9.4 发布构建已通过 15 个测试程序并自动部署，语言/DLL 与本地一致，用户 INI 保留。文件版本 0.9.4.0；DLL SHA256：79AC9A0C1CCE1C7C16BA941B88E9D9007093DA456DA26A5BC51088BD7A5AD9A1。运行 scripts/package-release.py 可在 build/releases/0.9.4 生成安装包、当前工作目录源码包及 SHA256SUMS.txt；源码包含未提交的新文件。未执行 Git 提交/推送或 Nexus 上传。

2026-10-04 菜单整理（当前最新）：用户复测前版正常，本轮将玩家环境自动控制整组（模式、补偿、开关阈值、持续时间）从基本页移至玩家页“玩家面光自动控制”分组。玩家/对话共用的环境检测频率仍在基本页，对话自动控制仍在 NPC→对话。精简中英文及英文回退中的环境、对话、预算、随从、潜行、第一人称和过渡说明；详细限制留在 docs 文档。未修改设置存储、默认值或运行逻辑。构建、15 个测试和部署校验通过，现有用户 INI 保留。最新 DLL 本地/部署 SHA256：0A3A1B08594376F0231A29FCEFFD202D384BA47FBFEF5A13743C55A519FF8E1C。未重新制作正式发布包或提交/推送。

2026-10-04 后续最新：用户希望降低环境判断频率，并进一步要求做成三档下拉设置而非滑块。已在通用页新增“环境检测频率”，性能优先每 1 秒（默认）、均衡每 0.5 秒、响应优先每 0.2 秒；General/AmbientPollMode=0/1/2，旧配置缺键默认 0。玩家与对话共用 include/AmbientPoll.h 调度，读取亮度/阈值 Step 只在到期时运行；安全、暂停、模型生命周期和灯光渐变仍每帧处理。AmbientPolicy 根据档位容忍正常间隔，长断档重新累计，卡顿不补轮询。15 个测试通过，覆盖三档、切换、静止模拟读数、暂停和老配置；已自动部署，用户 INI 保留。当前 DLL 本地/部署 SHA256 同为 8C525BAC4AE8557F0E02A34B04AFA5E6E7A715BD51264D8A46993217EE55817D。此前哈希均为历史产物。

用户还反馈“光源变化但原地不动不自动开启”。模组没有移动位置门槛，但取值是引擎缓存/被动排除结果。已有 1.6.1170 CalculateLightValue 捕获记录显示 middleHigh+0x328 标志及时间等早退条件（非 high+0x328）；当前 CommonLib 将该字段标为 unk328，不应凭猜测赋值。原地不刷新的现象可能来自引擎条件重算，但尚未用该场景的有效诊断日志证实，不能宣称已修复。当前部署没有主动重算、没有延长一秒采样有效期；实时排除 hook 采集仍随引擎原查询，不受判断频率档位节流。若继续研究，需记录站定/移动时 raw、paired、过滤值年龄与阈值状态，不能把门控频率变更当作缓存刷新修复。

2026-10-04 最新：用户已实测酒馆 4 名 NPC + 1 随从、预算设为 5 正常。本轮新增可选对话面光环境控制，NPC→对话→启用与显示设置独立模式（0 关闭默认、2 固定补偿、3 实时排除）、开关阈值（30/50 默认）与持续时间（2 秒默认）。与玩家模式/主开关独立，但采样明确为玩家位置，不是 NPC 皮肤直射光。实时排除仍仅已核验 1.6.1170，首次启用保存并重启，缺数据不自动切固定模式。固定模式共享玩家补偿并新增对话补偿，需校准。

新对话先不亮，确认持续暗处后按原对话过渡开启；明处持续超高阈值淡出，只保留当前对话来源完成淡出，不允许随从/指定来源绕过环境判断。对话结束恢复名单和预算。跨角色/cell/参数变化/读档复位，暂停和无效读数不累计计时。接口 V1 布局未变，对话被环境抑制返回 HiddenAmbient。新文件 include/DialogueAmbientPolicy.h、tests/DialogueAmbientTests.cpp、docs/dialogue-ambient-control.md；15 个测试程序通过。新对话功能游戏内表现待复测，旧玩家回归及 SE 原生布局测试通过不等于新功能 SE 实测。

当前 DLL/语言已自动部署，用户 INI 保留；DLL 本地/部署 SHA256 同为 DF6E1BCDB5F3667C7519EBDD93448EFFCC58BD5812BC6AE344272421C9371C4A。仍为本地 0.9.3 开发构建，未新打包/提交/推送。下面旧哈希、预算待测试和自动控制仅玩家的描述以此段为准。

2026-10-03 最新：用户确认收藏轮盘接入后测试正常。本轮新增 NPC 页顶部“同时启用的 NPC 面光数量”整数滑块，1～32、默认 4，INI General/NPCLightLimit；只影响随从与指定 NPC 共用的非对话灯预算。指定名单容量仍 32；玩家独立、随从优先于指定 NPC、对话独占/结束恢复均保持。支持现有预览/保存/撤销/恢复默认值，旧 INI 缺键默认 4，越界钳制，非法整数回退 4。公开 API V1 布局不变，配置 revision 包含新预算值。

已构建并自动部署 DLL 和两种语言文件，原有用户 INI 未覆盖。DLL 本地/部署 SHA256：C0284E6811009302BAF55E1F643D5C79DF0354904845A511369C9F5D78BE782B。新增回归覆盖 1/4/12/32 预算、实时增减、优先级、对话空目标/恢复和淡出预算，以及 INI 保存/旧配置/非法值/中文英文菜单文本。版本仍为本地 0.9.3 开发构建，未重新制作公开发布包，未提交/推送。新滑块的游戏内显示和实际多人照明仍待用户复测。下面 10-01 的接口未接入/旧哈希等说明为历史记录。

2026-10-01 最新：用户已授权先实现面光公共接口，API V1 已完成。入口 `FaceLighting_GetAPI`，独立头文件 `include/FaceLightingAPI.h`，契约/接入示例 `docs/public-api.md`。支持玩家主开关、捕获准星 NPC、双来源个人开关、随从组开关、随从分页和运行状态查询。回调限游戏主线程，检查 session/revision、目标身份、加载状态、配置预览和暂停；个人操作同步提交，实际场景更新随后调度。全部 14 个测试程序通过，包含实际 DLL 导出/版本/线程入口测试。API 游戏内客户端联测尚未进行。

最新开发 DLL 已自动部署到既有 H: 模组目录，本地与部署 SHA256 均为 `10087073FF3670E32ABDE14E120140CB12DECEEC717A0234C0F02CA9D2F33557`。版本资源仍为 0.9.3.0，但早先 0.9.3 发布压缩包不含 API，未重新打包/提交/推送。收藏轮盘代码未修改，下一步接入 FaceLightingClient；多灯与任意骨骼仍为设计。下面带有旧版本号、旧 SHA256、未部署/接口未实现的段落为历史记录，以此段与“当前真正的状态”为准。

2026-10-01 补充：当前 xmake 版本已升至 0.9.3，ActorState 修复纳入本次独立发布包，简短 changelog 位于 release-materials/0.9.3，并致谢发现者 jinx60。旧 0.9.2 包保留。本次构建成功，但游戏 DLL 被占用导致部署失败；不要把原部署 DLL 视为已更新到 0.9.3。下文 09-30 的版本与打包状态是历史记录，代码修复本身继续有效。GitHub 尚未同步本地 0.9.2 / 0.9.3 改动。

2026-10-01 后续：用户退出游戏后已重新部署成功，DLL 为 0.9.3.0，本地/部署 SHA256 均为 3A03462AECB13D6B5D7BCFF9AAE28719C0354F878CDA9C7F0F265B4AB386E18A。完成两项研究文档：docs/favorite-wheel-integration-design.md、docs/multi-light-bone-binding-design.md。收藏轮盘代码已为 0.2.7 并具有功能模式和 TextBridge 可选 DLL 调用；面光联动及多灯仍处于设计阶段，未实现、未修改轮盘代码。后续账号应先读这两份最新设计，旧版本号与架构评估仅作历史参考。

## 先读：当前真正的状态

- 项目：`D:/MyFiles/Documents/ChatGPT/SKSE模组 面部光照`，Windows / PowerShell，C++23、Xmake、MSVC、SKSE、vendored CommonLibVR。
- GitHub：<https://github.com/BlackMesa79/Face-Lighting-SKSE>，分支 `main`。
- 本地 HEAD 与本地缓存的 origin/main：`ec3dd92bc8fdbc33f115b4c93038ecdd1f898533`，对应 0.9.1 完整源码提交。交接时未联网重新 fetch。
- `xmake.lua` 当前版本为 **0.9.3**。0.9.3 发布包含 ActorState 修复，不含随后新增的公共 API。API 开发 DLL 已单独构建并部署。
- **本地 0.9.2/0.9.3 和 API 改动均尚未提交。只克隆 GitHub 无法取得最新工作。**
- 最近一次 API 构建后 14 个测试程序全部通过；SE 1.5.97 用户端的实际复测仍需单独确认，单元测试不代表游戏内验证。
- 最新请求是先实现面光公共接口，已完成。收藏轮盘接入待后续实施；多灯与任意骨骼待研究原型。

## 跨账号交接方法

同一电脑：让新账号打开原工作目录，读取本文件、`git status` 和相关代码即可。

另一台电脑：复制完整工作目录，至少包含所有 tracked 文件和下面列出的 untracked 文件。若需要保留已有发布产物，还应复制 `build/releases`。单独发送本文件或克隆仓库都不会带上未提交的修复。不要直接 reset/clean 覆盖本地工作。

可给新会话的提示：

> 请先阅读项目根目录 HANDOFF.md，核对当前 git 状态。保留未提交改动，尤其是 ActorRuntime 的跨版本修复。先说明当前代码、已打包版本与待验证事项的区别，再按照我本轮要求继续开发。

## 用户偏好与授权

- 中文沟通，结果清楚、简洁；发布 changelog 通常需要简短英文，另留中文版本。
- 已长期授权构建后自动部署到下述游戏模组目录，保留用户配置。
- 用户主要在 **AE 1.6.1170** 实测，但明确要求重视 **SE 1.5.97** 向下兼容。AE 正常不能代表 SE 正常。
- 从 **0.9.1 起安装包不得附带初始 `FaceLighting.ini`**。缺少配置时使用代码默认值，在菜单保存后生成 INI；继承用户已有配置。
- 安装包仅包含运行文件与一个 `readme.txt`。GPL 正文和第三方许可合并到 readme，不额外塞许可证目录、使用文档、changelog。语言 `.ini` 是运行资源，仍须包含。
- 根目录 `FaceLighting.ini` 保留作源码参考，不能因此重新加入安装包。
- 源码包单独提供。内部 `build/package/FaceLighting` 含额外开发文档，不要直接压成正式安装包。
- 曾明确要求提交推送 0.9.1，已完成；0.9.2 及后续修复未推送。后续发布同步时应明确告知实际完成范围。
- 未获用户要求不另开会话或启动子代理。不把分析评估自动扩大成跨项目修改。

## 重要路径与构建

- 游戏部署：`H:/Games/Dev Skyrim/mods/36 - Face Lighting SKSE/SKSE/Plugins`
- 用户本机实际配置可能在 MO2 overwrite：`H:/Games/Dev Skyrim/overwrite/SKSE/Plugins/FaceLighting.ini`
- 本机日志：`D:/MyFiles/Documents/My Games/Skyrim Special Edition/SKSE/FaceLighting.log`
- DLL：`build/windows/x64/release/FaceLighting.dll`
- 打包脚本：`scripts/package-release.py`
- Python：`C:/Users/BlackMesa/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe`（换电脑需重新定位）。
- 当前本机构建配置已设置 SE+AE、VR=false、deploy_dir；不要随意重置 Xmake 配置。

```powershell
xmake build --all
# 等命令彻底结束，再运行测试，避免正在链接的 exe 被占用。
$failedTests = @()
Get-ChildItem build/windows/x64/release/*Tests.exe | ForEach-Object {
    & $_.FullName
    if ($LASTEXITCODE -ne 0) { $failedTests += $_.Name }
}
if ($failedTests.Count) { throw ($failedTests -join ', ') }
```

构建后自动复制 DLL 和语言文件。游戏运行时 DLL 可能被占用；此时可能已成功编译但部署失败，不能报告已部署。不要强制结束游戏。

交接时本地与游戏部署 DLL 的 SHA256 相同：
`10F94A4848628CB3953AADE8BD482CBA3C8B5752FE13484836DB169E4B165E2F`

发布时先决定新版本号，再构建、测试、运行 Python 打包脚本。脚本读取 xmake 版本，生成 `build/releases/<版本>/FaceLighting-SKSE-<版本>.zip` 和 SHA256SUMS，验证无初始 INI。脚本目前**不自动生成源码包**，源码包需要另外制作，且必须包含当前未提交代码，不能直接 archive 老 HEAD。再次运行打包脚本可以把已有源码 zip 纳入校验清单。

## 最新修复：角色生命状态偏移

问题：用户在 1.5.97 下无法自动识别 Ashe，快捷键、控制台和菜单添加都可能报“未选中有效 NPC”。并非 AI 随从特有的限制。

已核实旧 0.9.2 DLL：RVA `0x4BDCC` 执行 `add rax,0xA0`，调用的 GetLifeState 读取 `+8`，实际读 actor+0xA8。这个 RVA 仅属于已核对的旧构建，不可用于通用补丁。

CommonLib 正确运行时布局：

| 版本 | ActorState | 生命状态字段 |
| --- | --- | --- |
| 1.5.97、1.6.629 前 | actor+0xB8 | actor+0xC0 |
| 1.6.629 及以后 | actor+0xC0 | actor+0xC8 |

已在 `include/ActorRuntime.h` 改为 `actor->AsActorState()->GetLifeState()`。不要恢复为直接 `actor->GetLifeState()`，不要硬编码统一 0xB8。此共享检查涉及玩家、对话、随从、指定 NPC，因此影响不限于 Ashe 或 SE。

原测试通过 C++ 错误布局写状态，再用同样布局读，形成盲点。现在 `tests/ActorRuntimeTests.cpp` 使用 `REL::Module::mock`，独立写入真实字节偏移，覆盖 1.5.97、1.6.353、1.6.629、1.6.1170；在旧错误偏移及另一版本偏移写入死亡数据，确认活人仍通过，并验证濒死/死亡/回收拒绝。`ENABLE_COMMONLIBSSE_TESTING` 仅在该测试 target 启用。

另一项既有修复必须保留：IsDead 显式使用 SE/AE vtable 0x99（VR 0x9A），原因是该 CommonLib 声明存在额外虚函数槽；直接 `actor->IsDead()` 曾存在错位风险。潜行检查的 Actor::IsSneaking 内部已走 AsActorState，随从标志走 GetActorRuntimeData。

详见 `docs/runtime-compatibility-checks.md`。下一步重点是用户端 1.5.97 / Ashe 的自动注册、Shift+L、控制台、菜单复测，随后再准备修复发布与源码同步。

## 当前面光规则：不要恢复已否定的策略

- 玩家独立管理。NPC 来源优先级为 **对话 > 随从 > 指定 NPC**。
- 非对话时最多 **4 个次要 NPC 光源**（指定与随从合计）；不是引擎固定上限。随从注册名单可以超过 4，注册不等于正在照明。
- 对话菜单打开时，暂停除玩家与当前对话对象以外的所有 NPC 面光；对话结束按用户开关、资格与预算恢复。不修改名单/偏好。
- 即使菜单打开时说话者暂时缺失，也保持暂停其他光源。
- 同一个角色多来源复用一盏灯。满载时高优先级可以抢占较低优先级；淡出灯也计入预算；无效角色先过滤再分配预算。
- **没有玩家周围空间避让。** 0.9.1 曾用半径和安全余量关闭附近 NPC，导致玩家开灯就让周围 NPC 全灭，用户反对，0.9.2 已移除。
- 用户后来明确要求重新保留“对话独占”，所以不要误按中间方案改成对话与所有次要灯共存。
- 手动关灯、第一人称设置、潜行和死亡保护仍优先；“保留玩家面光”不表示强行打开用户关闭的灯。
- 潜行隐藏覆盖玩家与队友，即使队友当前来自指定/对话来源；普通非队友 NPC 不因该队友判定隐藏。
- 玩家过渡默认约 0.2 秒。死亡/缺失骨骼等安全清理立即执行。玩家斩首场景此前未能实测复现，不要声称完整验证。
- 现有灯光预算只是减轻本模组竞争，**没有引擎最终光源丢弃检测，也不能保证渲染层绝对保底**。

## 其他现有功能与限制

- 指定 NPC 名单、快捷键添加/切换、对话光照、自动队友注册、每位随从开关、通知、可选第一人称。
- 默认 L 控制玩家，Shift+L 添加/切换准星 NPC。名单与随从偏好随 SKSE co-save 保存，INI 只保存设置。
- 指定名单最多 32；随从采集灯光候选最多 32，底层 NPC 管理容量 66；当前渲染保护另限制次要灯为 4。
- 暗环境控制：原玩家控制独立保留；10-04 开发构建新增默认关闭的对话自动控制，独立模式/阈值/延迟，仍使用玩家位置采样。随从/指定个人来源没有环境自动控制。见 docs/dialogue-ambient-control.md。
- 实时排除 hook 仅验证/允许对应的 1.6.1170 代码布局，不可宣称支持 1.5.97 或 1.7.x。新增多光源时需更新排除登记和汇总。
- 发布菜单已隐藏多余诊断控件，诊断仍可通过 INI 开启。
- CS 适配为开/关，不再自动版本白名单；保留基本版本信息。仅某些 Jiaye 构建曾由用户实测，不能把其他版本推断为已验证。
- 当前不支持 VR / 1.7.x；路线图中的兼容性不是已完成功能。

## 接口与后续功能

### 引擎光源丢弃检测

用户说“以后再说”。场景注册列表只能证明注册状态；BSRenderPass 的 geometry/sceneLights 可作为目标头部网格是否选入灯光的研究入口。未选入不等于超限，需排除视野、距离、材质和无绘制等情况。CS 聚簇路径还需单独检测。未添加渲染 hook 或自动自愈逻辑。

### 收藏轮盘联动

另一项目：`D:/MyFiles/Documents/ChatGPT/SKSE模组 收藏轮盘`。上次仅只读检查，未修改；该项目可能独立继续发展，实施前重新检查其状态。检查时为 0.1.4、只允许 1.6.1170。

面光已提供带版本号的公共 API，详见 `docs/public-api.md` 和 `docs/favorite-wheel-integration-design.md`。轮盘待增加功能分类：玩家开关、锁定准星 NPC、随从列表逐人开关/总开关。现有轮盘有选操作→关菜单→恢复游戏后主线程执行的队列，可以复用。轮盘当前 0.2.7，以最新设计文档为准。

需要处理：打开轮盘前保存目标句柄、执行时重验；区分用户开启与因对话/潜行/预算暂时隐藏；同一角色同时属于随从和指定名单时统一“关此角色”的语义；避免模拟按键或直接改 INI；缺少面光插件时正常降级。面光支持 SE 不意味着轮盘自动支持 SE。

### 单向照射

当前 NiPointLight 全向，旋转不会变聚光灯。CommonLib 的 BSShadowFrustumLight 是可研究入口，但动态创建、朝向、阴影性能及 CS 路径未验证。“单向照射”不等于“只照人物不照墙”，restrictedNode 也仅是调查线索，不能仅凭名字承诺效果。建议先做单角色聚光灯实验。

### 多灯与任意骨骼

可行，但需要一人一灯→一人一组灯的结构变化：独立灯 ID、骨骼名、位置/朝向、颜色、范围、强度、开关与预设。必须处理模型重建、骨骼缺失、不同种族/第一三人称骨架、节点缓存失效和死亡卸载。预算需按实际灯数而非角色数计算；潜行/对话规则及环境光排除要覆盖全部灯。

建议先轮盘联动，再玩家少量多灯编辑；单向照射独立实验。此顺序是建议，不是用户已授权启动实施。

## 文件导航与待提交清单

核心：`src/FaceLight.cpp`、`include/NpcLightManager.h`、`include/LightInstance.h`、`include/ActorRuntime.h`。
角色：`src/Followers.cpp`、`src/SelectedNPCs.cpp`。
设置/界面：`src/Settings.cpp`、`src/ConfigMenu.cpp`、`languages/`。
环境光：`src/LightProbe.cpp`、`src/LightExclusionProbe.cpp`。
说明：`docs/priority-protection-review.md`（包含历史方案，注意最新修订）、`docs/runtime-compatibility-checks.md`。
发布：`release-materials/0.9.2/` 中英简短 changelog；`build/releases/0.9.2/` 已打包旧产物。

交接文件创建前的 git 状态：

```text
 M README.md
 M docs/priority-protection-review.md
 M include/ActorRuntime.h
 M include/NpcLightManager.h
 M src/FaceLight.cpp
 M tests/ActorRuntimeTests.cpp
 M tests/NpcLightManagerTests.cpp
 M xmake.lua
?? docs/runtime-compatibility-checks.md
?? release-materials/0.9.2/
```

本 HANDOFF.md 另为新文件。本轮只写交接，不提交、不推送、不重新打包、不改游戏配置。
