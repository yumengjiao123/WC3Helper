// 原生（游戏 UI）版技能 CD 倒计时数字
// 移植自 kkapi.dll 的 DzSetCommandButtonShowCooldown 实现，仅支持 war3 1.27a。
#include "CoolDownNative.h"
#include "CoolDownDraw.h"
#include <unordered_map>
#include <cstdio>
#include <cmath>
#include "spdlog/spdlog.h"

extern LPVOID g_gameDllBase;

void FunHook(void *pOldFuncAddr, void *pNewFuncAddr, void *&pCallBackFuncAddr);
void UnFunHook(void *pOldFuncAddr, void *pNewFuncAddr);

bool g_useNativeCooldown = true;

//==================== war3 1.27a 偏移 ====================
// 驱动 CD 扫光动画的每帧回调：
//   void __fastcall(int *button, float *outProgress, int a3)
// 由 sub_6F3B5810（按钮刷新）用 sub_6F18ED70 注册到扫光动画控制器上：
//   sub_6F18ED70(sweepController, 0, sub_6F38FDD0, button, 0)
// CD 期间每帧被调用；内部调用 sub_6F398B30 取剩余 CD 写入 *outProgress，
// CD 归零时调用 sub_6F39A4C0 停止动画并隐藏扫光。
#define OFF_127A_CD_SWEEP_TICK 0x38FDD0

// Storm#401（SMemAlloc）在 game.dll 导入表中的槽位。
// 字体框必须用它分配，游戏销毁控件时会用 Storm#403 释放，堆必须一致。
#define OFF_127A_STORM401_IAT 0x120628

#define OFF_127A_FS_CTOR 0x0BFB10	  // CSimpleFontString 构造 (mem, parent, 2, 1)，196 字节
#define OFF_127A_SETFONT 0x0C0B40	  // (fs, fontName, scale, flag)
#define OFF_127A_SETTEXT 0x0C0010	  // (fs, text, flag)
#define OFF_127A_SETPOINT 0x0BD8A0	  // (frame, point, target, targetPoint, xy, flag)
#define OFF_127A_SETCOLOR 0x0A9390	  // (fs, color, &shadowOffset[2])
#define OFF_127A_SETJUSTIFY 0x0A93E0  // (fs, flags)
#define OFF_127A_GET_FONT 0x324AD0	  // (name, 0) -> const char* 字体名
#define OFF_127A_TPL_SIZE 0x02A2F0	  // (tplTable, name, 0) -> double 模板字号
#define OFF_127A_TPL_TABLE 0xB68FB8	  // off_6FB68FB8[0]

#define FS_POINT_CENTER 4
#define FS_JUSTIFY 8

//==================== 游戏函数原型 ====================
using CdSweepTickFn = void(__fastcall *)(int *a1, float *a2, int a3);
using FontStringCtorFn = void *(__thiscall *)(void *mem, void *parent, int a3, int a4);
using SetFontFn = int(__thiscall *)(void *fs, const char *fontName, float scale, int flag);
using SetTextFn = void(__thiscall *)(void *fs, const char *text, int flag);
using SetText2Fn = void *(__thiscall *)(void *fs, const char *text);
using SetPointFn = size_t(__thiscall *)(void *frame, int point, void *target, size_t targetPoint, __int64 xy, int flag);
using SetColorFn = int(__thiscall *)(void *fs, int color, void *shadowOffset);
using SetJustifyFn = void(__thiscall *)(void *fs, int flags);
using GetFontNameFn = const char *(__fastcall *)(const char *name, int a2);
using TplSizeFn = double(__fastcall *)(int table, const char *name, unsigned int a3);
using FnSMemAlloc = void *(__stdcall *)(unsigned int size, const char *file, int line, unsigned int flags);

static CdSweepTickFn g_oCdSweepTick = nullptr;

static FontStringCtorFn pFsCtor = nullptr;
static SetFontFn pSetFont = nullptr;
static SetTextFn pSetText = nullptr;
static SetText2Fn pSetText2Fn = nullptr;
static SetPointFn pSetPoint = nullptr;
static SetColorFn pSetColor = nullptr;
static SetJustifyFn pSetJustify = nullptr;
static GetFontNameFn pGetFontName = nullptr;
static TplSizeFn pTplSize = nullptr;

static FnSMemAlloc g_sMemAlloc = nullptr; // Storm.dll SMemAlloc(序数 401)

//==================== 每个按钮的 CD 文字框 ====================
struct CdTextEntry
{
	void *fontString = nullptr; // CSimpleFontString*
	float lastCd = -1.0f;		// 上次写入的秒数，避免重复 SetText
	bool shown = false;			// 当前是否处于显示状态
};

static std::unordered_map<DWORD, CdTextEntry> g_cdTexts;
static DWORD g_btnVftable = 0; // CCommandButton::`vftable'

