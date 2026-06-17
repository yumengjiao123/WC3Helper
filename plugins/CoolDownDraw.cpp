// #include "pch.h"
#define POINTER_64 __ptr64
typedef void *POINTER_64 PVOID64;
#include "CoolDownDraw.h"
#include <detours.h>
#include <shlwapi.h>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <chrono>
#include "jass.h"

#include <d3dx8.h>

#define GL_GLEXT_PROTOTYPES
#include <gl\gl.h>
#include <gl\glu.h>
#include <wglext.h>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#include <ft2build.h>
#include FT_FREETYPE_H

// spdlog
#include "spdlog/spdlog.h"

extern LPVOID g_gameDllBase;
extern DWORD LocalHero;
extern HWND hWnd;
extern bool g_IsPlayerObserver;

// 单个字符缓存
struct CharGlyph
{
	union
	{
		GLuint t1; // 字符纹理ID
		LPDIRECT3DTEXTURE8 t2;
	} tex;
	int w, h;		  // 字形宽高
	int bearingX;	  // 左偏移
	int bearingY;	  // 上偏移
	int advance;	  // 字符横向步进
	float uMax, vMax; // 新增：字形在幂次纹理内的UV边界
	CharGlyph()
	{
		tex.t1 = 0;
	}
};

// 新增：文字绘制缓存结构体
struct TextDrawItem
{
	float x;
	float y;
	DWORD color;
	std::wstring text;
	bool bAdj;
	bool bCenter;
	int fontSize;									 // 屏幕像素字体大小（1024x768 空间），0 表示使用默认值
	std::chrono::steady_clock::time_point timestamp; // 入队时间，用于过期过滤
};

struct D3DStateBackup
{
	DWORD oldZWrite, oldZTest, oldBlend, oldAlphaBlendOp;
	DWORD oldSrcBlend, oldDestBlend;
	DWORD oldCullMode, oldLighting, oldAlphaTest, oldFog;
	DWORD oldVertexShader;
	LPDIRECT3DTEXTURE8 oldTexture;
	// 纹理阶段状态 - 颜色操作
	DWORD oldColorOp, oldColorArg1, oldColorArg2;
	// 纹理阶段状态 - Alpha操作
	DWORD oldAlphaOp, oldAlphaArg1, oldAlphaArg2;
	// 纹理过滤和寻址
	DWORD oldMinFilter, oldMagFilter, oldMipFilter;
	DWORD oldAddressU, oldAddressV;
	D3DMATRIX oldProj, oldWorld;
};

//============= 全局变量 =============

// 全局文字缓存，每帧清空重绘
std::vector<TextDrawItem> g_textDrawQueue;

FT_Library g_ftLib = nullptr;
FT_Face g_ftFace = nullptr;
std::unordered_map<DWORD, CharGlyph> g_charCache;
LPDIRECT3DDEVICE8 g_pDevice = NULL;

bool g_init = false;
bool bIsOpenGL = false;
bool DirectxHookInitialized = false;
bool vsyncInitialized = false;
bool resetcalled = false;
bool OLD_D3D_PARAMETERS_LOADED = false;

void FunHook(void *pOldFuncAddr, void *pNewFuncAddr, void *&pCallBackFuncAddr);
void UnFunHook(void *pOldFuncAddr, void *pNewFuncAddr);

using D3DReset = HRESULT(__stdcall *)(LPDIRECT3DDEVICE8, D3DPRESENT_PARAMETERS *);
using EndScene = HRESULT(__fastcall *)(int);
using WGLSwapLayerBuffers = int(__stdcall *)(HDC, unsigned int);
using Present = HRESULT(__stdcall *)(LPDIRECT3DDEVICE8, CONST RECT *, CONST RECT *, HWND, CONST RGNDATA *);
using IsNeedDrawUnitOrigin = int(__thiscall *)(void *);

using pTargetFunc = void(__fastcall *)(DWORD pThis, int dummy);
pTargetFunc RealFunc = nullptr;

D3DReset g_oD3dReset = nullptr;
EndScene g_oEndScene = nullptr;

Present g_oPresent = nullptr;
WGLSwapLayerBuffers g_oWglSwapLayerBuffers = nullptr;

DWORD g_oIsDrawSkillPanel = 0;
DWORD g_oIsDrawSkillPanelOverlay = 0;
DWORD g_oIsNeedDrawUnit2 = 0;

DWORD g_DrawSkillPanelOffset = 0;
DWORD g_DrawSkillPanelOverlayOffset = 0;
DWORD g_IsNeedDrawUnitOriginOffset = 0;

DWORD g_jmpback = 0;

HGLRC War3GlobalOverlay_OPENGL = NULL;
HDC GlobalDc = NULL;

HWND GetGameWindow();
void DrawOverlayText(float x, float y, DWORD dwColor, const wchar_t *text, float scale = 1.0f, bool bCenter = false, int fontSize = 22);
void DrawTextD3D8(float x, float y, DWORD color, const wchar_t *text, bool bCenter = false, int fontSize = 22);
bool InitFreeType(int fontSize = 22);
void SaveD3DState(LPDIRECT3DDEVICE8 pDev, D3DStateBackup &outState);
void RestoreD3DState(LPDIRECT3DDEVICE8 pDev, D3DStateBackup &state);
void Setup2DOrtho(LPDIRECT3DDEVICE8 pDev);

