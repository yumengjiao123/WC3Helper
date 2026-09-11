// WFE（Warcraft Feature Extender）风格的 CD 倒计时数字
// 支持 1.24e 与 1.27a，偏移取自 WFEDll.dll.c 42155~42209 的版本分支表。
#include "WfeCooldown.h"
#include "CoolDownDraw.h"
#include <unordered_map>
#include <cstdio>
#include <cmath>

extern LPVOID g_gameDllBase;
extern LPVOID g_stormDllBase;

//==================== 宏定义区 ====================
// --- 函数偏移：1.27a ---
#define O127_CTOR 0x0A9000		// sub_6F0A9000  CTextFrame 构造器
#define O127_SETTEXT 0x0AA130	// sub_6F0AA130  SetText
#define O127_SETFONT 0x09CE60	// sub_6F09CE60  SetFont
#define O127_SETALLPTS 0x0BD750 // sub_6F0BD750  SetAllPoints
#define O127_SETPOINT 0x0BD8A0	// sub_6F0BD8A0  SetPoint
#define O127_SHOW 0x0A9FC0		// sub_6F0A9FC0
#define O127_WRAP 0x0AA2B0		// sub_6F0AA2B0
#define O127_SETCOLOR 0x0A9390	// sub_6F0A9390  SetColor
#define O127_SETJUST 0x0A93E0	// sub_6F0A93E0  SetJustification
#define O127_SKINSTR 0x324AD0	// sub_6F324AD0  取字体名
#define O127_GAMEUI 0xBE6350	// CGameUI 全局单例指针所在地址

// --- 函数偏移：1.24e（与 1.27a 一一对应，地址不同）---
#define O124_CTOR 0x6127A0		// sub_6F6127A0
#define O124_SETTEXT 0x6124E0	// sub_6F6124E0
#define O124_SETFONT 0x5FC100	// sub_6F5FC100
#define O124_SETALLPTS 0x606F90 // sub_6F606F90
#define O124_SETPOINT 0x606F10	// sub_6F606F10
#define O124_SHOW 0x611B10		// sub_6F611B10
#define O124_WRAP 0x611B30		// sub_6F611B30
#define O124_SETCOLOR 0x612730	// sub_6F612730
#define O124_SETJUST 0x612650	// sub_6F612650
#define O124_SKINSTR 0x320070	// sub_6F320070
#define O124_GAMEUI 0xACBDD8	// CGameUI 全局单例指针所在地址

#define WFE_FRAME_SIZE 576	// CTextFrame 大小
#define WFE_LAYOUT_PART 180 // layout 子对象偏移（SetAllPoints 用 frame+180）
#define WFE_TEXT_BUF 0x1E8	// +488：当前文本指针（NULL = 空）

// --- WFE COOLDOWNUI 配置节默认值 ---
#define WFE_TEXT_COLOUR (-1)		  // 0xFFFFFFFF 白色
#define WFE_SHADOW_COLOUR (-16777216) // 0xFF000000 黑色
#define WFE_TEXT_SIZE 0.017f		  // TEXTSIZE，WFE 里是 百分比/100
#define WFE_JUSTIFY 7

// --- 屏幕固定文本（系统信息）的锚点与样式 ---
// war3 UI 坐标：左上角 (0,0)，右下角约 (0.8,0.6)；y 轴向上，所以往下用负值
#define SYS_FRAME_POINT 0 // TOPLEFT
#define SYS_FRAME_X 0.048f
#define SYS_FRAME_Y (-0.041f)
#define SYS_FRAME_SIZE 0.014f
#define SYS_FRAME_COLOUR (-16777216)

using FnTfCtor = void *(__thiscall *)(void *mem, void *parent, int a3, int a4);
using FnTfSetText = void *(__thiscall *)(void *frame, const char *text);
using FnTfSetFont = void *(__thiscall *)(void *frame, const char *fontName, float height, int flags);
using FnSetAllPoints = void *(__thiscall *)(void *frame, void *relFrame, int update);
using FnSetPoint = void *(__thiscall *)(void *frame, int myPoint, void *relFrame, int relPoint,
										float x, float y, int update);
