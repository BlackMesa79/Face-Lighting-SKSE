# 骨骼朝向跟随与角色专属补光评估

日期：2026-09-16。骨骼跟随已通过自动化检查及用户游戏测试，正式发布版本为 0.8.1。

## 骨骼朝向模式

玩家、对话 NPC 页面分别提供“跟随骨骼朝向”，默认关闭。INI 对应 FollowHeadRotation、DialogueFollowHeadRotation，缺少字段时保持原行为。

关闭：头部世界位置 + 按 Actor::GetAngleZ() 旋转的偏移；Z 保持世界向上。
开启：头部世界位置 + 头骨世界旋转 × 配置偏移；包括转头、俯仰、侧倾。位置以头骨局部 X/Y/Z 表示，不假定所有骨骼都使用相同的面部前向轴。菜单切换为“头骨局部 X/Y/Z”标签，并提示按骨骼调整位置。两个模式共用各自已有偏移值，切换后可能需要重新调节。

光源仍作为头节点的子节点，开启时局部位置为 offset/headScale，避免角色缩放将配置距离倍增。没有增加光源、独立线程或动画钩子。缺少头部节点和无效位置时沿用清理逻辑。开启后不再每帧抵消动画头部旋转。

自动化覆盖：180 度动画转身且 Actor heading 不变、一般旋转（含俯仰/侧倾）、0.5/1/2 倍缩放、旧朝向模式、玩家/NPC 设置保存和旧 INI 默认值。用户已确认新模式测试无问题；未提供具体覆盖的动画清单，不据此宣称所有动画模组均已验证。

游戏验收：分别测试玩家和对话 NPC 开关、偏移预览/撤销/保存；静止转头、靠墙转身、低头、切换人称、读档。观察新模式是否需要调整局部 Y/Z，并检查动画过渡时是否过度摆动。

## 用户需求拆分

1. 更柔和、减轻脸部过曝：当前点光方案可以降低强度、调整位置、使用手动范围以限制附近环境受光。效果随曝光、材质和 CS 设置变化；更小范围不等于更软的光，原实现已不投射阴影，不能靠“软化阴影”消除亮斑。
2. 只照角色，不照墙：现有场景点光源不能通过半径/强度保证。墙面进入作用范围仍会直接受光。这通常是直接照明，不必然是光线反射。
3. 只照皮肤而不照衣甲：还要识别皮肤材质/渲染对象；与“只照角色”不是同一个过滤条件。脸、身体、手部、眼睛、头发以及替换身体/衣甲需要分别考虑。
4. 只照玩家与当前对话 NPC：需要目标身份或逐对象标记，不能将全局 Character Lighting 设置当成两盏独立可控的灯。

## 已核对的依据

- 本地 Jiaye 0816 的 Shaders/Lighting.hlsl，约 2474–2478 行：CharacterLight 标记控制额外 diffuseColor；按视线、法线和 CharacterLightParams 计算，与前面的点光计算分开。
- 同一构建的 Shaders/LightLimitFix/Common.hlsli：Light 数据包含颜色、半径、位置、房间和光照标志，没有现成的“仅某个 Actor/仅皮肤”接收者字段。未发现可以直接给现有面光设置的角色专属开关。
- CommonLib 的 BSShaderManager::State 提供 characterLightEnabled、characterLightParams 等共享状态；BSShaderProperty 有 CharacterLighting 标记。仅找到这些字段并不等于获得跨 CS 构建稳定、按 Actor 隔离的公开接口。
- 公版仓库 dev 的 SubsurfaceScattering.cpp 提供 Enable Character Lighting 与 CharacterLightingStrength 控件，说明可先在 CS 自带设置中对比这种效果。dev 代码不是所有已发布 CS 版本的行为保证。

来源：
https://github.com/community-shaders/skyrim-community-shaders/blob/dev/src/Features/SubsurfaceScattering.cpp
https://github.com/community-shaders/skyrim-community-shaders/blob/dev/src/ShaderCache.h

## 可行性结论

“柔和角色补光”可行，原版 Character Lighting/CS 对应选项值得先关闭本模组点光后单独比较。它更接近材质着色阶段增加补光，不是再往场景添加一盏点光源。

