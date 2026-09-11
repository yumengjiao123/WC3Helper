// #include "pch.h"
#define POINTER_64 __ptr64
typedef void *POINTER_64 PVOID64;
#include "CoolDownDraw.h"
#include <detours.h>
#include <shlwapi.h>
#include <vector>
#include <chrono>
#include <cstdio>
#include "jass.h"

// WFE 风格的 CD 数字 + 屏幕文本（1.24e / 1.27a，移植自 WFEDll 的 CCooldownUI）
#include "WfeCooldown.h"

// spdlog
#include "spdlog/spdlog.h"

extern LPVOID g_gameDllBase;
extern DWORD LocalHero;
extern bool g_IsPlayerObserver;

//============= 全局变量 =============
std::vector<CCommandButton *> g_ButtonQueue;

// war3 1.24e：CGameUI 全局单例指针（CGameUI 构造时赋值，析构早期清零，
// 非零即代表游戏 UI 存活）。命令按钮从这里直接枚举，无需运行时收集：
//   CGameUI+968 -> CCommandBar（448 字节，4 列 x 3 行网格 = 12 个技能按钮）
//       +292 列数 / +296 行数 / +340 行数组（每行 16 字节，+8 为列数组指针，
//       列数组每项 4 字节即 CCommandButton*）
//   CGameUI+964 -> CInfoBar（340 字节）
//       +328 -> CInventoryBar（328 字节）：+300 数量(6) / +304 按钮数组
//       （每项 8 字节，+4 为 CCommandButton*）
#define OFFSET_124E_GAME_UI_PTR 0xACBDD8
#define OFFSET_127A_GAME_UI_PTR 0xBE6350
#define OFFSET_124E_COMMANDBUTTON_VFTABLE 0x950D1C
#define OFFSET_127A_COMMANDBUTTON_VFTABLE 0x98F6A8
#define GAMEUI_INFOBAR 964
#define GAMEUI_COMMANDBAR 968
#define INFOBAR_INVENTORYBAR 328
#define INVENTORY_COUNT 300
#define INVENTORY_ARRAY 304
#define GRID_NUM_COLS 296
#define GRID_NUM_ROWS 292
#define GRID_ROW_ARRAY 340

// CD 扫光回调（CD 期间每帧驱动一次）
#define OFF_127A_CD_SWEEP_TICK 0x38FDD0
// 游戏 UI 的每帧动画 tick。它遍历某个 UI 元素的控制器列表逐个推进，
// 游戏内每帧都会走到，属于游戏自身的界面更新流程，与渲染后端无关。
// 屏幕左上角的系统信息就挂在这里刷新，彻底取代 EndScene / wglSwapLayerBuffers。
#define OFF_127A_UI_TICK 0x18F030 // 1.27a: sub_6F18F030
#define OFF_124E_UI_TICK 0x4E90A0 // 1.24e: sub_6F4E90A0（与 1.27a 同构）

bool g_hookCoolDown = false;
Version ver = Version::unknown;

// 是否显示屏幕左上角的系统信息（时间 + 观看者模式）。
// 显示走 war3 原生 UI（CTextFrame），与 d3d/opengl 无关。
bool g_showSystemInfo = true;

void FunHook(void *pOldFuncAddr, void *pNewFuncAddr, void *&pCallBackFuncAddr);
void UnFunHook(void *pOldFuncAddr, void *pNewFuncAddr);

using CdSweepTickFn = void(__fastcall *)(int *a1, float *a2, int a3);
using UiTickFn = void(__fastcall *)(void *pThis, void *edx, int dt);
using IsNeedDrawUnitOrigin = int(__thiscall *)(void *);

CdSweepTickFn g_oCdSweepTick = nullptr;
UiTickFn g_oUiTick = nullptr;
DWORD g_lastSysInfoTick = 0;

using pTargetFunc = double(__fastcall *)(DWORD pThis, int dummy);
pTargetFunc g_oRealFunc = nullptr;