using FnTfFlag = void *(__thiscall *)(void *frame, int a2);
using FnTfSetColor = void *(__thiscall *)(void *frame, int color, void *shadow);
using FnTfSetJustify = void *(__thiscall *)(void *frame, int flags);
using FnSkinGetString = const char *(__fastcall *)(const char *name, void *zero);
using FnSMemAlloc = void *(__stdcall *)(unsigned int size, const char *file, int line, unsigned int flags);

static FnTfCtor g_tfCtor = nullptr;
static FnTfSetText g_tfSetText = nullptr;
static FnTfSetFont g_tfSetFont = nullptr;
static FnSetAllPoints g_setAllPoints = nullptr;
static FnSetPoint g_setPoint = nullptr;
static FnTfFlag g_tfShow = nullptr;
static FnTfFlag g_tfWrap = nullptr;
static FnTfSetColor g_tfSetColor = nullptr;
static FnTfSetJustify g_tfSetJustify = nullptr;
static FnSkinGetString g_skinGetString = nullptr;
static FnSMemAlloc g_sMemAlloc = nullptr;
static DWORD g_gameUiAddr = 0;

bool g_useWfeCooldown = false;

// button -> CTextFrame。CTextFrame 挂在 CGameUI 下，不在按钮的子 frame 链表里，
// 无法遍历找回，只能缓存。
static std::unordered_map<DWORD, void *> g_frames;

// 屏幕左上角的系统信息文本（同样挂在 CGameUI 下）
static void *g_sysText = nullptr;

// 创建这些文本时依附的 CGameUI 实例。
// 游戏 UI 会随"结束任务 / 回主菜单"销毁重建，挂在旧 CGameUI 下的 CTextFrame
// 会被一起释放。不检查就会在 UI 换过之后继续写已释放内存（结束任务时崩溃）。
static DWORD g_boundGameUI = 0;

// 校验当前游戏 UI 是否还是创建这些文本时的那个实例。
// 换过（或当前没有 UI）就把所有缓存指针整批作废。
// 返回 0 表示此刻不能安全操作任何文本。
static DWORD AcquireGameUI()
{
	DWORD cur = g_gameUiAddr ? *(DWORD *)g_gameUiAddr : 0;
	if (cur != g_boundGameUI)
	{
		// 旧 CGameUI 已析构（或正在析构），挂在它下面的 CTextFrame 全部失效
		g_frames.clear();
		g_sysText = nullptr;
		g_boundGameUI = cur;
	}
	return cur;
}

bool WfeCooldownInit(Version ver)
{
	if (!g_gameDllBase || !g_stormDllBase)
	{
		return false;
	}

	g_sMemAlloc = (FnSMemAlloc)GetProcAddress((HMODULE)g_stormDllBase, (LPCSTR)401);
	if (!g_sMemAlloc)
	{
		return false;
	}

	DWORD base = (DWORD)g_gameDllBase;
	if (ver == Version::v127a)
	{
		g_tfCtor = (FnTfCtor)(base + O127_CTOR);
		g_tfSetText = (FnTfSetText)(base + O127_SETTEXT);
		g_tfSetFont = (FnTfSetFont)(base + O127_SETFONT);
		g_setAllPoints = (FnSetAllPoints)(base + O127_SETALLPTS);
		g_setPoint = (FnSetPoint)(base + O127_SETPOINT);
		g_tfShow = (FnTfFlag)(base + O127_SHOW);
		g_tfWrap = (FnTfFlag)(base + O127_WRAP);
		g_tfSetColor = (FnTfSetColor)(base + O127_SETCOLOR);
		g_tfSetJustify = (FnTfSetJustify)(base + O127_SETJUST);
		g_skinGetString = (FnSkinGetString)(base + O127_SKINSTR);
		g_gameUiAddr = base + O127_GAMEUI;
	}
	else if (ver == Version::v124e)
	{
		g_tfCtor = (FnTfCtor)(base + O124_CTOR);
		g_tfSetText = (FnTfSetText)(base + O124_SETTEXT);
		g_tfSetFont = (FnTfSetFont)(base + O124_SETFONT);
		g_setAllPoints = (FnSetAllPoints)(base + O124_SETALLPTS);
		g_setPoint = (FnSetPoint)(base + O124_SETPOINT);
		g_tfShow = (FnTfFlag)(base + O124_SHOW);
		g_tfWrap = (FnTfFlag)(base + O124_WRAP);
		g_tfSetColor = (FnTfSetColor)(base + O124_SETCOLOR);
		g_tfSetJustify = (FnTfSetJustify)(base + O124_SETJUST);
		g_skinGetString = (FnSkinGetString)(base + O124_SKINSTR);
		g_gameUiAddr = base + O124_GAMEUI;
	}
	else
	{
		return false;
	}

	return WfeCooldownAvailable();
}

