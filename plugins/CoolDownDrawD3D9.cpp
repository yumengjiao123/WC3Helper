#include "CoolDownDrawD3D9.h"
#include <d3dx9.h>
#include <windows.h>
#include <unordered_map>
#include <vector>
#include <ft2build.h>
#include FT_FREETYPE_H

// ========== 外部共享变量 ==========
extern FT_Face g_ftFace;
extern FT_Library g_ftLib;
extern HWND hWnd;

// D3D9 全局设备指针
extern LPVOID g_pDevice;

// D3D9 渲染状态备份
struct D3D9StateBackup
{
	DWORD oldZWrite, oldZTest, oldBlend, oldAlphaBlendOp;
	DWORD oldSrcBlend, oldDestBlend;
	DWORD oldCullMode, oldLighting, oldAlphaTest, oldFog;
	DWORD oldVertexShader;
	LPDIRECT3DTEXTURE9 oldTexture;
	DWORD oldColorOp, oldColorArg1, oldColorArg2;
	DWORD oldAlphaOp, oldAlphaArg1, oldAlphaArg2;
	DWORD oldMinFilter, oldMagFilter, oldMipFilter;
	DWORD oldAddressU, oldAddressV;
	D3DMATRIX oldProj, oldWorld;
};

// D3D9 文字绘制顶点
struct TextVertexD3D9
{
	float x, y, z, rhw;
	DWORD color;
	float u, v;
	static const DWORD FVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;
};

// D3D9 字符纹理缓存（独立于 D3D8/OpenGL 的 g_charCache）
static std::unordered_map<DWORD, CharGlyph> g_charCacheD3D9;

// ========== D3D9 辅助函数 ==========

static CharGlyph LoadGlyphD3D9(wchar_t ch, int fontSize)
{
	unsigned int idx = ch;
	if (g_charCacheD3D9.contains(idx))
		return g_charCacheD3D9[idx];

	FT_Set_Pixel_Sizes(g_ftFace, 0, fontSize);

	FT_Error err = FT_Load_Char(g_ftFace, ch, FT_LOAD_RENDER);
	if (err)
	{
		CharGlyph empty{};
		g_charCacheD3D9[idx] = empty;
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
		g_charCacheD3D9[idx] = glyph;
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

	LPDIRECT3DTEXTURE9 tex = nullptr;
	HRESULT hr = ((LPDIRECT3DDEVICE9)g_pDevice)->CreateTexture(texW, texH, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, nullptr);
	if (FAILED(hr))
	{
		g_charCacheD3D9[idx] = glyph;
		return glyph;
	}

	D3DLOCKED_RECT lock;
	hr = tex->LockRect(0, &lock, nullptr, 0);
	if (FAILED(hr))
	{
		tex->Release();
		g_charCacheD3D9[idx] = glyph;
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
			pDst[y * pitch + x * 4 + 0] = 255;	 // B
			pDst[y * pitch + x * 4 + 1] = 255;	 // G
			pDst[y * pitch + x * 4 + 2] = 255;	 // R
			pDst[y * pitch + x * 4 + 3] = alpha; // A
		}
	}
	tex->UnlockRect(0);

	glyph.tex = tex;
	glyph.uMax = (float)w / (float)texW;
	glyph.vMax = (float)h / (float)texH;
	g_charCacheD3D9[idx] = glyph;
	return glyph;
}

static void SaveD3D9State(LPDIRECT3DDEVICE9 pDev, D3D9StateBackup &outState)
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
	pDev->GetFVF(&outState.oldVertexShader);

	IDirect3DBaseTexture9 *pBaseTex = nullptr;
	pDev->GetTexture(0, &pBaseTex);
	outState.oldTexture = (LPDIRECT3DTEXTURE9)pBaseTex;

	pDev->GetTextureStageState(0, D3DTSS_COLOROP, &outState.oldColorOp);
	pDev->GetTextureStageState(0, D3DTSS_COLORARG1, &outState.oldColorArg1);
	pDev->GetTextureStageState(0, D3DTSS_COLORARG2, &outState.oldColorArg2);
	pDev->GetTextureStageState(0, D3DTSS_ALPHAOP, &outState.oldAlphaOp);
	pDev->GetTextureStageState(0, D3DTSS_ALPHAARG1, &outState.oldAlphaArg1);
	pDev->GetTextureStageState(0, D3DTSS_ALPHAARG2, &outState.oldAlphaArg2);
	pDev->GetSamplerState(0, D3DSAMP_MINFILTER, &outState.oldMinFilter);
	pDev->GetSamplerState(0, D3DSAMP_MAGFILTER, &outState.oldMagFilter);
	pDev->GetSamplerState(0, D3DSAMP_MIPFILTER, &outState.oldMipFilter);
	pDev->GetSamplerState(0, D3DSAMP_ADDRESSU, &outState.oldAddressU);
	pDev->GetSamplerState(0, D3DSAMP_ADDRESSV, &outState.oldAddressV);

	pDev->GetTransform(D3DTS_PROJECTION, &outState.oldProj);
	pDev->GetTransform(D3DTS_WORLD, &outState.oldWorld);
}