// 游戏 sub_6F337E70：清除按钮 CD 显示的统一入口
// （CD 结束 sub_6F35F170 / 按钮重置 sub_6F35F150 / 按钮刷新 sub_6F369390 /
//   按钮析构 sub_6F369300 全都会走这里）
using CdDisplayResetFunc = void(__fastcall *)(DWORD pThis, DWORD dummyEdx);
CdDisplayResetFunc g_oRealCdDisplayReset = nullptr;

DWORD g_oIsDrawSkillPanel = 0;
DWORD g_oIsDrawSkillPanelOverlay = 0;
DWORD g_oIsNeedDrawUnit2 = 0;

DWORD g_DrawSkillPanelOffset = 0;
DWORD g_DrawSkillPanelOverlayOffset = 0;
DWORD g_IsNeedDrawUnitOriginOffset = 0;

DWORD g_Func6F0E8030 = 0;
DWORD g_jmpback = 0;

void UnHookCooldown();
void __fastcall MyCdSweepTick(int *a1, float *a2, int a3);
// 游戏清除按钮 CD 显示的统一入口（1.27a: sub_6F39A4C0）
void __fastcall MyCdDisplayReset(DWORD pThis, DWORD dummyEdx);
// 游戏 UI 每帧 tick（1.27a: sub_6F18F030）
void __fastcall MyUiTick(void *pThis, void *edx, int dt);

