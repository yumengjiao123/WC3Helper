// #include "pch.h"
#include <Windows.h>
#include "plugins/mana.h"
#include "plugins/widescreen.h"
#include "plugins/unitinfo.h"
#include "plugins/wfe.h"
#include "plugins/unitcommand.h"
#include "jass.h"

#include <shlwapi.h>

// spdlog
#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"

#define GS_NOTHING 0	// 主页
#define GS_LOBBY 1		// 大厅
#define GS_LOADING 3	// loading
#define GS_INGAME 4		// 游戏中
#define GS_PAUSEDGAME 6 // 暂停

#define VK_ShowManaBar VK_F5
#define VK_DelayReducer VK_F6
#define VK_AutoSpellSkill VK_F7
#define VK_ZoomIn VK_HOME
#define VK_ZoomOut VK_END

// #define GAME_DLL_SIZE 0xA00000 //1.27
#define GAME_DLL_SIZE 0xb80000 // 1.24

extern DWORD LocalHero;

HANDLE g_hThread = NULL;
bool g_Running = true;
bool g_ShowMana = false;
bool g_WideScreen = true;
bool g_DelayReducer = false;
bool g_AutoSpellSkill = true;
bool g_IsPlayerObserver = false;

LPVOID g_NBDllBase = NULL;
LPVOID g_gameDllBase = NULL;
char gGamepath[MAX_PATH] = {0};

DWORD g_LoadingBarPtr;
DWORD g_ChangeLBText;

DWORD g_DelayOld1 = 0;
DWORD g_DelayOld2 = 0;

float g_camdistance = 1650.00f;

void DoInit();
void WriteBytes(void *lpAddr, LPBYTE data, DWORD len);
void ResetDelay(LPVOID gameDllBase);
void DelayReducer(LPVOID gameDllBase);
void DelayReducer2(LPVOID gameDllBase);
void SetManaBarColor();
void initLog();
void HookChatMessage();
void HookCooldown();

void GameStateInit(LPVOID gameDllBase);
bool ChangeLoadingBarText(char *newtext);

void WINAPI HotKeys();
void SetTls();
bool IsInGame();
bool HotKeyPressed(int iKey);

LPVOID getPlayer(int playerIndex);
LPVOID getLocalPlayer();
int GetPlayerId(LPVOID hPlayer);

void FreeFreeTypeRes();
BOOL APIENTRY DllMain(HMODULE hModule,
					  DWORD ul_reason_for_call,
					  LPVOID lpReserved)
{
	HANDLE hThread = NULL;
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hModule);
		DoInit();
		HideDll(hModule);
		hThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)HotKeys, NULL, NULL, NULL);
		CloseHandle(hThread);
		break;
	case DLL_PROCESS_DETACH:
		// UnloadWFE();
		TerminateThread(hThread, 0);
		FreeFreeTypeRes();
		break;
	}
	return TRUE;
}

void ReadConfig()
{
	char szIniPath[MAX_PATH] = {0};
	GetCurrentDirectoryA(MAX_PATH, szIniPath);

	if (StrStrIA(szIniPath, "\\YY") != nullptr)
	{
		PathRemoveFileSpecA(szIniPath);
	}

	strcat(szIniPath, "\\helper.ini");
	g_WideScreen = (GetPrivateProfileIntA("Helper", "WideScreen", 1, szIniPath) != 0);
	//g_DelayReducer = (GetPrivateProfileIntA("Helper", "NoDelay", 1, szIniPath) != 0);
}

void DoInit()
{
	ReadConfig();
	initLog();
	g_gameDllBase = GetModuleHandleA("game.dll");
	HMODULE stormDllBase = GetModuleHandleA("storm.dll");

	// ResetDelay(g_gameDllBase);
	ShowManaBar(g_gameDllBase, stormDllBase, true);
	spdlog::info("ManaBar loaded");

	if (g_WideScreen)
	{
		UpdateWideScreen(g_gameDllBase);
		spdlog::info("WideScreen loaded");
	}

	initJASS();
	spdlog::info("JASS Env loaded");

	HookChatMessage();
	spdlog::info("ChatMessage hooked");

	UnitCommandON();
	spdlog::info("UnitCommand loaded");

	HookCooldown();
	spdlog::info("Cooldown hooked");

	// GameStateInit(gameDllBase);
	// LoadWFE();
	// SetManaBarColor();
	// DreamUiInit(g_gameDllBase, stormDllBase, true);
}