bool WfeCooldownAvailable()
{
	return g_tfCtor && g_tfSetText && g_tfSetFont && g_setAllPoints &&
		   g_tfShow && g_tfWrap && g_tfSetColor && g_tfSetJustify &&
		   g_skinGetString && g_sMemAlloc;
}

// 按游戏自己的 CTextFrame 创建序列建一个文本控件
static void *CreateFrame(CCommandButton *btn)
{
	// 父对象用 CGameUI 单例：CTextFrame 不是按钮的子 frame，
	// 不受按钮子 frame 绘制顺序影响，位置由 SetAllPoints 锚定到按钮。
	DWORD gameUI = g_gameUiAddr ? *(DWORD *)g_gameUiAddr : 0;
	if (!gameUI)
	{
		return nullptr;
	}

	void *mem = g_sMemAlloc(WFE_FRAME_SIZE, "WC3Helper", 0, 0);
	if (!mem)
	{
		return nullptr;
	}
	void *tf = g_tfCtor(mem, (void *)gameUI, 0, 0);
	if (!tf)
	{
		return nullptr;
	}

	const char *fontName = g_skinGetString("MasterFont", nullptr);
	if (g_tfSetFont)
	{
		g_tfSetFont(tf, fontName ? fontName : "MasterFont", WFE_TEXT_SIZE, 0);
	}

	// 锚定目标：优先父对象的 layout 子对象(+180)，不可读则退回原指针
	void *rel = (char *)btn + WFE_LAYOUT_PART;
	if (IsBadReadPtr(rel, 4) || IsBadReadPtr(*(void **)rel, 4))
	{
		rel = btn;
	}
	if (g_setAllPoints)
	{
		g_setAllPoints((char *)tf + WFE_LAYOUT_PART, rel, 1);
	}

	// *(frame+12) |= *(frame+16) | 2
	*(DWORD *)((char *)tf + 12) |= *(DWORD *)((char *)tf + 16) | 2;

	if (g_tfShow)
	{
		g_tfShow(tf, 1);
	}
	if (g_tfWrap)
	{
		g_tfWrap(tf, 1);
	}
	if (g_tfSetColor)
	{
		float shadow[2] = {0.0016f, -0.0016f};
		g_tfSetColor(tf, WFE_SHADOW_COLOUR, shadow);
	}
	if (g_tfSetJustify)
	{
		g_tfSetJustify(tf, WFE_JUSTIFY);
	}

	return tf;
}

static void *GetFrame(CCommandButton *btn)
{
	auto it = g_frames.find((DWORD)btn);
	if (it == g_frames.end())
	{
		return nullptr;
	}
	if (IsBadReadPtr(it->second, 0x10))
	{
		g_frames.erase(it);
		return nullptr;
	}
	return it->second;
}

void WfeCooldownUpdate(CCommandButton *btn, float remain)
{
	if (!btn || !WfeCooldownAvailable() || !AcquireGameUI())
	{
		return;
	}

	void *tf = GetFrame(btn);

	if (remain <= 0.0f)
	{
		// 无 CD：清空文本
		if (tf && *(const char **)((BYTE *)tf + WFE_TEXT_BUF))
		{
			g_tfSetText(tf, nullptr);
		}
		return;
	}

	if (!tf)
	{
		tf = CreateFrame(btn);
		if (!tf)
		{
			return;
		}
		g_frames[(DWORD)btn] = tf;
	}

	char text[16];
	if (remain < 1.0f)
	{
		_snprintf_s(text, sizeof(text), _TRUNCATE, "%.2f", remain);
	}
	else
	{
		_snprintf_s(text, sizeof(text), "%d", (int)std::ceil(remain));
	}

	// 值没变就不重复 SetText
	const char *cur = *(const char **)((BYTE *)tf + WFE_TEXT_BUF);
	if (!cur || strcmp(cur, text) != 0)
	{
		g_tfSetText(tf, text);
	}
}