void __fastcall SetCdForAddr(DWORD pThis, int dummy);

static void DrawTextToScreen(float x, float y, const wchar_t *text, DWORD color, bool bAdj = true, bool bCenter = false, int fontSize = 0)
{
	// 窗口最小化时跳过入队，避免无意义堆积
	if (hWnd && IsIconic(hWnd))
		return;

	// 上限保护：队列积压超过500条时直接清空（兜底）
	if (g_textDrawQueue.size() > 60)
		g_textDrawQueue.clear();

	// 不再直接绘制，只加入缓存队列
	g_textDrawQueue.push_back({x, y, color, text, bAdj, bCenter, fontSize, std::chrono::steady_clock::now()});
}

static void DrawTextToScreenReal(float x, float y, const wchar_t *text, DWORD color, bool bAdj, bool bCenter = false, int fontSize = 0)
{
	if (!hWnd)
	{
		hWnd = GetGameWindow();
	}
	if (!hWnd || (!bIsOpenGL && !g_pDevice))
		return;

	RECT rc_wnd;
	GetClientRect(hWnd, &rc_wnd);
	// 计算宽高
	auto iWidth = rc_wnd.right - rc_wnd.left;
	auto iHeight = rc_wnd.bottom - rc_wnd.top;

	float scaleX = 1.00f;
	float scaleY = 1.00f;

	float rx = x, ry = y;
	if (bAdj)
	{
		if (bIsOpenGL)
		{
			GLint viewport[4];
			glGetIntegerv(GL_VIEWPORT, viewport);
			scaleX = viewport[2] / 1024.0f;
			scaleY = viewport[3] / 768.0f;
			rx *= scaleX;
			ry *= scaleY;
		}
		else
		{
			scaleX = iWidth / 1024.0f;
			scaleY = iHeight / 768.0f;
			rx = x * scaleX;
			ry = y * scaleY;
		}
	}

	// fontSize==0 表示默认22，否则按窗口缩放比例换算为实际像素字体大小
	const float pixelScale = scaleX > scaleY ? scaleX : scaleY;
	int realFontSize;
	if (fontSize <= 0)
	{
		realFontSize = (int)(22 * pixelScale + 0.5f);
	}
	else
	{
		// fontSize 是 1024x768 空间下的像素大小，换算到实际屏幕像素
		realFontSize = (int)(fontSize * pixelScale + 0.5f);
	}
	if (realFontSize < 6)
		realFontSize = 6;

	if (bIsOpenGL)
	{
		DrawOverlayText(rx, ry, color, text, 1.00f, bCenter, realFontSize);
	}
	else
	{
#if 1
		DrawTextD3D8(rx, ry, color, text, bCenter, realFontSize);
#else
		RECT rc = {(LONG)rx, (LONG)ry, 0, 0};
		g_pFont->DrawText(text, -1, &rc, DT_NOCLIP, color);
#endif
	}
}