// 校验按钮指针仍有效（虚表必须是 CCommandButton 的虚表）
static bool IsValidButton(DWORD btn)
{
	if (!btn || IsBadReadPtr((void *)btn, 0x40))
		return false;
	DWORD vft = *(DWORD *)btn;
	if (!vft || IsBadReadPtr((void *)vft, 0x10))
		return false;
	return g_btnVftable == 0 || vft == g_btnVftable;
}

static CdTextEntry &GetEntry(DWORD btn)
{
	// 按钮被销毁后地址可能被复用，超过阈值清理掉失效项
	if (g_cdTexts.size() > 128)
	{
		for (auto it = g_cdTexts.begin(); it != g_cdTexts.end();)
		{
			if (!IsValidButton(it->first))
				it = g_cdTexts.erase(it);
			else
				++it;
		}
	}
	return g_cdTexts[btn];
}

// 取剩余 CD（与 CoolDownDraw 中的 GetButtonRemainingCd 一致：
// 用 CAbility 虚表 +732 的虚函数，按 orderId 取剩余时间）
static bool GetButtonRemainingCdNative(CCommandButton *cmdbt, float *remain)
{
	*remain = 0.0f;
	if (!cmdbt || !cmdbt->commandButtonData)
		return false;

	CAbility *abi = cmdbt->commandButtonData->ability;
	if (!abi)
		return false;

	// 跳过英雄属性等没有实际 CD 的按钮
	if (abi->id == 0 || abi->id == 'AHer' || abi->id == 'Apit' || abi->id == 'Asid' || abi->id == 'Asud')
		return false;

	using GetCdFn = float *(__thiscall *)(void *, DWORD *, DWORD);
	DWORD vtable = *(DWORD *)abi;
	if (IsBadReadPtr((void *)vtable, 800))
		return false;

	GetCdFn fn = *(GetCdFn *)(vtable + 732);
	if (!fn || IsBadReadPtr((void *)fn, 1))
		return false;

	DWORD out = 0;
	*remain = *fn((void *)abi, &out, cmdbt->commandButtonData->orderId_8);
	return *remain > 0.0f;
}

// 显示 / 隐藏（与 kkapi 一致：置 +144 标记后走虚表 +104 / +100）
static void SetFrameVisible(void *frame, bool visible)
{
	if (!frame)
		return;
	DWORD vtable = *(DWORD *)frame;
	if (!vtable || IsBadReadPtr((void *)vtable, 0x80))
		return;

	*(DWORD *)((char *)frame + 144) = visible ? 1 : 0;
	using VFn = void(__thiscall *)(void *);
	VFn fn = (VFn)(*(DWORD *)(vtable + (visible ? 104 : 100)));
	if (fn)
		fn(frame);
}

// 懒创建挂在按钮上的 CD 数字文字框
static void *CreateCdTextFrame(CCommandButton *btn)
{
	if (!pFsCtor || !pSetFont || !pGetFontName)
		return nullptr;

	void *mem = g_sMemAlloc(196, "WC3Helper", 0, 0);
	if (!mem)
	{
		return nullptr;
	}

	// 构造函数第 2 个参数是父控件：挂到按钮上，随按钮一起销毁
	void *fs = pFsCtor(mem, btn, 2, 1);
	if (!fs)
		return nullptr;

	// 字体：用游戏自己的 MasterFont，字号取 CommandButtonNumber 模板值
	const char *fontName = pGetFontName("MasterFont", 0);
	float scale = 0.0115f;
	if (pTplSize && g_gameDllBase)
	{
		int table = *(int *)((DWORD)g_gameDllBase + OFF_127A_TPL_TABLE);
		if (table)
			scale = (float)pTplSize(table, "CommandButtonNumber", 0);
	}
	if (!fontName)
		fontName = "MasterFont";

	pSetFont(fs, fontName, scale, 0);
	/*if (pSetJustify)
		pSetJustify(fs, FS_JUSTIFY);*/

	// 居中贴在按钮上
	if (pSetPoint)
	{
		__int64 xy = 0; // x = 0.0f, y = 0.0f
		pSetPoint(fs, FS_POINT_CENTER, btn, FS_POINT_CENTER, xy, 1);
	}

	// 白色文字 + 轻微阴影偏移
	//if (pSetColor)
	//{
	//	float shadow[2] = {0.001f, -0.001f};
	//	pSetColor(fs, -1, shadow); // -1 == 0xFFFFFFFF
	//}

	return fs;
}