//==================== 屏幕固定文本（系统信息）====================
// 位置/样式宏见文件顶部「宏定义区」

// 建一个挂在 CGameUI 上、锚在屏幕左上角的文本
static void *CreateScreenText()
{
	DWORD gameUI = g_gameUiAddr ? *(DWORD *)g_gameUiAddr : 0;
	if (!gameUI || !g_setPoint)
	{
		return nullptr;
	}

	void *mem = g_sMemAlloc(WFE_FRAME_SIZE, "WC3Helper", 0, 0);
	if (!mem)
	{
		return nullptr;
	}
	void *tf = g_tfCtor(mem, (void *)gameUI, 0, 0);
	if (!tf)
	{
		return nullptr;
	}

	const char *fontName = g_skinGetString("MasterFont", nullptr);
	g_tfSetFont(tf, fontName ? fontName : "MasterFont", SYS_FRAME_SIZE, 0);

	// 锚到 CGameUI 的左上角（layout 子对象在 +180）
	void *rel = (void *)(gameUI + WFE_LAYOUT_PART);
	if (IsBadReadPtr(rel, 4) || IsBadReadPtr(*(void **)rel, 4))
	{
		rel = (void *)gameUI;
	}
	g_setPoint((char *)tf + WFE_LAYOUT_PART, SYS_FRAME_POINT, rel, SYS_FRAME_POINT,
			   SYS_FRAME_X, SYS_FRAME_Y, 1);

	*(DWORD *)((char *)tf + 12) |= *(DWORD *)((char *)tf + 16) | 2;
	g_tfShow(tf, 1);
	g_tfWrap(tf, 1);
	float shadow[2] = {0.0016f, -0.0016f};
	g_tfSetColor(tf, WFE_SHADOW_COLOUR, shadow);
	g_tfSetJustify(tf, WFE_JUSTIFY);

	return tf;
}

void WfeSystemTextUpdate(const char *text)
{
	if (!WfeCooldownAvailable() || !AcquireGameUI())
	{
		return;
	}

	if (!text)
	{
		if (g_sysText && *(const char **)((BYTE *)g_sysText + WFE_TEXT_BUF))
		{
			g_tfSetText(g_sysText, nullptr);
		}
		return;
	}

	if (!g_sysText)
	{
		g_sysText = CreateScreenText();
		if (!g_sysText)
		{
			return;
		}
	}
	else if (IsBadReadPtr(g_sysText, 0x10))
	{
		g_sysText = nullptr;
		return;
	}

	const char *cur = *(const char **)((BYTE *)g_sysText + WFE_TEXT_BUF);
	if (!cur || strcmp(cur, text) != 0)
	{
		g_tfSetText(g_sysText, text);
	}
}

void WfeCooldownShutdown()
{
	// 只有游戏 UI 还是我们建这些文本时的那个实例、且地址本身还可读，
	// 才能安全地去清空文本；否则那些指针早就随旧 CGameUI 一起释放了。
	DWORD cur = 0;
	if (g_gameUiAddr && !IsBadReadPtr((void *)g_gameUiAddr, 4))
	{
		cur = *(DWORD *)g_gameUiAddr;
	}

	if (cur && cur == g_boundGameUI)
	{
		for (auto &kv : g_frames)
		{
			void *tf = kv.second;
			if (tf && !IsBadReadPtr(tf, 0x10) && *(const char **)((BYTE *)tf + WFE_TEXT_BUF))
			{
				g_tfSetText(tf, nullptr);
			}
		}

		if (g_sysText && !IsBadReadPtr(g_sysText, 0x10) &&
			*(const char **)((BYTE *)g_sysText + WFE_TEXT_BUF))
		{
			g_tfSetText(g_sysText, nullptr);
		}
	}

	g_frames.clear();
	g_sysText = nullptr;
	g_boundGameUI = 0;

	g_useWfeCooldown = false;
}