void DrawSystemInfo()
{
	auto now = std::chrono::system_clock::now();
	auto time_t_now = std::chrono::system_clock::to_time_t(now);
	auto tm = *std::localtime(&time_t_now);

	RECT rc;
	// 获取窗口【客户区】坐标（左上角、右下角）
	GetClientRect(hWnd, &rc);
	// 计算宽高
	auto iWidth = rc.right - rc.left;
	auto iHeight = rc.bottom - rc.top;

	auto text = std::format(L"{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d} ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	if (g_IsPlayerObserver)
	{
		text += L" [观看者模式]";
	}

	if (bIsOpenGL)
	{
		DrawTextToScreenReal(iWidth * 0.06f, iHeight * 0.068f, text.c_str(), 0xFFEFB60B, false);
	}
	else
	{
		DrawTextToScreenReal(iWidth * 0.06f, iHeight * 0.068f, text.c_str(), 0xFFEFB60B, false);
	}
}

HRESULT STDMETHODCALLTYPE MyD3D8Reset(LPDIRECT3DDEVICE8 device, D3DPRESENT_PARAMETERS *parameters)
{
	if (!g_oD3dReset)
	{
		spdlog::info("FATAL ERROR IN MyD3DReset");
		return 0;
	}

	if (resetcalled)
		return g_oD3dReset(device, parameters);

	resetcalled = true;
	HRESULT hr;

	if (!OLD_D3D_PARAMETERS_LOADED)
	{
		OLD_D3D_PARAMETERS_LOADED = true;
	}

	parameters->FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
	parameters->FullScreen_RefreshRateInHz = D3DPRESENT_RATE_UNLIMITED;

	hr = g_oD3dReset(device, parameters);
	resetcalled = false;
	spdlog::info("MyD3D8Reset called");
	return hr;
}

// d3d8下的方案
HRESULT __fastcall MyEndScene(int GlobalWc3Data)
{
	IDirect3DDevice8 *pDevice = *(IDirect3DDevice8 **)(GlobalWc3Data + 1412);
	if (!pDevice || pDevice->TestCooperativeLevel() != D3D_OK)
	{
		spdlog::info("pDevice is Null");
		g_textDrawQueue.clear();
		return g_oEndScene(GlobalWc3Data);
	}

	if (!g_init && pDevice != NULL)
	{
		g_pDevice = pDevice;
		g_init = true;
		if (g_pDevice && !vsyncInitialized)
		{
			void **pVTable = *(void ***)g_pDevice;
			DWORD D3dReset_org = (DWORD)pVTable[14];
			if (D3dReset_org)
			{
				FunHook((void *)D3dReset_org, (void *)MyD3D8Reset, (void *&)g_oD3dReset);
				spdlog::info("D3D RESET SUCCESS HOOKED");
				vsyncInitialized = true;
			}
		}
	}

	DrawSystemInfo();
	// 绘制所有缓存的技能冷却文字，只保留 1 秒内的数据
	auto now = std::chrono::steady_clock::now();
	for (const auto &item : g_textDrawQueue)
	{
		if (std::chrono::duration_cast<std::chrono::milliseconds>(now - item.timestamp).count() < 1000)
			DrawTextToScreenReal(item.x, item.y, item.text.c_str(), item.color, item.bAdj, item.bCenter, item.fontSize);
	}
	g_textDrawQueue.clear();
	return pDevice->EndScene();
}

bool InitFreeType(int fontSize)
{
	// 初始化FT库
	FT_Error err = FT_Init_FreeType(&g_ftLib);
	if (err)
		return false;

	// 加载系统字体，路径自行替换（微软雅黑）
	const char *fontPath = "C:/Windows/Fonts/msyhbd.ttc";
	if (!std::filesystem::exists(fontPath))
	{
		fontPath = "C:/Windows/Fonts/msyhbd.ttf";
	}

	err = FT_New_Face(g_ftLib, fontPath, 0, &g_ftFace);
	if (err)
	{
		return false;
	}

	// 设置像素字号
	FT_Set_Pixel_Sizes(g_ftFace, 0, fontSize);

	// 纹理像素对齐（OpenGL灰度单通道）
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	return true;
}

void FreeFreeTypeRes()
{
	for (auto &kv : g_charCache)
	{
		if (bIsOpenGL)
		{
			if (kv.second.tex.t1 != 0)
				glDeleteTextures(1, &kv.second.tex.t1);
		}
		else
		{
			if (kv.second.tex.t2 != nullptr)
				kv.second.tex.t2->Release();
		}
	}
	if (g_ftFace)
		FT_Done_Face(g_ftFace);
	if (g_ftLib)
		FT_Done_FreeType(g_ftLib);
}

CharGlyph LoadGlyph(wchar_t ch, int fontSize)
{
	unsigned int idx = ch;
	if (g_charCache.contains(idx))
		return g_charCache[idx];

	// 设置像素字号
	FT_Set_Pixel_Sizes(g_ftFace, 0, fontSize);

	FT_Error err = FT_Load_Char(g_ftFace, ch, FT_LOAD_RENDER);
	if (err)
	{
		CharGlyph empty{};
		g_charCache[idx] = empty;
		return empty;
	}
	FT_GlyphSlot slot = g_ftFace->glyph;
	int w = slot->bitmap.width;
	int h = slot->bitmap.rows;

	CharGlyph glyph{};
	glyph.w = w;
	glyph.h = h;
	glyph.bearingX = slot->bitmap_left;
	glyph.bearingY = slot->bitmap_top;
	glyph.advance = slot->advance.x >> 6;

	if (w == 0 || h == 0)
	{
		g_charCache[idx] = glyph;
		return glyph;
	}

	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	auto nextPow2 = [](int n)
	{
		int res = 1;
		while (res < n)
			res <<= 1;
		return res;
	};
	int texW = nextPow2(w);
	int texH = nextPow2(h);
	std::vector<unsigned char> texData(texW * texH * 2, 0);

	// 逐行拷贝，不要修改y顺序
	for (int y = 0; y < h; y++)
	{
		unsigned char *srcRow = slot->bitmap.buffer + y * slot->bitmap.pitch;
		unsigned char *dstRow = &texData[y * texW * 2];
		for (int x = 0; x < w; x++)
		{
			unsigned char gray = srcRow[x];
			dstRow[x * 2] = gray;
			dstRow[x * 2 + 1] = gray;
		}
	}

	glTexImage2D(
		GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA,
		texW, texH, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE,
		texData.data());

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // GL_NEAREST
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glyph.tex.t1 = tex;
	glyph.uMax = (float)w / texW;
	glyph.vMax = (float)h / texH;
	g_charCache[idx] = glyph;
	return glyph;
}

CharGlyph LoadGlyphD3D(wchar_t ch, int fontSize)
{
	unsigned int idx = ch;
	if (g_charCache.contains(idx))
		return g_charCache[idx];

	// 设置像素字号
	FT_Set_Pixel_Sizes(g_ftFace, 0, fontSize);

	FT_Error err = FT_Load_Char(g_ftFace, ch, FT_LOAD_RENDER);
	if (err)
	{
		CharGlyph empty{};
		g_charCache[idx] = empty;
		return empty;
	}

	FT_GlyphSlot slot = g_ftFace->glyph;
	int w = slot->bitmap.width;
	int h = slot->bitmap.rows;

	CharGlyph glyph{};
	glyph.w = w;
	glyph.h = h;
	glyph.bearingX = slot->bitmap_left;
	glyph.bearingY = slot->bitmap_top;
	glyph.advance = slot->advance.x >> 6;

	if (w == 0 || h == 0)
	{
		g_charCache[idx] = glyph;
		return glyph;
	}

	auto nextPow2 = [](int n)
	{
		int res = 1;
		while (res < n)
			res <<= 1;
		return res;
	};
	int texW = nextPow2(w);
	int texH = nextPow2(h);

	LPDIRECT3DTEXTURE8 tex = nullptr;
	HRESULT hr = g_pDevice->CreateTexture(texW, texH, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex);
	if (FAILED(hr))
	{
		g_charCache[idx] = glyph;
		return glyph;
	}

	D3DLOCKED_RECT lock;
	hr = tex->LockRect(0, &lock, nullptr, 0);
	if (FAILED(hr))
	{
		tex->Release();
		g_charCache[idx] = glyph;
		return glyph;
	}

	BYTE *pDst = (BYTE *)lock.pBits;
	BYTE *pSrc = slot->bitmap.buffer;
	int pitch = lock.Pitch;
	ZeroMemory(pDst, pitch * texH);
	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			BYTE alpha = pSrc[y * slot->bitmap.pitch + x];
			// D3DFMT_A8R8G8B8: [B, G, R, A] 小端序
			pDst[y * pitch + x * 4 + 0] = 255;	 // B
			pDst[y * pitch + x * 4 + 1] = 255;	 // G
			pDst[y * pitch + x * 4 + 2] = 255;	 // R
			pDst[y * pitch + x * 4 + 3] = alpha; // A
		}
	}
	tex->UnlockRect(0);

	glyph.tex.t2 = tex;
	glyph.uMax = (float)w / (float)texW;
	glyph.vMax = (float)h / (float)texH;
	g_charCache[idx] = glyph;
	return glyph;
}

