# Community Shaders 适配记录 — 0.3.1

日期：2026-09-13。基准为用户当前 Jiaye build 整合，同时参考 N 网公版；ENB 后续评估。

## 核对依据

| 来源 | 固定提交 | 核对内容 |
| --- | --- | --- |
| 公版 v1.6.0 | bc8f4b3737d7a5eec80931b566e8d706c71906c6 | 光源扩展字段、标记、逆平方公式 |
| N 网当前公版 v1.8.4 | 02646c3008dd7cae91fc790c67c0f342a29aa938 | 同上，布局及公式一致 |
| 用户提供的 Jiaye 仓库 dev | 3d472fdeab2d9eec83f27098850c16865ede5820 | 同上，布局及公式一致 |

本地 DLL 文件版本 1.6.0.0；SHA256 `4AC1F0EE33CA0D179B91EBB5570DF1F7775F6B3762FAC283E981E005F4525C7C`。本地 ISL 元数据为 1-3-0，LightLimitFix 为 3-0-3，存在 PostProcessing 文件。用户确认使用带 PostProcessing 和 SSRT 的 Jiaye 实验构建。尚未获得该二进制的精确提交号；上述 Jiaye dev 是源码参考，不能声称与本地构建完全一致。

参考链接：[N 网公版](https://www.nexusmods.com/skyrimspecialedition/mods/86492)、[公版扩展布局](https://github.com/community-shaders/skyrim-community-shaders/blob/02646c3008dd7cae91fc790c67c0f342a29aa938/src/Features/InverseSquareLighting/Common.h)、[Jiaye 光源处理](https://github.com/jiayev/skyrim-community-shaders/blob/3d472fdeab2d9eec83f27098850c16865ede5820/src/Features/InverseSquareLighting.cpp)。

## 实现与取舍

- 通过游戏原生 NiPointLight/ShadowSceneNode 接入 CS；不修改 CS、PostProcessing、SSRT 的文件或全局配置。
- ambient.red 按位保存扩展标记；green 为截断阈值；blue 为基础 LIGH FormID。本插件生成的光源无基础 LIGH，blue 初始化为零。
- radius.x 是作用半径；radius.z 在 CS 协议中是光源尺寸。使用现有成员访问，不调用实验版私有函数地址。
- 不再每帧清零 ambient 或把三个 radius 分量全部写成作用半径。设置改变时仅更新本插件负责的值，其余 CS 标记保留。
- 普通模式默认开启，两个新增 CS 选项默认关闭；保持 0.2 的半径、强度和位置配置。
- 逆平方模式由 CS 根据强度推导范围。使用 CS 的四倍强度系数及相同截断公式，初始化引擎半径与菜单估算值保持一致。极低强度的零半径边界会微调截断阈值至 0.0501，避免 CS 后续除零。
- 线性标记只在 CS Linear Lighting 开启时改变转换路径；它跳过传统点光源的颜色转换及倍率，不是打开全局 Linear Lighting 的开关。
- 运行时版本与元数据匹配只说明尝试使用已核对协议，不构成实验构建验证。未识别组合保留普通补光，CS 首选项仍保存在 INI。

## 验收

编译、CS 位标记保留、逆平方范围与极低强度边界、设置持久化和撤销、0.2 配置迁移、位置换算已自动检查通过。用户已验证 0.2 基础补光和实时预览，并测试 0.3 CS 模式：观感有所改善，逆平方单开过亮，搭配线性后正常但仍偏亮。0.3.1 联动及校准待游戏内复测。

游戏内建议顺序：

1. 保持两个 CS 模式关闭，对照此前补光及所有滑块。
2. 开启逆平方模式，调整强度并观察范围，切回普通模式确认原半径恢复；保存、撤销及重启后确认状态。
3. 在整合开启 Linear Lighting 的情况下比较线性开关；固定天气、位置、曝光和后处理预设比较。
4. 在 Jiaye build 中分别比较 PostProcessing/SSRT 开关前后的脸部亮度、反射和间接照明，不由本插件自动切换它们。
5. 统一测试读档、死亡读档、室内外切换、快速旅行、第一/第三人称切换；确认没有重复光源、残留或恢复失败。

公版 1.8.4 与本地 Jiaye build 的视觉表现均需分别实测，当前没有承诺二者画面一致。

## 0.3.1 调整

逆平方标记强制搭配线性标记，保留普通模式 CSLinear 偏好。运行时和菜单范围统一使用校准强度，系数约 0.2962。数学参考为默认头部偏移距离 sqrt(1625)、半径 100、强度 1 的普通线性光源；验证包含 CS 距离与范围边缘衰减。该系数固定，不追踪距离或曝光，不包含传统点光源转换倍率，也不保证不同强度、脸部位置、CS 预设下等亮。旧配置无需重置，逆平方亮度和范围会有所降低。


用户后续验收：0.3.1 亮度校准后观感改善；读档、场景切换和第一/第三人称切换均已测试，无问题反馈。

## 0.7.2 用户实测结果

用户确认：Jiaye build 0516（原使用的 1.6.0 构建）测试正常；Jiaye build 0816 使用“手动启用”后测试正常，菜单检测版本显示正常。日期标识不等于 DLL 版本号，本次结果仅覆盖上述实测构建，不扩展为其他实验版均已验证。继续保留默认自动识别及手动覆盖。