void ResetDelay(LPVOID gameDllBase)
{
	int offset = 1; // 1.27 偏移量为1
	// 1.27
	// BYTE delay_reducer_sig_data[] = {
	// 	0xFF, 0xB8, 0xFF, 0xFA, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xC3, 0xFF, 0xCC, 0xFF, 0xCC,
	// 	0xFF, 0xCC, 0xFF, 0xCC, 0xFF, 0xCC, 0xFF, 0xCC, 0xFF, 0xCC, 0xFF, 0xCC, 0xFF, 0xCC, 0xFF, 0xCC};

	// 1.24
	BYTE delay_reducer_sig_data[] = {
		0xFF, 0x5E, 0xFF, 0xE9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0xFF, 0xCC, 0xFF, 0xCC, 0xFF, 0xCC, 0xFF, 0xCC,
		0xFF, 0xCC, 0xFF, 0xCC, 0xFF, 0xCC, 0xFF, 0xCC, 0xFF, 0xCC,
		0xFF, 0xCC, 0xFF, 0xB8, 0xFF, 0xFA, 0xFF, 0x00, 0xFF, 0x00,
		0xFF, 0x00, 0xFF, 0xC3};

	int len = sizeof(delay_reducer_sig_data) / 2;
	offset = 17;
	DWORD dwStartAddr = (DWORD)gameDllBase;
	DWORD dwEndAddr = (DWORD)gameDllBase + GAME_DLL_SIZE - len;
	bool found = true;
	while (dwStartAddr < dwEndAddr)
	{
		found = true;
		for (DWORD i = 0; i < len; i++)
		{
			BYTE code = *(BYTE *)(dwStartAddr + i);
			if (delay_reducer_sig_data[i * 2 + 1] != (code & delay_reducer_sig_data[i * 2]))
			{
				found = false;
				break;
			}
		}
		if (found)
		{
			spdlog::info("game.dll delay_reducer addr found: {}", dwStartAddr);
			break;
		}
		dwStartAddr++;
	}

	if (found && dwStartAddr != 0)
	{ // delay 设置为30
		WriteBytes((void *)(dwStartAddr + offset), (LPBYTE) "\x1E", 1);
	}
}

void DelayReducer(LPVOID dwGameDll)
{
	if (!g_DelayReducer)
	{
		// 552268 1.24
		WriteBytes((void *)((DWORD)dwGameDll + 0x552268), (LPBYTE) "\xD3\x90\x90\x90\x90", 5);
		WriteBytes((void *)((DWORD)dwGameDll + 0x55227A), (LPBYTE) "\x01", 1);
		TextPrint("|CFFFCD211 DMF|R: Delay Reducer turned |CFF00FF00On|R.", 3.0f);
		g_DelayReducer = true;
	}
	else
	{ // 96 58 22 00 00 8D 3C D5 00 00
		WriteBytes((void *)((DWORD)dwGameDll + 0x552268), (LPBYTE) "\x96\x58\x22\x00\x00", 5);
		WriteBytes((void *)((DWORD)dwGameDll + 0x55227A), (LPBYTE) "\x03", 1);
		TextPrint("|CFFFCD211 DMF|R: Delay Reducer turned |CFFFF0000Off|R.", 3.0f);
		g_DelayReducer = false;
	}
}