void SaveD3DState(LPDIRECT3DDEVICE8 pDev, D3DStateBackup &outState)
{
	pDev->GetRenderState(D3DRS_ZWRITEENABLE, &outState.oldZWrite);
	pDev->GetRenderState(D3DRS_ZENABLE, &outState.oldZTest);
	pDev->GetRenderState(D3DRS_ALPHABLENDENABLE, &outState.oldBlend);
	pDev->GetRenderState(D3DRS_BLENDOP, &outState.oldAlphaBlendOp);
	pDev->GetRenderState(D3DRS_SRCBLEND, &outState.oldSrcBlend);
	pDev->GetRenderState(D3DRS_DESTBLEND, &outState.oldDestBlend);
	pDev->GetRenderState(D3DRS_CULLMODE, &outState.oldCullMode);
	pDev->GetRenderState(D3DRS_LIGHTING, &outState.oldLighting);
	pDev->GetRenderState(D3DRS_ALPHATESTENABLE, &outState.oldAlphaTest);
	pDev->GetRenderState(D3DRS_FOGENABLE, &outState.oldFog);
	pDev->GetVertexShader(&outState.oldVertexShader);

	// GetTexture 需要基类指针类型
	IDirect3DBaseTexture8 *pBaseTex = nullptr;
	pDev->GetTexture(0, &pBaseTex);
	outState.oldTexture = (LPDIRECT3DTEXTURE8)pBaseTex;

	// 保存纹理阶段状态
	pDev->GetTextureStageState(0, D3DTSS_COLOROP, &outState.oldColorOp);
	pDev->GetTextureStageState(0, D3DTSS_COLORARG1, &outState.oldColorArg1);
	pDev->GetTextureStageState(0, D3DTSS_COLORARG2, &outState.oldColorArg2);
	pDev->GetTextureStageState(0, D3DTSS_ALPHAOP, &outState.oldAlphaOp);
	pDev->GetTextureStageState(0, D3DTSS_ALPHAARG1, &outState.oldAlphaArg1);
	pDev->GetTextureStageState(0, D3DTSS_ALPHAARG2, &outState.oldAlphaArg2);
	pDev->GetTextureStageState(0, D3DTSS_MINFILTER, &outState.oldMinFilter);
	pDev->GetTextureStageState(0, D3DTSS_MAGFILTER, &outState.oldMagFilter);
	pDev->GetTextureStageState(0, D3DTSS_MIPFILTER, &outState.oldMipFilter);
	pDev->GetTextureStageState(0, D3DTSS_ADDRESSU, &outState.oldAddressU);
	pDev->GetTextureStageState(0, D3DTSS_ADDRESSV, &outState.oldAddressV);

	pDev->GetTransform(D3DTS_PROJECTION, &outState.oldProj);
	pDev->GetTransform(D3DTS_WORLD, &outState.oldWorld);
}

