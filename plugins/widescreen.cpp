// #include "pch.h"
#include "common.h"
#include "widescreen.h"
#include <detours.h>
#include <iostream>
#include <process.h>

void FunHook(void *pOldFuncAddr, void *pNewFuncAddr, void *&pCallBackFuncAddr);
void UnFunHook(void *pOldFuncAddr, void *pNewFuncAddr);

using PFN_CreateMatrixPerspectiveFov = void(__fastcall *)(float *, DWORD, float, float, float, float);
PFN_CreateMatrixPerspectiveFov p_orgCreateMatrixPerspectiveFov = nullptr;
HWND hWnd = NULL;

void __fastcall CreateMatrixPerspectiveFov(float *outMatrix, DWORD edx, float fovY, float aspectRatio, float nearZ, float farZ)
{
	RECT r;
	float fWideScreenMul = 1.0f;
	auto hwnd = GetGameWindow();
	if (!hwnd)
	{
		return;
	}

	if (GetWindowRect(hwnd, &r))
	{
		float width = float(r.right - r.left);
		float rHeight = 1.0f / (r.bottom - r.top);
		fWideScreenMul = width * rHeight * 0.75f; // (width / height) / (4.0f / 3.0f)
	}

	float yScale = 1.0f / tan(fovY * 0.5f / sqrt(aspectRatio * aspectRatio + 1.0f));
	float xScale = yScale / (aspectRatio * fWideScreenMul);
	outMatrix[0] = xScale;
	outMatrix[5] = yScale;
	outMatrix[10] = (nearZ + farZ) / (farZ - nearZ);
	outMatrix[14] = (-2.0f * nearZ * farZ) / (farZ - nearZ);

	outMatrix[1] = 0.0f;
	outMatrix[2] = 0.0f;
	outMatrix[3] = 0.0f;
	outMatrix[4] = 0.0f;
	outMatrix[6] = 0.0f;
	outMatrix[7] = 0.0f;
	outMatrix[8] = 0.0f;
	outMatrix[9] = 0.0f;
	outMatrix[11] = 1.0f;
	outMatrix[12] = 0.0f;
	outMatrix[13] = 0.0f;
	outMatrix[15] = 0.0f;
}

void UpdateWideScreen(LPVOID gameDllBase)
{
	DWORD offset = (DWORD)gameDllBase;
	Version ver = GetWar3Version();
	if (ver == Version::v124e)
	{
		offset += 0x7B6E90;
	}
	else if (ver == Version::v126a)
	{
		offset += 0x7B66F0;
	}
	else if (ver == Version::v127a)
	{
		offset += 0x0D31D0;
	}
	else
	{
		return;
	}

	FunHook((void *)offset, (void *)CreateMatrixPerspectiveFov, (void *&)p_orgCreateMatrixPerspectiveFov);
}

void ResetD3D()
{
	// 强制游戏重新加载d3d
	auto hwnd = GetGameWindow();
	if (!hwnd)
	{
		return;
	}

	ShowWindow(hwnd, SW_MINIMIZE);
	ShowWindow(hwnd, SW_SHOWNORMAL);
}

void FunHook(void *pOldFuncAddr, void *pNewFuncAddr, void *&pCallBackFuncAddr)
{
	if (!pOldFuncAddr)
	{
		return;
	}

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(void *&)pOldFuncAddr, pNewFuncAddr);
	DetourTransactionCommit();
	pCallBackFuncAddr = pOldFuncAddr;
}

void UnFunHook(void *pOldFuncAddr, void *pNewFuncAddr)
{
	if (!pOldFuncAddr)
	{
		return;
	}

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach(&(void *&)pOldFuncAddr, pNewFuncAddr);
	DetourTransactionCommit();
}

static HWND findTopWindow(DWORD pid)
{
	std::pair<HWND, DWORD> params = {0, pid};

	// Enumerate the windows using a lambda to process each window
	BOOL bResult = EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL
							   {
			auto pParams = (std::pair<HWND, DWORD>*)(lParam);

			DWORD processId;
			if (GetWindowThreadProcessId(hwnd, &processId) &&
				processId == pParams->second &&
				GetWindow(hwnd, GW_OWNER) == 0)
			{
				// Stop enumerating
				SetLastError(-1);
				pParams->first = hwnd;
				return FALSE;
			}

			// Continue enumerating
			return TRUE; }, (LPARAM)&params);

	if (!bResult && GetLastError() == -1 && params.first)
	{
		return params.first;
	}

	return 0;
}

HWND GetGameWindow()
{
	if (!hWnd)
	{
		hWnd = findTopWindow(GetCurrentProcessId());
	}
	return hWnd;
}

#ifdef _MANAGED
#pragma managed(pop)
#endif