void DelayReducer2(LPVOID dwGameDll)
{
	// 从wfe获取的偏移量是这个
	// 0x6F6616A1
	// 0x6F662231
	DWORD delayNew1 = 10;
	DWORD delayNew2 = 10;
	if (!g_DelayReducer)
	{
		g_DelayOld1 = *(DWORD *)((DWORD)dwGameDll + 0x6616A1);
		spdlog::info("DelayOld1: {}", g_DelayOld1);
		delayNew1 = std::min<DWORD>(delayNew1, g_DelayOld1);
		WriteBytes((void *)((DWORD)dwGameDll + 0x6616A1), (LPBYTE)&delayNew1, 4);

		g_DelayOld2 = *(DWORD *)((DWORD)dwGameDll + 0x662231);	
		spdlog::info("DelayOld2: {}", g_DelayOld2);
		delayNew2 = std::min<DWORD>(delayNew2, g_DelayOld2);
		WriteBytes((void *)((DWORD)dwGameDll + 0x662231), (LPBYTE)&delayNew2, 4);

		TextPrint("|CFFFCD211 DMF|R: Delay Reducer turned |CFF00FF00On|R.", 3.0f);
		g_DelayReducer = true;
	}
	else
	{
		WriteBytes((void *)((DWORD)dwGameDll + 0x6616A1), (LPBYTE)&g_DelayOld1, 4);
		WriteBytes((void *)((DWORD)dwGameDll + 0x662231), (LPBYTE)&g_DelayOld2, 4);
		TextPrint("|CFFFCD211 DMF|R: Delay Reducer turned |CFFFF0000Off|R.", 3.0f);
		g_DelayReducer = false;
	}
}

void AutoSpellSkill()
{
	g_AutoSpellSkill = !g_AutoSpellSkill;
	if (g_AutoSpellSkill)
	{
		TextPrint("|CFFFCD211 DMF|R: AutoSpellSkill turned |CFF00FF00On|R.", 3.0f);
	}
	else
	{
		TextPrint("|CFFFCD211 DMF|R: AutoSpellSkill turned |CFFFF0000Off|R.", 3.0f);
	}
}

void SetManaBarColor()
{
	// 1.24e
	// FF 00 FF FF FF   00 00 00 E9 2E  89 08 B0 00 18  19 02
	// FF 00 FF FF FF   00 00 00 E9 2E  6A 18 B0 00 D1  18 02

	BYTE manabar_color_sig_data[] = {
		0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xE9, 0xFF, 0x2E,
		0x00, 0x00, 0x00, 0x00, 0xFF, 0xB0, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0xFF, 0x02};

	DWORD len = sizeof(manabar_color_sig_data) / 2;
	DWORD dwStartAddr = (DWORD)0x00000000;
	DWORD dwEndAddr = 0xFFFFFFFF - len;
	bool found = true;
	while (dwStartAddr < dwEndAddr)
	{
		found = true;
		for (DWORD i = 0; i < len; i++)
		{
			BYTE code = *(BYTE *)(dwStartAddr + i);
			if (manabar_color_sig_data[i * 2 + 1] != (code & manabar_color_sig_data[i * 2]))
			{
				found = false;
				break;
			}
		}
		if (found)
		{
			spdlog::info("game.dll manabar_color addr found: {}", dwStartAddr);
			break;
		}
		dwStartAddr++;
	}

	if (found && dwStartAddr != 0)
	{
		spdlog::info("SetManaBarColor loaded");
		WriteBytes((void *)(dwStartAddr + 1), (LPBYTE) "\xFF", 1);
		WriteBytes((void *)(dwStartAddr + 2), (LPBYTE) "\x80", 1);
		WriteBytes((void *)(dwStartAddr + 3), (LPBYTE) "\x00", 1);
	}
}

void WriteBytes(void *lpAddr, LPBYTE data, DWORD len)
{
	if (!lpAddr || !data)
	{
		return;
	}

	static DWORD dwProtect;

	if (VirtualProtect(lpAddr, len, PAGE_EXECUTE_READWRITE, &dwProtect))
	{
		memcpy((BYTE *)((DWORD)lpAddr), data, len);
		VirtualProtect(lpAddr, len, dwProtect, &dwProtect);
	}
}