// 屏幕左上角的系统信息（时间 + 观看者模式）。
// 内容交给 war3 原生 CTextFrame 显示，这里只负责拼字符串。
void DrawSystemInfo()
{
	if (!g_useWfeCooldown)
	{
		return;
	}

	auto now = std::chrono::system_clock::now();
	auto time_t_now = std::chrono::system_clock::to_time_t(now);
	auto tm = *std::localtime(&time_t_now);

	auto wtext = std::format(L"{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d} ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	if (g_IsPlayerObserver)
	{
		wtext += L" [观看者模式]";
	}

	// 宽字符 -> 系统 ANSI(GBK)：war3 的字体按 GBK 解释字节
	char text[256] = {0};
	WideCharToMultiByte(CP_ACP, 0, wtext.c_str(), -1, text, sizeof(text), nullptr, nullptr);

	// 内部按字符串去重，重复调用不会重复 SetText
	WfeSystemTextUpdate(text);
}

// 游戏 UI 的每帧动画 tick。
// 每帧会被调用多次（每个渲染中的 UI 元素一次），所以系统信息做限流更新。
void __fastcall MyUiTick(void *pThis, void *edx, int dt)
{
	if (g_showSystemInfo)
	{
		DWORD now = GetTickCount();
		if (now - g_lastSysInfoTick >= 100)
		{
			g_lastSysInfoTick = now;
			DrawSystemInfo();
		}
	}

	if (g_oUiTick)
	{
		g_oUiTick(pThis, edx, dt);
	}
}

std::string GetAbilityFourCC(DWORD abilityID)
{
	std::string ccid = "0000";
	ccid[3] = abilityID & 0xFF;
	ccid[2] = abilityID >> 8 & 0xFF;
	ccid[1] = abilityID >> 16 & 0xFF;
	ccid[0] = abilityID >> 24 & 0xFF;
	return ccid;
}
#ifndef WC3HELPER_BASIC
int __fastcall MyIsDrawSkillPanel(unsigned char *UnitAddr, int addr1)
{
	int result;
	int GETOID;
	int OID;
	using DrawSkillPanel = int(__thiscall *)(void *, int);

	if (addr1)
	{
		GETOID = *(int *)(addr1 + 444);
		if (GETOID > 0)
			OID = *(int *)(GETOID + 8);
		else
			OID = 852290;
		result = 1;

		if (((IsNeedDrawUnitOrigin)g_IsNeedDrawUnitOriginOffset)(UnitAddr))
		{
			((DrawSkillPanel)g_DrawSkillPanelOffset)(UnitAddr, OID);
		}
		else if (IsNotBadUnit(UnitAddr))
		{
			if (!IsEnemy(UnitAddr))
			{
				((DrawSkillPanel)g_DrawSkillPanelOffset)(UnitAddr, OID);
				return result;
			}

			if (GetUnitOwnerSlot(UnitAddr) >= 12)
			{
				((DrawSkillPanel)g_DrawSkillPanelOffset)(UnitAddr, OID);
				return result;
			}

			if (MyIsPlayerObserver(MyGetLocalPlayer()))
			{
				((DrawSkillPanel)g_DrawSkillPanelOffset)(UnitAddr, OID);
				return result;
			}
		}
	}
	else
	{
		return 0;
	}
	return result;
}

int __fastcall MyIsDrawSkillPanelOverlay(unsigned char *UnitAddr, int addr1)
{
	int result; // eax@2
	int GETOID; // eax@3
	int OID;	// esi@4

	using DrawSkillPanelOverlay = int(__thiscall *)(void *, int);

	if (addr1)
	{
		GETOID = *(int *)(addr1 + 444);
		if (GETOID > 0)
			OID = *(int *)(GETOID + 8);
		else
			OID = 852290;

		result = 1;

		if (((IsNeedDrawUnitOrigin)g_IsNeedDrawUnitOriginOffset)(UnitAddr))
		{
			((DrawSkillPanelOverlay)g_DrawSkillPanelOverlayOffset)(UnitAddr, OID);
		}
		else if (IsNotBadUnit(UnitAddr))
		{
			if (!IsEnemy(UnitAddr))
			{
				((DrawSkillPanelOverlay)(g_DrawSkillPanelOverlayOffset))(UnitAddr, OID);
				return result;
			}

			if (GetUnitOwnerSlot(UnitAddr) >= 12)
			{
				((DrawSkillPanelOverlay)(g_DrawSkillPanelOverlayOffset))(UnitAddr, OID);
				return result;
			}

			if (MyIsPlayerObserver(MyGetLocalPlayer()))
			{
				((DrawSkillPanelOverlay)(g_DrawSkillPanelOverlayOffset))(UnitAddr, OID);
				return result;
			}
		}
	}
	else
	{
		result = 0;
	}
	return result;
}

int __fastcall MyIsNeedDrawUnit2(unsigned char *UnitAddr, int)
{
	using IsNeedDrawUnit2 = int(__thiscall *)(unsigned char *UnitAddr);

	if (IsNotBadUnit(UnitAddr))
	{
		if (!IsEnemy(UnitAddr))
		{
			return 1;
		}

		if (GetUnitOwnerSlot(UnitAddr) >= 12)
		{
			return 1;
		}

		if (g_IsPlayerObserver)
		{
			return 1;
		}
	}
	if (g_oIsNeedDrawUnit2)
	{
		return ((IsNeedDrawUnit2)g_oIsNeedDrawUnit2)(UnitAddr);
	}
	return 0;
}
#endif

void HookCooldown()
{
	ver = GetWar3Version();

	// 1.24e / 1.27a 走游戏原生 UI 字体框方案（WFE 风格），
	// 不需要 D3D / OpenGL 任何绘制代码。
	if (ver == Version::v124e || ver == Version::v127a)
	{
		g_useWfeCooldown = WfeCooldownInit(ver);
		if (g_useWfeCooldown)
		{
			spdlog::info("cooldown number will be drawn by WFE-style CTextFrame");

			// 游戏清除按钮 CD 显示的统一入口：
			//   CD 结束 / 按钮重置 / 按钮刷新 / 按钮析构 都会走这里。
			// 必须 hook 它：每帧扫光回调在 CD 结束、游戏停掉动画之后就不再被
			// 调用，最后一帧写进去的 "0.00"/"0.01" 没人回收。
			DWORD resetOff = (ver == Version::v127a) ? WFE_127A_DISPLAY_RESET
													 : WFE_124E_DISPLAY_RESET;
			DWORD pCdReset = (DWORD)g_gameDllBase + resetOff;
			FunHook((void *)pCdReset, (void *)MyCdDisplayReset, (void *&)g_oRealCdDisplayReset);

			// 系统信息：挂到游戏 UI 的每帧动画 tick 上，属于界面更新流程本身，
			// 不再需要 EndScene / wglSwapLayerBuffers 任何渲染时机。
			DWORD pUiTick = 0;
			if (ver == Version::v124e && g_showSystemInfo)
			{
				pUiTick = (DWORD)g_gameDllBase + OFF_124E_UI_TICK;
			}

			if (ver == Version::v127a && g_showSystemInfo)
			{
				pUiTick = (DWORD)g_gameDllBase + OFF_127A_UI_TICK;
			}

			if (pUiTick)
			{
				FunHook((void *)pUiTick, (void *)MyUiTick, (void *&)g_oUiTick);
			}
		}
	}

	g_Func6F0E8030 = (DWORD)g_gameDllBase + 0x0E8030;
	DWORD pPreSetCooldown = (DWORD)g_gameDllBase;
	DWORD pCdSweepTick = (DWORD)g_gameDllBase;

	if (ver == Version::v124e)
	{
		pPreSetCooldown += 0x3502A0; // sub_6F35F170也可以
		// 每帧驱动：sub_6F35F170（与 1.27a 的 sub_6F38FDD0 同构）
		pCdSweepTick += WFE_124E_SWEEP_TICK;
	}
	else if (ver == Version::v126a)
	{
		pPreSetCooldown += 0x34F760;
		spdlog::info("v126a");
	}
	else if (ver == Version::v127a)
	{
		pPreSetCooldown += 0x398B30;
		pCdSweepTick += OFF_127A_CD_SWEEP_TICK;
		spdlog::info("v127a");
	}
	else
	{
		return;
	}

	// FunHook((void *)pPreSetCooldown, (void *)SetCdForAddr, (void *&)g_oRealFunc);
	FunHook((void *)pCdSweepTick, (void *)MyCdSweepTick, (void *&)g_oCdSweepTick);

	g_hookCoolDown = true;
	atexit(UnHookCooldown);
}

void UnHookCooldown()
{
	if (!g_hookCoolDown)
	{
		return;
	}

	g_hookCoolDown = false;
	if (g_useWfeCooldown)
	{
		WfeCooldownShutdown();
	}
	if (g_oRealCdDisplayReset)
	{
		UnFunHook((void *)g_oRealCdDisplayReset, (void *)MyCdDisplayReset);
		g_oRealCdDisplayReset = nullptr;
	}
	if (g_oUiTick)
	{
		UnFunHook((void *)g_oUiTick, (void *)MyUiTick);
		g_oUiTick = nullptr;
	}
#ifndef WC3HELPER_BASIC
	UnFunHook((void *)g_oIsDrawSkillPanel, (void *)MyIsDrawSkillPanel);
	UnFunHook((void *)g_oIsDrawSkillPanelOverlay, (void *)MyIsDrawSkillPanelOverlay);
	UnFunHook((void *)g_oIsNeedDrawUnit2, (void *)MyIsNeedDrawUnit2);
#endif
	// UnFunHook((void *)g_oRealFunc, (void *)SetCdForAddr);
	g_ButtonQueue.clear(); // 已废弃，清空以防万一
}

// 校验按钮指针是否仍是有效的 CCommandButton。
// 参考 kkapi 的 KKCommandGetCooldownModel：拿到按钮指针后先比 vtable
// （`*a1 != game.dll + vftable偏移` 就直接返回 0），不信任任何缓存下来的按钮。
// 按钮会随 UI 刷新 / 结束任务重建，继续用旧指针会读到已释放内存。
bool IsValidCommandButton(const void *btn)
{
	if (!btn || IsBadReadPtr(btn, 4) || !g_gameDllBase)
	{
		return false;
	}

	if (ver == Version::unknown)
	{
		ver = GetWar3Version();
	}

	DWORD vftOff = (ver == Version::v127a) ? OFFSET_127A_COMMANDBUTTON_VFTABLE
										   : OFFSET_124E_COMMANDBUTTON_VFTABLE;
	return *(const DWORD *)btn == (DWORD)g_gameDllBase + vftOff;
}

// 取按钮当前技能/物品的剩余 CD（单位技能与物品自带技能通用）。
//   物品自带技能(flag2&0x600==0x200)走 abi+0xDC；
//   普通技能按 orderId 在 abi+0xCC 的命令表里找索引，
//   再按有无基础 CD 从 0x1C4/0x318 两张表里取计时器数据，
//   剩余时间 = 计时器记录的结束时间(pData+4) - 当前时间(pData2+0x40)。
// 返回 true 且 *remain > 0 表示按钮正在 CD 中。
static bool GetButtonRemainingCd(CCommandButton *cmdbt, float *remain)
{
	*remain = 0.0f;
	if (!IsValidCommandButton(cmdbt) || !cmdbt->commandButtonData)
	{
		return false;
	}

	CAbility *abi = cmdbt->commandButtonData->ability;
	if (!abi)
	{
		return false;
	}

	// 跳过英雄属性/巡逻/stop 等没有实际 CD 的按钮
	if (abi->id == 0 || abi->id == 'AHer' || abi->id == 'Apit' || abi->id == 'Asid' || abi->id == 'Asud')
	{
		return false;
	}

	// 调用虚函数
	using GetCdFn = float *(__thiscall *)(void *, DWORD *, DWORD);
	GetCdFn fn = *(GetCdFn *)(*(DWORD *)abi + 732);

	DWORD out = 0;
	if (!fn)
		return false;

	*remain = *fn((void *)abi, &out, cmdbt->commandButtonData->orderId_8);
	return *remain > 0.0f;
}

double __fastcall SetCdForAddr(DWORD pThis, int dummy)
{
	// 按钮列表由 CollectCommandButtons 从 CGameUI 全局单例直接枚举
	// （12 技能 + 6 物品栏），这里不需要再运行时收集
	return g_oRealFunc(pThis, dummy);
}

// 从 CGameUI 全局单例直接枚举全部命令按钮（12 技能 + 6 物品栏）。
// 返回 false 表示当前没有存活的游戏 UI（比如在主菜单界面）。
// 每个指针都校验 vtable == CCommandButton::vftable，防止读到已释放内存
bool CollectCommandButtons(std::vector<CCommandButton *> &out)
{
	out.clear();
	out.reserve(24); // 12 技能 + 6 物品，留余量避免反复扩容
	if (!g_gameDllBase)
	{
		return false;
	}

	DWORD gameUI = *(DWORD *)((BYTE *)g_gameDllBase + OFFSET_124E_GAME_UI_PTR);
	void *btnVftable = (void *)((BYTE *)g_gameDllBase + OFFSET_124E_COMMANDBUTTON_VFTABLE);

	if (ver == Version::unknown)
	{
		ver = GetWar3Version();
	}

	if (ver == Version::v127a)
	{
		gameUI = *(DWORD *)((BYTE *)g_gameDllBase + OFFSET_127A_GAME_UI_PTR);
		btnVftable = (void *)((BYTE *)g_gameDllBase + OFFSET_127A_COMMANDBUTTON_VFTABLE);
	}

	if (!gameUI)
	{
		return false;
	}

	// CCommandBar：4x3 网格，12 个技能按钮
	DWORD cmdBar = *(DWORD *)(gameUI + GAMEUI_COMMANDBAR);
	if (cmdBar)
	{
		DWORD numCols = *(DWORD *)(cmdBar + GRID_NUM_COLS);
		DWORD numRows = *(DWORD *)(cmdBar + GRID_NUM_ROWS);
		DWORD rowArr = *(DWORD *)(cmdBar + GRID_ROW_ARRAY);

		if (rowArr && numCols >= 1 && numCols <= 16 && numRows >= 1 && numRows <= 16)
		{
			for (DWORD r = 0; r < numRows; ++r)
			{
				DWORD colArr = *(DWORD *)(rowArr + 16 * r + 8);
				if (!colArr)
				{
					continue;
				}
				for (DWORD c = 0; c < numCols; ++c)
				{
					CCommandButton *btn = *(CCommandButton **)(colArr + 4 * c);
					if (btn && *(void **)btn == btnVftable)
					{
						out.push_back(btn);
					}
				}
			}
		}
	}

	// CInfoBar -> CInventoryBar：6 个物品栏按钮
	DWORD infoBar = *(DWORD *)(gameUI + GAMEUI_INFOBAR);
	if (infoBar)
	{
		DWORD invBar = *(DWORD *)(infoBar + INFOBAR_INVENTORYBAR);
		if (invBar)
		{
			DWORD count = *(DWORD *)(invBar + INVENTORY_COUNT);
			DWORD arr = *(DWORD *)(invBar + INVENTORY_ARRAY);
			if (arr && count >= 1 && count <= 16)
			{
				for (DWORD i = 0; i < count; ++i)
				{
					CCommandButton *btn = *(CCommandButton **)(arr + 8 * i + 4);
					if (btn && *(void **)btn == btnVftable)
					{
						out.push_back(btn);
					}
				}
			}
		}
	}
	return !out.empty();
}

// 游戏清除按钮 CD 显示的统一入口（1.27a: sub_6F39A4C0，1.24e: sub_6F337E70）。
// CD 结束 / 按钮重置 / 按钮刷新 / 按钮析构都会走这里，是收尾隐藏数字的唯一可靠时机。
void __fastcall MyCdDisplayReset(DWORD pThis, DWORD dummyEdx)
{
	if (g_oRealCdDisplayReset)
	{
		g_oRealCdDisplayReset(pThis, dummyEdx);
	}

	// pThis 在按钮析构时也会走到这里，此时 vtable 已经换了；校验不过就不碰它
	if (!pThis || !g_useWfeCooldown || !IsValidCommandButton((const void *)pThis))
	{
		return;
	}

	// 只在 CD 真的结束时才隐藏。
	// 这个入口不只是 CD 结束才走：按钮刷新(sub_6F3B5810 开头)也会调它，
	// 无条件隐藏会让"点击技能"时数字闪一下（Hide 与下一帧 Show 之间空一帧）。
	float remaining = 0.0f;
	if (!GetButtonRemainingCd((CCommandButton *)pThis, &remaining))
	{
		WfeCooldownUpdate((CCommandButton *)pThis, 0.0f);
	}
}

void __fastcall MyCdSweepTick(int *a1, float *a2, int a3)
{
	// 先走原函数，保证游戏自身扫光动画不受影响
	if (g_oCdSweepTick)
		g_oCdSweepTick(a1, a2, a3);

	if (!a1 || !g_useWfeCooldown)
	{
		return;
	}

	// 每次都重新枚举：按钮会随 UI 刷新 / 结束任务重建，
	// 沿用上一帧缓存的指针会在按钮销毁后读到已释放内存。
	CollectCommandButtons(g_ButtonQueue);

	for (auto cmdbt : g_ButtonQueue)
	{
		float remaining = 0.0f;
		// 无 CD 或只剩尾巴（<0.05s）时传 0 隐藏，避免出现 "0.00"/"0.01"
		if (GetButtonRemainingCd(cmdbt, &remaining) && remaining > 0.05f)
		{
			WfeCooldownUpdate(cmdbt, remaining);
		}
		else
		{
			WfeCooldownUpdate(cmdbt, 0.0f);
		}
	}
}