void RestoreD3DState(LPDIRECT3DDEVICE8 pDev, D3DStateBackup &state)
{
	pDev->SetRenderState(D3DRS_ZWRITEENABLE, state.oldZWrite);
	pDev->SetRenderState(D3DRS_ZENABLE, state.oldZTest);
	pDev->SetRenderState(D3DRS_ALPHABLENDENABLE, state.oldBlend);
	pDev->SetRenderState(D3DRS_BLENDOP, state.oldAlphaBlendOp);
	pDev->SetRenderState(D3DRS_SRCBLEND, state.oldSrcBlend);
	pDev->SetRenderState(D3DRS_DESTBLEND, state.oldDestBlend);
	pDev->SetRenderState(D3DRS_CULLMODE, state.oldCullMode);
	pDev->SetRenderState(D3DRS_LIGHTING, state.oldLighting);
	pDev->SetRenderState(D3DRS_ALPHATESTENABLE, state.oldAlphaTest);
	pDev->SetRenderState(D3DRS_FOGENABLE, state.oldFog);
	pDev->SetVertexShader(state.oldVertexShader);
	pDev->SetTexture(0, state.oldTexture);

	// 恢复纹理阶段状态
	pDev->SetTextureStageState(0, D3DTSS_COLOROP, state.oldColorOp);
	pDev->SetTextureStageState(0, D3DTSS_COLORARG1, state.oldColorArg1);
	pDev->SetTextureStageState(0, D3DTSS_COLORARG2, state.oldColorArg2);
	pDev->SetTextureStageState(0, D3DTSS_ALPHAOP, state.oldAlphaOp);
	pDev->SetTextureStageState(0, D3DTSS_ALPHAARG1, state.oldAlphaArg1);
	pDev->SetTextureStageState(0, D3DTSS_ALPHAARG2, state.oldAlphaArg2);
	pDev->SetTextureStageState(0, D3DTSS_MINFILTER, state.oldMinFilter);
	pDev->SetTextureStageState(0, D3DTSS_MAGFILTER, state.oldMagFilter);
	pDev->SetTextureStageState(0, D3DTSS_MIPFILTER, state.oldMipFilter);
	pDev->SetTextureStageState(0, D3DTSS_ADDRESSU, state.oldAddressU);
	pDev->SetTextureStageState(0, D3DTSS_ADDRESSV, state.oldAddressV);

	pDev->SetTransform(D3DTS_PROJECTION, &state.oldProj);
	pDev->SetTransform(D3DTS_WORLD, &state.oldWorld);

	// 释放保存的纹理引用
	if (state.oldTexture)
		state.oldTexture->Release();
}

struct TextVertex
{
	float x, y, z, rhw;
	DWORD color;
	float u, v;
	static const DWORD FVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;
};

void DrawCharQuad(LPDIRECT3DDEVICE8 pDev, float x, float y, float w, float h, DWORD color, float uMax, float vMax)
{
	TextVertex v[4] = {
		{x, y, 0.0f, 1.0f, color, 0.0f, 0.0f},
		{x + w, y, 0.0f, 1.0f, color, uMax, 0.0f},
		{x, y + h, 0.0f, 1.0f, color, 0.0f, vMax},
		{x + w, y + h, 0.0f, 1.0f, color, uMax, vMax},
	};

	pDev->SetVertexShader(TextVertex::FVF);
	pDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(TextVertex));
}

void Setup2DOrtho(LPDIRECT3DDEVICE8 pDev)
{
	RECT rc;
	GetClientRect(hWnd, &rc);
	D3DXMATRIX ortho;
	D3DXMATRIX identity;
	D3DXMatrixOrthoOffCenterLH(&ortho, 0.0f, (float)rc.right, (float)rc.bottom, 0.0f, 0.0f, 1.0f);
	D3DXMatrixIdentity(&identity);
	pDev->SetTransform(D3DTS_PROJECTION, &ortho);
	pDev->SetTransform(D3DTS_WORLD, &identity);

	pDev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	pDev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
	pDev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	pDev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	pDev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	pDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	pDev->SetRenderState(D3DRS_LIGHTING, FALSE);
	pDev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	pDev->SetRenderState(D3DRS_FOGENABLE, FALSE);

	// 设置纹理阶段状态 - 颜色使用顶点颜色调制纹理
	pDev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	pDev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	pDev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

	// 设置纹理阶段状态 - Alpha 使用纹理 Alpha 和顶点 Alpha
	pDev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	pDev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	pDev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	// 统一设置纹理过滤、寻址，替代 tex->SetFilter/SetAddressMode
	pDev->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
	pDev->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
	pDev->SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);
	pDev->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	pDev->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
}