“指定 Actor + 仅皮肤 + 环境完全不变”需要专项原型：识别目标渲染对象、给着色器传入目标/材质信息、处理 CS 公版与 Jiaye 分支渲染路径差异。现有代码没有证明一个稳定的通用实现，不能承诺只改一个 light flag 就完成。ENB 需单独评估。

即使直接补光仅作用在角色材质上，SSRT/SSGI 等屏幕空间效果也可能将变亮的角色纳入反射或间接光，因此不能保证环境最终画面绝对零变化。

建议先确认 CS 自带 Character Lighting 的观感是否符合需求，再决定是否开发独立的“角色柔光”模式。保留现有点光模式用于需要位置、半径和场景交互的用户。本轮只完成评估，没有改动 CS 设置、全局角色光或着色器。


## 2026-09-19 复评：推荐独立角色补光模式

重新核对本地 Jiaye 0816 的 Lighting.hlsl 2474–2478 行，以及 [CS dev Lighting.hlsl](https://github.com/community-shaders/skyrim-community-shaders/blob/dev/package/Shaders/Lighting.hlsl)：CharacterLight 分支将额外项加入 diffuseColor，与点光源 diffuse/specular 计算分开。用户描述更接近这类角色着色补光，而非缩小点光半径或增加阴影柔化。此判断针对效果方向，不宣称原版每个 NPC、每种材质均启用相同标记。

[CS dev SubsurfaceScattering.cpp](https://github.com/community-shaders/skyrim-community-shaders/blob/dev/src/Features/SubsurfaceScattering.cpp)提供 Character Lighting 开关与强度，并在 Reset 中写入共享 characterLightEnabled／characterLightParams。因此不应在本插件每帧争写这些全局值；该选项虽位于 SSS 页面，但 Character Lighting 与皮肤散射本身是不同功能。页面与代码来自可变 dev 分支，不代表所有发布版设置相同。

可研究的路线：

1. 现有点光调整：降低强度、减少近距离过曝、调偏移与范围，工作量较低；无法保证不照墙，缩小半径可能出现边界截断。CS 光源 size 字段也没有证明为通用角色柔光或接收对象过滤开关。
2. 原版渲染接收对象过滤：本地 BSLight 有 geomList、objectNode、affectLand、affectWater 等字段，值得做引擎专项验证；字段存在不代表稳定 API。关闭地形／水面影响不等于排除建筑墙面，CS clustered lighting 也不保证尊重原生对象列表。因此不能仅凭这些字段宣布原版与 CS 均可只照角色。
3. 角色着色补光（更符合目标）：先复用或验证 Character Lighting 的观感，再研究仅对所选 Actor 的渲染对象施加独立的漫反射补光参数。若没有合适的现成接口，需绘制阶段钩子或 CS 专门集成，而不是简单向 NiPointLight 增加开关。

应将“仅目标角色整体”作为第一阶段，“仅皮肤、不影响衣甲”作为第二阶段。后者要处理脸／身体／手／眼睛／头发、衣甲内嵌皮肤、兽族、自定义身体、第一人称双手，以及换装与模型重建。不能直接把 skinned mesh（骨骼蒙皮网格）当成 skin material（皮肤材质）。渲染属性还可能共享，避免无意影响其他角色。

专项原型的验收：只选一个角色，关闭其点光，确认柔光可独立开关；邻近未选择 NPC 和墙面没有直接补光；对话／指定／随从优先级保持单一结果；换装、读档、换场景与镜头切换可正确恢复。先选一个固定 CS/Jiaye 构建验证，原版与其他分支、ENB 分开核验。反射／间接光仍可能反映变亮后的角色，不承诺最终画面中环境每个像素都完全不变。

开发顺序建议：亮度诊断原型与角色柔光观感对照都值得先做，但均不直接加入已验收稳定版。若只能先投入一项工程，先完成玩家暗环境数据验证；若用户最在意墙面被照亮，则优先角色补光专项原型，而不是继续给点光叠加参数。角色着色补光可能减少对引擎光照检测的反馈，但同样要实测，不能当作已证实的优势。