void initLog()
{
	spdlog::init_thread_pool(8192, 1);
	auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/helper.log", true);
	file_sink->set_level(spdlog::level::debug);
	file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [thread %t] %v");
	std::vector<spdlog::sink_ptr> sinks = {file_sink};

	auto filelogger = std::make_shared<spdlog::async_logger>(
		"filelogger",
		file_sink,
		spdlog::thread_pool(),
		spdlog::async_overflow_policy::block);
	// 注册并设为默认日志器
	spdlog::register_logger(filelogger);
	spdlog::set_default_logger(filelogger);
	spdlog::set_level(spdlog::level::debug);
	spdlog::flush_on(spdlog::level::info);
}

#if 0
void GameStateInit(LPVOID gameDllBase)
{
	// 1.24e
	_LoadingBarPtr = (DWORD)gameDllBase + 0xAE3BC4;
	_ChangeLBText = (DWORD)gameDllBase + 0x6124E0;

	// 1.26
	// 	_LoadingBarPtr = War3.GameBase + 0xACCD6C;
	// 	_ChangeLBText = War3.GameBase + 0x611D40;
}

void HandleGameStateChange(DWORD newstate)
{
	switch (newstate)
	{
	case GS_NOTHING:
		break;
	case GS_LOBBY:
		break;
	case GS_LOADING:
		char Text[200];
		sprintf_s(Text, WideCharToUTF8(L"L O A D I N G : 已加载ManaBar插件 By 大米饭"));
		ChangeLoadingBarText(Text);
		break;
	case GS_INGAME:
		//lua_tinker::call<void>(L, "GameStateChange",newstate);
		//lua_tinker::call<void>(L, "GameBegin");
		break;
	case GS_PAUSEDGAME:
		break;
	default:
		break;
	}
	OldGamestate = newstate;
}

DWORD GetGameStateValue()
{
	DWORD rt;
	if (!ReadProcessMemory(GetCurrentProcess(), (LPVOID)(TlsValue + 4 * 0x0D), (LPVOID)&rt, 4, NULL))
		return -1;
	if (!ReadProcessMemory(GetCurrentProcess(), (LPVOID)(rt + 0x10), (LPVOID)&rt, 4, NULL))
		return -1;
	if (!ReadProcessMemory(GetCurrentProcess(), (LPVOID)(rt + 0x8), (LPVOID)&rt, 4, NULL))
		return -1;
	if (!ReadProcessMemory(GetCurrentProcess(), (LPVOID)(rt + 0x278), (LPVOID)&rt, 4, NULL))
		return -1;
	return rt;
}

void GameStateThread()
{
	while (true)
	{
		DWORD p = GetGameStateValue();
		if (p == -1)
			continue;
		if (GameStateUpdate(p))
			HandleGameStateChange(p);
		Sleep(10);
	}
}


bool ChangeLoadingBarText(char* newtext)
{
	__asm
	{
	looplbl:
		MOV EAX, DWORD PTR DS : [_LoadingBarPtr]
			MOV EAX, DWORD PTR DS : [EAX]
			TEST EAX, EAX;
		JE looplbl;
		MOV ECX, DWORD PTR DS : [EAX + 0x190] ;
		TEST ECX, ECX;
		JE endlbl;
		MOV EAX, newtext;
		PUSH EAX;
		CALL _ChangeLBText;
	}
	return true;
endlbl:
	return false;
}
#endif

// there is something wrong with this method. never use it
UINT MakeString(char *pszStr)
{
	static DWORD _datas[3];
	static DWORD *pjstr = &_datas[0];
	DWORD addrMakeStringFunc = (DWORD)g_gameDllBase + 0x012040;
	UINT jstr;
	__asm
	{
		mov eax, addrMakeStringFunc;
		mov ecx, pjstr;
		push pszStr;
		call eax;
		mov jstr, eax;
		pop eax;
	}
	return jstr;
}

