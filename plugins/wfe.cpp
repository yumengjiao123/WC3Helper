// #include "pch.h"
#include <windows.h>
#include "wfe.h"
#include <detours.h>
#include <iostream>
#include <process.h>

#include "spdlog/spdlog.h"

#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

void FunHook(void *pOldFuncAddr, void *pNewFuncAddr, void *&pCallBackFuncAddr);
void UnFunHook(void *pOldFuncAddr, void *pNewFuncAddr);

DWORD WINAPI MyGetModuleFileNameA(
	HMODULE hModule,
	LPSTR lpFilename,
	DWORD nSize);

extern LPVOID g_NBDllBase;
extern char gGamepath[MAX_PATH];

typedef LPVOID(__thiscall *tsub_XXXX86A0)(void *pThis, LPCSTR lpFilename);

typedef DWORD(WINAPI *tGetModuleFileNameA)(
	HMODULE hModule,
	LPSTR lpFilename,
	DWORD nSize);

tsub_XXXX86A0 p_sub_XXXX86A0 = nullptr;
tGetModuleFileNameA oGetModuleFileNameA = nullptr;

LPVOID __fastcall sub_XXXX86A0(void *pThis, LPCSTR lpFilename)
{
	if (p_sub_XXXX86A0 == nullptr)
	{
		return NULL;
	}

	DWORD ret = (DWORD)p_sub_XXXX86A0(pThis, lpFilename);

	if (lpFilename != nullptr)
	{
		spdlog::info("lpFilename: {}", lpFilename);
		if (StrStrIA(lpFilename, "war3.exe") != nullptr)
		{
			// 标志为1.24e 268531
			*(DWORD *)(ret + 12) = 268531;
		}
	}

	return (LPVOID)ret;
}

BOOL GetGameDllDirectory(LPSTR pathBuffer, DWORD cchPath)
{
	// 通过句柄获取完整路径
	if (GetModuleFileNameA(0, pathBuffer, cchPath) == 0)
	{
		// 处理错误
		return FALSE;
	}

	// 移除文件名，只保留目录路径
	PathRemoveFileSpecA(pathBuffer);
	return TRUE;
}

void LoadWFE()
{
	char dllpath[MAX_PATH];
	GetGameDllDirectory(dllpath, MAX_PATH);

	// 11对战平台目录在war3的yy下
	if (StrStrIA(dllpath, "\\YY") != nullptr)
	{
		PathRemoveFileSpecA(dllpath);
	}
	
	strcpy_s(gGamepath, MAX_PATH, dllpath);
	strcat_s(dllpath, "\\NBDLL.dll");

	spdlog::info("dllpath: {}", dllpath);

	auto t = (tGetModuleFileNameA)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetModuleFileNameA");
	FunHook(t, (void *)MyGetModuleFileNameA, (void *&)oGetModuleFileNameA);

	g_NBDllBase = LoadLibraryA(dllpath);
	if (g_NBDllBase == nullptr)
	{
		spdlog::error("Failed to load NBDll.dll");
		return;
	}
	// 重置GetModuleFileNameA
	UnFunHook((void *)oGetModuleFileNameA, (void *)MyGetModuleFileNameA);
	// DWORD offset = (DWORD)g_NBDllBase;
	// offset += 0x286A0;
	// spdlog::info("NBDLL.dll sub_XXXX86A0 offset: 0x{:x}", offset);
	// FunHook((void *)offset, (void *)sub_XXXX86A0, (void *&)p_sub_XXXX86A0);

	// 隐藏DLL,不然会被一些安全软件检测到
	HideDll((HMODULE)g_NBDllBase);
}

void UnloadWFE()
{
	if (g_NBDllBase)
	{
		FreeLibrary((HMODULE)g_NBDllBase);
		g_NBDllBase = nullptr;
	}
}

DWORD WINAPI MyGetModuleFileNameA(
	HMODULE hModule,
	LPSTR lpFilename,
	DWORD nSize)
{
	// 先调用原函数，拿到真实路径
	DWORD ret = oGetModuleFileNameA(hModule, lpFilename, nSize);

	if (lpFilename != nullptr && ret > 0)
	{
		spdlog::info("lpFilename: {}", lpFilename);
		if (StrStrIA(lpFilename, "war3.exe") != nullptr) // 11对战平台, 此时hModule为0
		{
			sprintf_s(lpFilename, nSize, "%s\\war3.exe", gGamepath);
			ret = strlen(lpFilename);
			spdlog::info("new lpFilename: {}", lpFilename);
		}

		if (StrStrIA(lpFilename, "game.dll") != nullptr) // 11对战平台, 此时hModule为0
		{
			sprintf_s(lpFilename, nSize, "%s\\game.dll", gGamepath);
			ret = strlen(lpFilename);
			spdlog::info("new lpFilename: {}", lpFilename);
		}
	}

	return ret;
}

void HideDll(HMODULE hModule)
{
	DWORD dwPEB_LDR_DATA = NULL;

	_asm
	{
		pushad;
		pushfd;
		mov eax, fs:[30h]
		mov eax, [eax+0Ch]
		mov dwPEB_LDR_DATA, eax

															 // InLoadOrderModuleList:
			mov esi, [eax+0Ch]
			mov edx, [eax+10h]

		LoopInLoadOrderModuleList:
			lodsd
			mov esi, eax
			mov ecx, [eax+18h]
			cmp ecx, hModule
			jne SkipA
			mov ebx, [eax]
			mov ecx, [eax+4]
			mov [ecx], ebx
			mov [ebx+4], ecx
			jmp InMemoryOrderModuleList

		SkipA:
			cmp edx, esi
			jne LoopInLoadOrderModuleList

		InMemoryOrderModuleList:
			mov eax, dwPEB_LDR_DATA
			mov esi, [eax+14h]
			mov edx, [eax+18h]

		LoopInMemoryOrderModuleList:
			lodsd
			mov esi, eax
			mov ecx, [eax+10h]
			cmp ecx, hModule
			jne SkipB
			mov ebx, [eax]
			mov ecx, [eax+4]
			mov [ecx], ebx
			mov [ebx+4], ecx
			jmp InInitializationOrderModuleList

		SkipB:
			cmp edx, esi
			jne LoopInMemoryOrderModuleList

		InInitializationOrderModuleList:
			mov eax, dwPEB_LDR_DATA
			mov esi, [eax+1Ch]
			mov edx, [eax+20h]

		LoopInInitializationOrderModuleList:
			lodsd
			mov esi, eax
			mov ecx, [eax+08h]
			cmp ecx, hModule
			jne SkipC
			mov ebx, [eax]
			mov ecx, [eax+4]
			mov [ecx], ebx
			mov [ebx+4], ecx
			jmp Finished

		SkipC:
			cmp edx, esi
			jne LoopInInitializationOrderModuleList

		Finished:
			popfd;
			popad;
	}
}