static void UpdateCdText(CCommandButton *btn)
{
	if (!IsValidButton((DWORD)btn))
		return;

	float cd = 0.0f;
	bool hasCd = GetButtonRemainingCdNative(btn, &cd);

	CdTextEntry &entry = GetEntry((DWORD)btn);

	// 按钮被销毁后地址可能已复用，清掉失效的文字框指针
	if (entry.fontString && IsBadReadPtr(entry.fontString, 0x10))
	{spdlog::info("xxx");
		entry.fontString = nullptr;
		//entry.shown = false;
		entry.lastCd = -1.0f;
	}

	if (!hasCd)
	{
		if (entry.fontString && entry.shown)
		{
			SetFrameVisible(entry.fontString, false);
			//entry.shown = false;
			entry.lastCd = -1.0f;
		}
		return;
	}

	if (!entry.fontString)
	{
		spdlog::info("aaa");
		entry.fontString = CreateCdTextFrame(btn);
		entry.lastCd = -1.0f;
		if (!entry.fontString){
			spdlog::info("bbb");
			return;
		}
	}

	// 与上次显示值相同则跳过，避免每帧重建字形
	int shownCd = (int)(cd * 100.0f + 0.5f);
	int lastCd = (int)(entry.lastCd * 100.0f + 0.5f);
	if (shownCd != lastCd)
	{
		char buf[16];
		if (cd < 1.0f)
			_snprintf_s(buf, sizeof(buf), _TRUNCATE, "%.2f", cd);
		else
			_snprintf_s(buf, sizeof(buf), _TRUNCATE, "%d", (int)std::ceil(cd));
		if (pSetText2Fn){
			spdlog::info("666");
			pSetText2Fn(entry.fontString, buf);
		}
		entry.lastCd = cd;
	}

	if (!entry.shown)
	{
		SetFrameVisible(entry.fontString, true);
		entry.shown = true;
	}
}

//==================== hook ====================
void __fastcall MyCdSweepTick(int *a1, float *a2, int a3)
{
	// 先走原函数，保证游戏自身扫光动画不受影响
	if (g_oCdSweepTick)
		g_oCdSweepTick(a1, a2, a3);

	if (a1 && g_useNativeCooldown)
	{
		__try
		{
			UpdateCdText((CCommandButton *)a1);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
	}
}

void HookNativeCooldown()
{
	if (!g_useNativeCooldown || !g_gameDllBase)
		return;
	if (GetWar3Version() != Version::v127a)
	{
		spdlog::info("NativeCooldown: 仅支持 1.27a，跳过");
		return;
	}

	// 游戏的 operator new 是 Storm.dll 序数 401(SMemAlloc)。
	// 必须用它分配：按钮销毁时游戏会用配对的 SMemFree 释放挂在它
	// 下面的所有子对象，包括我们的 fontstring
	HMODULE hStorm = GetModuleHandleA("Storm.dll");
	if (!hStorm)
	{
		spdlog::info("hStorm is null");
		return;
	}
	g_sMemAlloc = (FnSMemAlloc)GetProcAddress(hStorm, (LPCSTR)401);
	if (!g_sMemAlloc)
	{
		spdlog::info("g_sMemAlloc is null");
		return;
	}

	DWORD base = (DWORD)g_gameDllBase;
	g_btnVftable = base + 0x98F6A8; // CCommandButton::`vftable'
	pFsCtor = (FontStringCtorFn)(base + OFF_127A_FS_CTOR);
	pSetFont = (SetFontFn)(base + OFF_127A_SETFONT);
	pSetText = (SetTextFn)(base + OFF_127A_SETTEXT);
	pSetText2Fn = (SetText2Fn)(base + 0x0C1020);
	pSetPoint = (SetPointFn)(base + OFF_127A_SETPOINT);
	pSetColor = (SetColorFn)(base + OFF_127A_SETCOLOR);
	pSetJustify = (SetJustifyFn)(base + OFF_127A_SETJUSTIFY);
	pGetFontName = (GetFontNameFn)(base + OFF_127A_GET_FONT);
	pTplSize = (TplSizeFn)(base + OFF_127A_TPL_SIZE);

	void *tickAddr = (void *)(base + OFF_127A_CD_SWEEP_TICK);
	FunHook(tickAddr, (void *)MyCdSweepTick, (void *&)g_oCdSweepTick);
	spdlog::info("NativeCooldown hooked at 0x{:X}", (DWORD)tickAddr);
}

void UnHookNativeCooldown()
{
	if (g_oCdSweepTick)
	{
		UnFunHook((void *)g_oCdSweepTick, (void *)MyCdSweepTick);
		g_oCdSweepTick = nullptr;
	}

	// 隐藏并清空所有已创建的文字框（对象本身由游戏随按钮销毁）
	for (auto &kv : g_cdTexts)
	{
		if (kv.second.fontString && kv.second.shown)
			SetFrameVisible(kv.second.fontString, false);
	}
	g_cdTexts.clear();
}