void WINAPI HotKeys()
{
	std::string hname;
	if (g_gameDllBase == NULL)
	{
		g_gameDllBase = GetModuleHandleA("game.dll");
	}

	SetTls();
	bool bIsShown = false;
	while (true)
	{
		// GetLocalHero(hname);
		if (IsInGame())
		{
			if (!g_IsPlayerObserver && GetLocalHero(hname))
				TextPrint(std::format("|CFFFCD211 DMF|R: 玩家选择英雄 |CFF00FF00 {} |R.", MyGetUnitName(LocalHero)).c_str(), 5.0f);

			if (!bIsShown)
			{
				TextPrint("|CFFFCD211 DMF|R 大米饭显蓝消延迟改名插件帮助.\n"
						  "     |CFFFF0000F5|R              : Enable / Disable ShowManaBar (Enabled by default)\n"
						  "     |CFFFF0000F6|R              : Enable / Disable Lag Reducer\n"
						  "     |CFFFF0000F7|R              : Enable / Disable Auto cast skill\n"
						  "     |CFFFF0000F8|R              : Enable / Disable Ally ultimate cooldown alert\n"
						  "     |CFFFF0000 Home/End|R       : ZoomIn/ZoomOut\n"
						//   "     |CFFFF0000-cpn XXX |R       : Rename to XXX\n"
						  "     |CFFFF0000-mute X |R        : Mute x(x is player index)\n",
						  //"     |CFFFF0000BackSpace|R : Clear Screen",
						  10.0f);
				TextPrint("|CFFFCD211 DMF|R 注意:重选或者交换英雄后请再输入一次-repick.\n",10.0f);
				g_IsPlayerObserver = MyIsPlayerObserver(MyGetLocalPlayer());
				bIsShown = true;
				DelayReducer2(g_gameDllBase);
			}

			if (HotKeyPressed(VK_ShowManaBar))
			{
				spdlog::info("ShowManaBar turned On");
				TextPrint("|CFFFCD211 DMF|R: Show ManaBar turned |CFF00FF00On|R.", 5.0f);
				while (HotKeyPressed(VK_ShowManaBar))
					Sleep(100);
			}

			if (HotKeyPressed(VK_DelayReducer))
			{
				// DelayReducer(g_gameDllBase);
				DelayReducer2(g_gameDllBase);
				while (HotKeyPressed(VK_DelayReducer))
					Sleep(100);
			}

			if (HotKeyPressed(VK_AutoSpellSkill))
			{
				AutoSpellSkill();
				while (HotKeyPressed(VK_AutoSpellSkill))
					Sleep(100);
			}

			if (HotKeyPressed(VK_ZoomIn))
			{
				if (g_camdistance >= 1700.00f)
				{
					g_camdistance -= 50;
					float zero = 0.00f;
					MySetCameraField(0, &g_camdistance, &zero);
				}
				// while (HotKeyPressed(VK_ZoomIn))
				// 	Sleep(100);
			}

			if (HotKeyPressed(VK_ZoomOut))
			{
				if (g_camdistance < 3000.00f)
				{
					g_camdistance += 50;
					float zero = 0.00f;
					MySetCameraField(0, &g_camdistance, &zero);
				}
				// while (HotKeyPressed(VK_ZoomOut))
				// 	Sleep(100);
			}
		}

		Sleep(50);
	}
}

bool HotKeyPressed(int iKey)
{
	HWND hWnd = GetGameWindow();
	if (GetForegroundWindow() != hWnd)
		return false;

	if (GetAsyncKeyState(iKey))
		return true;

	return false;
}

void SetTls()
{
	DWORD Data = *(DWORD *)((DWORD)g_gameDllBase + 0xAE59AC);	  // 1.26 0xACEB4C
	DWORD TlsIndex = *(DWORD *)((DWORD)g_gameDllBase + 0xACEA4C); // 1.26 0xAB7BF4

	if (TlsIndex)
	{
		DWORD v5 = **(DWORD **)(*(DWORD *)(*(DWORD *)((DWORD)g_gameDllBase + 0xAE59BC) + 4 * Data) + 44);
		TlsSetValue(TlsIndex, *(LPVOID *)(v5 + 520));
	}
}

bool IsInGame()
{
	return *(DWORD *)((DWORD)g_gameDllBase + 0xACC594) == 4 && *(DWORD *)((DWORD)g_gameDllBase + 0xACC590) == 4;
}