void DrawTextD3D8(float startX, float startY, DWORD color, const wchar_t *text, bool bCenter, int fontSize)
{
	if (!g_ftFace || !text)
		return;

	if (!g_pDevice)
	{
		return;
	}

	D3DStateBackup state;
	SaveD3DState(g_pDevice, state);
	Setup2DOrtho(g_pDevice);

	if (fontSize <= 0)
		fontSize = 22;
	if (bCenter)
	{
		float measureX = 0.0f;
		float minX = 0.0f, maxX = 0.0f, minY = 0.0f, maxY = 0.0f;
		bool hasBounds = false;
		for (const wchar_t *p = text; *p; ++p)
		{
			CharGlyph glyph = LoadGlyphD3D(*p, fontSize);
			if (glyph.tex.t2 && glyph.w > 0 && glyph.h > 0)
			{
				float x0 = measureX + (float)glyph.bearingX;
				float y0 = -(float)glyph.bearingY;
				float x1 = x0 + (float)glyph.w;
				float y1 = y0 + (float)glyph.h;
				if (!hasBounds)
				{
					minX = x0;
					maxX = x1;
					minY = y0;
					maxY = y1;
					hasBounds = true;
				}
				else
				{
					if (x0 < minX)
						minX = x0;
					if (x1 > maxX)
						maxX = x1;
					if (y0 < minY)
						minY = y0;
					if (y1 > maxY)
						maxY = y1;
				}
			}
			measureX += glyph.advance;
		}
		if (hasBounds)
		{
			startX -= (minX + maxX) * 0.5f;
			startY -= (minY + maxY) * 0.5f;
		}
	}

	// 描边：在主色前先用黑色向4个方向偏移1像素绘制
	const DWORD outlineColor = (color & 0xFF5D3D25); // 与主色相同 alpha，RGB=0（黑色）
	const float offsets[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
	for (auto &off : offsets)
	{
		float curX = startX + off[0];
		const wchar_t *p = text;
		while (*p)
		{
			wchar_t ch = *p;
			CharGlyph glyph = LoadGlyphD3D(ch, fontSize);
			if (glyph.tex.t2)
			{
				g_pDevice->SetTexture(0, glyph.tex.t2);
				float x = curX + glyph.bearingX;
				float y = (startY + off[1]) - glyph.bearingY;
				DrawCharQuad(g_pDevice, x, y, (float)glyph.w, (float)glyph.h, outlineColor, glyph.uMax, glyph.vMax);
			}
			curX += glyph.advance;
			p++;
		}
	}

	// 主色文字
	float curX = startX;
	while (*text)
	{
		wchar_t ch = *text;
		CharGlyph glyph = LoadGlyphD3D(ch, fontSize);
		if (!glyph.tex.t2)
		{
			curX += glyph.advance;
			text++;
			continue;
		}
		g_pDevice->SetTexture(0, glyph.tex.t2);
		float x = curX + glyph.bearingX;
		float y = startY - glyph.bearingY;
		DrawCharQuad(g_pDevice, x, y, (float)glyph.w, (float)glyph.h, color, glyph.uMax, glyph.vMax);
		curX += glyph.advance;
		text++;
	}

	g_pDevice->SetTexture(0, nullptr);
	RestoreD3DState(g_pDevice, state);
}

void SetOverlayOrtho()
{
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	int viewWidth = viewport[2];
	int viewHeight = viewport[3];

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	// 投影矩阵严格匹配游戏视口，左上角为(0,0)
	glOrtho(0.0f, (float)viewWidth, (float)viewHeight, 0.0f, -1.0f, 1.0f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_TEXTURE_2D);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

void RestoreOpenGLState()
{
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}

void DrawOverlayText(float x, float y, DWORD dwColor, const wchar_t *text, float scale, bool bCenter, int fontSize)
{
	if (!g_ftFace || !text)
		return;

	if (!War3GlobalOverlay_OPENGL)
	{
		return;
	}

	HGLRC oldcontext = wglGetCurrentContext();
	wglMakeCurrent(GlobalDc, War3GlobalOverlay_OPENGL);

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	SetOverlayOrtho();

	// OpenGL 路径：fontSize 已经是实际屏幕像素大小（由调用方换算），scale 固定为1
	if (fontSize <= 0)
		fontSize = 22;
	if (bCenter)
	{
		float measureX = 0.0f;
		float minX = 0.0f, maxX = 0.0f, minY = 0.0f, maxY = 0.0f;
		bool hasBounds = false;
		for (const wchar_t *p = text; *p; ++p)
		{
			CharGlyph glyph = LoadGlyph(*p, fontSize);
			if (glyph.tex.t1 && glyph.w > 0 && glyph.h > 0)
			{
				float x0 = measureX + (float)glyph.bearingX;
				float y0 = -(float)glyph.bearingY;
				float x1 = x0 + (float)glyph.w;
				float y1 = y0 + (float)glyph.h;
				if (!hasBounds)
				{
					minX = x0;
					maxX = x1;
					minY = y0;
					maxY = y1;
					hasBounds = true;
				}
				else
				{
					if (x0 < minX)
						minX = x0;
					if (x1 > maxX)
						maxX = x1;
					if (y0 < minY)
						minY = y0;
					if (y1 > maxY)
						maxY = y1;
				}
			}
			measureX += glyph.advance;
		}
		if (hasBounds)
		{
			x -= (minX + maxX) * 0.5f * scale;
			y -= (minY + maxY) * 0.5f * scale;
		}
	}

	float a = ((dwColor >> 24) & 0xFF) / 255.0f;
	float r = ((dwColor >> 16) & 0xFF) / 255.0f;
	float g = ((dwColor >> 8) & 0xFF) / 255.0f;
	float b = (dwColor & 0xFF) / 255.0f;

	// 描边：先用黑色向4个方向偏移1像素绘制
	const float offsets[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
	for (auto &off : offsets)
	{
		// 0xFF5D3D25
		glColor4f(0.36f, 0.24f, 0.145f, a);
		float ox = x + off[0] / scale;
		float oy = y + off[1] / scale;
		float oCurX = ox;

		float base_curX2 = oCurX;
		glPushMatrix();
		glTranslatef(base_curX2, oy, 0.0f);
		glScalef(scale, scale, 1.0f);
		glTranslatef(-base_curX2, -oy, 0.0f);

		const wchar_t *p = text;
		while (*p)
		{
			wchar_t ch = *p;
			CharGlyph glyph = LoadGlyph(ch, fontSize);
			if (glyph.tex.t1 && glyph.w > 0 && glyph.h > 0)
			{
				glBindTexture(GL_TEXTURE_2D, glyph.tex.t1);
				float x0 = (float)(oCurX + glyph.bearingX);
				float y0 = (float)(oy - glyph.bearingY);
				float x1 = x0 + (float)glyph.w;
				float y1 = y0 + (float)glyph.h;
				glBegin(GL_QUADS);
				glTexCoord2f(0.0f, 0.0f);
				glVertex2f(x0, y0);
				glTexCoord2f(glyph.uMax, 0.0f);
				glVertex2f(x1, y0);
				glTexCoord2f(glyph.uMax, glyph.vMax);
				glVertex2f(x1, y1);
				glTexCoord2f(0.0f, glyph.vMax);
				glVertex2f(x0, y1);
				glEnd();
			}
			oCurX += glyph.advance;
			p++;
		}
		glPopMatrix();
	}

	// 主色文字
	glColor4f(r, g, b, a);
	float curX = x;
	float base_curX = curX;
	glPushMatrix();
	glTranslatef(base_curX, y, 0.0f);
	glScalef(scale, scale, 1.0f);
	glTranslatef(-base_curX, -y, 0.0f);

	while (*text)
	{
		wchar_t ch = *text;
		CharGlyph glyph = LoadGlyph(ch, fontSize);
		// glyph.w *= scale;
		// glyph.h *= scale;

		if (glyph.tex.t1 && glyph.w > 0 && glyph.h > 0)
		{
			glBindTexture(GL_TEXTURE_2D, glyph.tex.t1);
			float x0 = (float)(curX + glyph.bearingX);
			float y0 = (float)(y - glyph.bearingY);
			float x1 = x0 + (float)glyph.w;
			float y1 = y0 + (float)glyph.h;

			glBegin(GL_QUADS);
			glTexCoord2f(0.0f, 0.0f);
			glVertex2f(x0, y0); // 左上
			glTexCoord2f(glyph.uMax, 0.0f);
			glVertex2f(x1, y0); // 右上
			glTexCoord2f(glyph.uMax, glyph.vMax);
			glVertex2f(x1, y1); // 右下
			glTexCoord2f(0.0f, glyph.vMax);
			glVertex2f(x0, y1); // 左下
			glEnd();
		}

		curX += glyph.advance;
		text++;
	}
	glPopMatrix();

	RestoreOpenGLState();
	glPopAttrib();

	wglMakeCurrent(GlobalDc, oldcontext);
	// g_oWglSwapLayerBuffers(GlobalDc, WGL_SWAP_OVERLAY1);
}

// opengl下的方案
int __stdcall MyWglSwapLayerBuffers(HDC dc, unsigned int b)
{
	if (!dc)
	{
		g_textDrawQueue.clear();
		return g_oWglSwapLayerBuffers(dc, b);
	}

	GlobalDc = dc;

	if (!War3GlobalOverlay_OPENGL)
	{
		War3GlobalOverlay_OPENGL = wglCreateContext(dc);
		if (!War3GlobalOverlay_OPENGL)
		{
			spdlog::error("创建OVERLAY1分层RC失败");
		}
	}

	// 只在交换主图层时绘制
	if (b & WGL_SWAP_MAIN_PLANE)
	{
		DrawSystemInfo();

		auto now = std::chrono::steady_clock::now();
		for (const auto &item : g_textDrawQueue)
		{
			if (std::chrono::duration_cast<std::chrono::milliseconds>(now - item.timestamp).count() < 1000)
				DrawTextToScreenReal(item.x, item.y, item.text.c_str(), item.color, item.bAdj, item.bCenter, item.fontSize);
		}

		g_textDrawQueue.clear();
		g_oWglSwapLayerBuffers(dc, WGL_SWAP_OVERLAY1);
	}

	// 先调用原函数交换主图层
	return g_oWglSwapLayerBuffers(dc, b);
}

bool CheckWar3RenderMode()
{
	// 获取完整的命令行字符串
	const char *cmdLine = GetCommandLineA();
	if (!cmdLine)
		return false;

	std::string args(cmdLine);
	std::transform(args.begin(), args.end(), args.begin(), ::tolower);
	if (args.find("-opengl") != std::string::npos)
	{
		return true;
	}
	return false;
}

void HookOpenGL()
{
	void *hOpenGL = GetModuleHandle(L"opengl32.dll");
	if (hOpenGL)
	{
		spdlog::info("HookOpenGL");
		DWORD wglSwapLayerBuffers_org = (DWORD)GetProcAddress((HMODULE)hOpenGL, "wglSwapLayerBuffers");
		FunHook((void *)wglSwapLayerBuffers_org, (void *)MyWglSwapLayerBuffers, (void *&)g_oWglSwapLayerBuffers);
	}
}

void HookD3D8()
{
	bIsOpenGL = CheckWar3RenderMode();
	if (bIsOpenGL)
	{
		HookOpenGL();
	}
	else
	{
		if (!DirectxHookInitialized)
		{
			spdlog::info("HookD3D8");
			DirectxHookInitialized = true;
			DWORD EndScene_org = (DWORD)g_gameDllBase;
			EndScene_org += 0x52FD70;
			FunHook((void *)EndScene_org, (void *)MyEndScene, (void *&)g_oEndScene);
		}
	}
}

int PlantDetourJMP(unsigned char *source, const unsigned char *destination, size_t length)
{

	unsigned long oldProtection;
	bool bRet = VirtualProtect(source, length, PAGE_EXECUTE_READWRITE, &oldProtection);

	if (bRet == false)
		return false;

	source[0] = 0xE9;
	*(unsigned long *)(source + 1) = (unsigned long)(destination - source) - 5;

	for (unsigned int i = 5; i < length; i++)
		source[i] = 0x90;

	VirtualProtect(source, length, oldProtection, &oldProtection);
	FlushInstructionCache(GetCurrentProcess(), source, length);
	return true;
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

int JumpBackAddr7()
{
	MessageBoxA(0, 0, 0, 7);
	return 0;
}

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

int __fastcall MyIsNeedDrawUnit2(unsigned char *UnitAddr, int addr1)
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

void HookCooldown()
{
	InitFreeType();
	HookD3D8();

	g_DrawSkillPanelOffset = (DWORD)g_gameDllBase + 0x277FE0;
	g_DrawSkillPanelOverlayOffset = (DWORD)g_gameDllBase + 0x278090;
	g_IsNeedDrawUnitOriginOffset = (DWORD)g_gameDllBase + 0x2868E0;

	DWORD IsDrawSkillPanelOffset = (DWORD)g_gameDllBase + 0x34FDC0;
	FunHook((void *)IsDrawSkillPanelOffset, (void *)MyIsDrawSkillPanel, (void *&)g_oIsDrawSkillPanel);

	DWORD IsDrawSkillPanelOverlayOffset = (DWORD)g_gameDllBase + 0x34FE00;
	FunHook((void *)IsDrawSkillPanelOverlayOffset, (void *)MyIsDrawSkillPanelOverlay, (void *&)g_oIsDrawSkillPanelOverlay);

	DWORD IsNeedDrawUnit2Offset = (DWORD)g_gameDllBase + 0x28ECF0;
	FunHook((void *)IsNeedDrawUnit2Offset, (void *)MyIsNeedDrawUnit2, (void *&)g_oIsNeedDrawUnit2);

	DWORD pPreSetCooldown = (DWORD)g_gameDllBase;
	pPreSetCooldown += 0x3502A0; // sub_6F35F170也可以
	FunHook((void *)pPreSetCooldown, (void *)SetCdForAddr, (void *&)RealFunc);
}

void paddingStringtoLength(std::wstring &str, int len)
{
	auto l = str.length();
	len -= l;
	auto leftpad = len / 2;
	auto rightpad = len - leftpad;
	str.insert(0, leftpad, ' ');
	str.append(rightpad, ' ');
}

bool GetCommandButtonPos1024(CCommandButton *btn, float &x, float &y, float *outBtnH1024 = nullptr)
{
	if (!btn)
		return false;

	auto &lf = btn->baseSimpleButton.baseSimpleFrame.baseLayoutFrame;

	float uiLeft = lf.borderL;
	float uiRight = lf.borderR;
	float uiTop = lf.borderU;
	float uiBottom = lf.borderB;

	if (uiRight <= uiLeft || uiTop <= uiBottom)
		return false;

	constexpr float UI_W = 0.8f;
	constexpr float UI_H = 0.6f;

	float uiCenterX = (uiLeft + uiRight) * 0.5f;
	float uiCenterY = (uiTop + uiBottom) * 0.5f;

	x = uiCenterX / UI_W * 1024.0f;
	y = (UI_H - uiCenterY) / UI_H * 768.0f;

	if (outBtnH1024)
	{
		// 按钮在 1024x768 空间的高度（像素）
		*outBtnH1024 = (uiTop - uiBottom) / UI_H * 768.0f;
	}

	return true;
}

void __fastcall SetCdForAddr(DWORD pThis, int dummy)
{
	CCommandButton *cmdbt = (CCommandButton *)pThis;

	if (cmdbt && cmdbt->commandButtonData)
	{
		CAbility *abi = cmdbt->commandButtonData->ability;

		if (abi && ((abi->flag2 & 0x200) != 0 && (abi->flag2 & 0x400) == 0))
		{
			unsigned char *pData = *(unsigned char **)((DWORD)abi + 0xDC);
			if (pData)
			{
				float val1 = *(float *)(pData + 0x4);
				int pData2 = *(int *)(pData + 0xC);
				if (pData2 > 0)
				{
					float val2 = *(float *)(pData2 + 0x40);
					float val3 = val1 - val2;
					float x = 0.0f;
					float y = 0.0f;
					float btnH1024 = 0.0f;

					if (GetCommandButtonPos1024(cmdbt, x, y, &btnH1024))
					{
						// 字体大小 = 按钮高度的 40%，限制在 [8, 48] 范围内
						int fontSize = (int)(btnH1024 * 0.40f + 0.5f);
						if (fontSize < 8)
							fontSize = 8;
						if (fontSize > 48)
							fontSize = 48;

						std::wstring text = std::format(L"{:3.0f}", std::trunc(val3));
						if (val3 < 1.00f)
						{
							text = std::format(L"{:3.2f}", val3);
						}
						paddingStringtoLength(text, 4);
						DrawTextToScreen(x, y, text.c_str(), 0xFFE0E0E0, true, true, fontSize);
					}
				}
			}
		}
		else
		{
			// auto addr = ((DWORD)g_gameDllBase + 0xAC52CC);
			// auto v5 = *(float *)addr;
			// spdlog::info("SetCdForAddr = {}", *(float *)addr);
			// if (0.0 == v5){
			// 	(pTargetFunc)((DWORD)g_gameDllBase + 0x337E70)(pThis, dummy)
			// }
		}
	}
	RealFunc(pThis, dummy);
}