static void RestoreD3D9State(LPDIRECT3DDEVICE9 pDev, D3D9StateBackup &state)
{
	if (!pDev)
		return;

	try
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
		pDev->SetFVF(state.oldVertexShader);
		pDev->SetTexture(0, state.oldTexture);

		pDev->SetTextureStageState(0, D3DTSS_COLOROP, state.oldColorOp);
		pDev->SetTextureStageState(0, D3DTSS_COLORARG1, state.oldColorArg1);
		pDev->SetTextureStageState(0, D3DTSS_COLORARG2, state.oldColorArg2);
		pDev->SetTextureStageState(0, D3DTSS_ALPHAOP, state.oldAlphaOp);
		pDev->SetTextureStageState(0, D3DTSS_ALPHAARG1, state.oldAlphaArg1);
		pDev->SetTextureStageState(0, D3DTSS_ALPHAARG2, state.oldAlphaArg2);
		pDev->SetSamplerState(0, D3DSAMP_MINFILTER, state.oldMinFilter);
		pDev->SetSamplerState(0, D3DSAMP_MAGFILTER, state.oldMagFilter);
		pDev->SetSamplerState(0, D3DSAMP_MIPFILTER, state.oldMipFilter);
		pDev->SetSamplerState(0, D3DSAMP_ADDRESSU, state.oldAddressU);
		pDev->SetSamplerState(0, D3DSAMP_ADDRESSV, state.oldAddressV);

		pDev->SetTransform(D3DTS_PROJECTION, &state.oldProj);
		pDev->SetTransform(D3DTS_WORLD, &state.oldWorld);
	}
	catch (...)
	{
	}

	if (state.oldTexture)
	{
		try
		{
			state.oldTexture->Release();
		}
		catch (...)
		{
		}
		state.oldTexture = nullptr;
	}
}

static void DrawCharQuadD3D9(LPDIRECT3DDEVICE9 pDev, float x, float y, float w, float h, DWORD color, float uMax, float vMax)
{
	TextVertexD3D9 v[4] = {
		{x, y, 0.0f, 1.0f, color, 0.0f, 0.0f},
		{x + w, y, 0.0f, 1.0f, color, uMax, 0.0f},
		{x, y + h, 0.0f, 1.0f, color, 0.0f, vMax},
		{x + w, y + h, 0.0f, 1.0f, color, uMax, vMax},
	};

	pDev->SetFVF(TextVertexD3D9::FVF);
	pDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(TextVertexD3D9));
}

static void Setup2DOrthoD3D9(LPDIRECT3DDEVICE9 pDev)
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

	pDev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	pDev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	pDev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

	pDev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	pDev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	pDev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	pDev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	pDev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	pDev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
	pDev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	pDev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
}

// ========== D3D9 文字绘制主函数 ==========

void DrawTextD3D9(float startX, float startY, DWORD color, const wchar_t *text, bool bCenter, int fontSize)
{
	LPDIRECT3DDEVICE9 pDev = (LPDIRECT3DDEVICE9)g_pDevice;
	if (!g_ftFace || !text)
		return;

	if (!g_pDevice)
		return;

	if (D3D_OK != pDev->TestCooperativeLevel())
		return;

	D3D9StateBackup state;
	SaveD3D9State(pDev, state);
	Setup2DOrthoD3D9(pDev);

	if (fontSize <= 0)
		fontSize = 22;

	if (bCenter)
	{
		float measureX = 0.0f;
		float minX = 0.0f, maxX = 0.0f, minY = 0.0f, maxY = 0.0f;
		bool hasBounds = false;
		for (const wchar_t *p = text; *p; ++p)
		{
			CharGlyph glyph = LoadGlyphD3D9(*p, fontSize);
			if (glyph.tex && glyph.w > 0 && glyph.h > 0)
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
	const DWORD outlineColor = (color & 0xFF5D3D25);
	const float offsets[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
	for (auto &off : offsets)
	{
		float curX = startX + off[0];
		const wchar_t *p = text;
		while (*p)
		{
			wchar_t ch = *p;
			CharGlyph glyph = LoadGlyphD3D9(ch, fontSize);
			if (glyph.tex)
			{
				pDev->SetTexture(0, (LPDIRECT3DTEXTURE9)glyph.tex);
				float x = curX + glyph.bearingX;
				float y = (startY + off[1]) - glyph.bearingY;
				DrawCharQuadD3D9(pDev, x, y, (float)glyph.w, (float)glyph.h, outlineColor, glyph.uMax, glyph.vMax);
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
		CharGlyph glyph = LoadGlyphD3D9(ch, fontSize);
		if (!glyph.tex)
		{
			curX += glyph.advance;
			text++;
			continue;
		}
		pDev->SetTexture(0, (LPDIRECT3DTEXTURE9)glyph.tex);
		float x = curX + glyph.bearingX;
		float y = startY - glyph.bearingY;
		DrawCharQuadD3D9(pDev, x, y, (float)glyph.w, (float)glyph.h, color, glyph.uMax, glyph.vMax);
		curX += glyph.advance;
		text++;
	}

	pDev->SetTexture(0, nullptr);
	RestoreD3D9State(pDev, state);
}
