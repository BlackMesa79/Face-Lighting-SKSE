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

