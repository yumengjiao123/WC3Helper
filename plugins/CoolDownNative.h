#pragma once

// 用游戏原生 UI（CSimpleFontString）在技能按钮上显示 CD 倒计时数字。
//
// 实现思路移植自 kkapi.dll 的 DzSetCommandButtonShowCooldown：
//   1. 给每个命令按钮懒创建一个游戏自己的字体框（CSimpleFontString），
//      挂成按钮的子控件，由游戏 UI 渲染树负责绘制与销毁；
//   2. 挂钩游戏驱动 CD 扫光动画的每帧回调，在回调里把剩余 CD 写入字体框。
// 这样数字完全走游戏原生渲染通道，不再需要 D3D8/D3D9/OpenGL 三套绘制代码。
//
// 目前只支持 war3 1.27a。

// 安装/卸载原生 CD 数字（内部会判断版本，非 1.27a 直接跳过）
void HookNativeCooldown();
void UnHookNativeCooldown();

// 是否启用原生 CD 数字
extern bool g_useNativeCooldown;
