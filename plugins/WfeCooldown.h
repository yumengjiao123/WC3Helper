#pragma once
#include "common.h"

struct CCommandButton;

// ---------------------------------------------------------------------------
// WFE（Warcraft Feature Extender）风格的 CD 倒计时数字。
//
// 移植自 WFEDll.dll 的 CCooldownUI：用游戏原生的 CTextFrame（576 字节）显示
// 剩余 CD，绘制完全走 war3 自己的 UI 管线，与 d3d/opengl 无关。
//
// 偏移来源：WFEDll.dll.c 42155~42209 的版本分支表（a1 为版本号）
//   a1 == 52240 (1.27a): ctor 692224 / SetText 696624 / vftable 9810000,9810276
//   a1 ==  6387 (1.24e): ctor 6367136 / SetText 6366432 / vftable 9963100,9963056
// 两个版本的对象布局一致：576 字节、文本缓冲 +0x1E8、layout 子对象 +0xB4(180)。
//
// 与旧 war3text 方案的关键区别：
//   不再 hook 按钮刷新入口 sub_6F3B5810。该 hook 会在刷新时重复创建 CTextFrame
//   （旧的挂在 CGameUI 上没人删），导致点击技能时数字闪烁/重叠。
// ---------------------------------------------------------------------------

// 每帧驱动：游戏驱动 CD 扫光动画的回调，CD 期间每帧被调用
#define WFE_127A_SWEEP_TICK 0x38FDD0 // sub_6F38FDD0
#define WFE_124E_SWEEP_TICK 0x35F170 // sub_6F35F170
// 清除入口：CD 结束 / 按钮刷新 / 按钮析构都会走这里，用于收尾隐藏
#define WFE_127A_DISPLAY_RESET 0x39A4C0 // sub_6F39A4C0
#define WFE_124E_DISPLAY_RESET 0x337E70 // sub_6F337E70

// 按版本装配偏移，成功返回 true（仅支持 1.24e / 1.27a）
bool WfeCooldownInit(Version ver);
bool WfeCooldownAvailable();

// 更新按钮上的 CD 数字。remain <= 0 表示无 CD，此时隐藏数字。
void WfeCooldownUpdate(CCommandButton *btn, float remain);

// 屏幕固定位置的文本（父对象 CGameUI，锚在屏幕左上角），用于系统信息。
// text 为 ANSI/GBK 字符串；传 nullptr 清空。内部做去重，可每帧调用。
void WfeSystemTextUpdate(const char *text);

// 卸载：隐藏并清空所有已创建的文本（对象由游戏随父 frame 释放）
void WfeCooldownShutdown();

// 是否启用（初始化成功时为 true）
extern bool g_useWfeCooldown;
