// #include "pch.h"
#include "common.h"
#include "plugins/mana.h"
#include "plugins/widescreen.h"
#include <shlwapi.h>
#include <intrin.h>
#include <stdlib.h>

// spdlog：日志内部按 UTF-8 存储，打开宽字符重载以便直接输出 wchar_t 路径
#define SPDLOG_WCHAR_TO_UTF8_SUPPORT
#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"

bool g_WideScreen = true;

// 蓝条颜色（ARGB 0xAARRGGBB），启动时由 helper.ini 的 [Helper] ManaBarColor 覆盖，详见 ReadConfig
DWORD g_manaBarColor = 0xFF00AAFF;

LPVOID g_gameDllBase = nullptr;
LPVOID g_stormDllBase = nullptr;

// 外部 storm80.mix（降延迟）的加载状态，详见 LoadStorm80()
static HMODULE g_storm80 = nullptr;
static wchar_t g_storm80Path[MAX_PATH] = {0};
static bool g_LoadStorm80 = false;

void DoInit();
void initLog();
void HookCooldown();
void UnHookCooldown();

static void HideModuleFromPEB(HMODULE hModule)
{
	if (!hModule)
	{
		return;
	}

	// x86：PEB 在 fs:[0x30]，PEB->Ldr 在 +0x0C。
	// PEB_LDR_DATA 里三条链表依次在 +0x0C / +0x14 / +0x1C。
	BYTE *ldr = *(BYTE **)(__readfsdword(0x30) + 0x0C);
	if (!ldr)
	{
		return;
	}

	DWORD target = (DWORD)hModule;

	struct
	{
		DWORD headOffset;	 // 链表头在 PEB_LDR_DATA 里的偏移
		DWORD dllBaseOffset; // 从链表节点算 DllBase 的偏移
	} lists[] = {
		{0x0C, 0x18}, // InLoadOrderModuleList
		{0x14, 0x10}, // InMemoryOrderModuleList
		{0x1C, 0x08}, // InInitializationOrderModuleList
	};

	for (auto &l : lists)
	{
		LIST_ENTRY *head = (LIST_ENTRY *)(ldr + l.headOffset);
		if (IsBadReadPtr(head, sizeof(LIST_ENTRY)))
		{
			continue;
		}

		LIST_ENTRY *node = head->Flink;
		while (node != head && !IsBadReadPtr(node, sizeof(LIST_ENTRY)))
		{
			if (*(DWORD *)((BYTE *)node + l.dllBaseOffset) == target)
			{
				// 标准 unlink：把自己从这条链表里摘掉
				node->Blink->Flink = node->Flink;
				node->Flink->Blink = node->Blink;
				break;
			}
			node = node->Flink;
		}
	}
}

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
		// 从 PEB 模块链表摘除自己（storm80 的隐藏 DLL 手法）
		HideModuleFromPEB(hModule);
		break;
	case DLL_PROCESS_DETACH:
		UnHookCooldown();
		break;
	}
	return TRUE;
}

// 解析颜色字符串，统一按十六进制，允许 0x / 0X / # 前缀。
// 不足 8 位时按 RRGGBB 处理并补上不透明 alpha；解析不出数字则返回 def。
static DWORD ParseHexColor(const wchar_t *text, DWORD def)
{
	if (text == nullptr)
	{
		return def;
	}

	while (*text == L' ' || *text == L'\t')
	{
		text++;
	}

	if (text[0] == L'0' && (text[1] == L'x' || text[1] == L'X'))
	{
		text += 2;
	}
	else if (text[0] == L'#')
	{
		text++;
	}

	wchar_t *end = nullptr;
	unsigned long value = wcstoul(text, &end, 16);
	if (end == text)
	{
		return def;
	}

	if (end - text <= 6)
	{
		value |= 0xFF000000;
	}

	return (DWORD)value;
}

// 配置文件缺失时按默认值写出一份（带注释），方便用户直接在 ini 里改
static void WriteDefaultConfig(const wchar_t *iniPath)
{
	// 整份内容直接用宽字符落盘（UTF-16LE + BOM），键值仍然都是 ASCII
	static const wchar_t kDefaultIni[] =
		L"; WC3Helper (ManaBar) 配置文件\n"
		L"; 文件不存在时按默认值自动生成，修改后重启游戏生效\n"
		L"\n"
		L"[Helper]\n"
		L"; 宽屏支持：1=开启，0=关闭\n"
		L"WideScreen=1\n"
		L"\n"
		L"; 蓝条颜色：0xAARRGGBB，也可只写 6 位 RRGGBB（alpha 自动补 FF）\n"
		L"; 例：0xFFFF8C00 橙 / 0xFF00E5EE 蓝绿 / 00FF00 绿\n"
		L"ManaBarColor=0xFF00AAFF\n"
		L"\n"
		L"; 降延迟：1=开启，0=关闭。开启后加载同目录下的 strom80.dll\n"
		L"NoDelay=0\n";

	HANDLE hFile = CreateFile(iniPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return;
	}

	// 先写 UTF-16LE BOM，再写正文（去掉结尾的 L'\0'）
	const wchar_t bom = 0xFEFF;
	DWORD written = 0;
	WriteFile(hFile, &bom, sizeof(bom), &written, nullptr);
	WriteFile(hFile, kDefaultIni, (DWORD)(sizeof(kDefaultIni) - sizeof(wchar_t)), &written, nullptr);
	CloseHandle(hFile);
}

void ReadConfig()
{
	wchar_t szIniPath[MAX_PATH] = {0};
	GetCurrentDirectory(MAX_PATH, szIniPath);

	if (StrStrI(szIniPath, L"\\YY") != nullptr)
	{
		PathRemoveFileSpec(szIniPath);
	}

	wcscat(szIniPath, L"\\helper.ini");

	// ini 不存在则先创建一份默认配置，后面的读取逻辑保持不变
	if (GetFileAttributes(szIniPath) == INVALID_FILE_ATTRIBUTES)
	{
		WriteDefaultConfig(szIniPath);
	}

	g_WideScreen = (GetPrivateProfileInt(L"Helper", L"WideScreen", 1, szIniPath) != 0);

	// ManaBarColor：写 0xAARRGGBB / AARRGGBB，或者只写 6 位 RRGGBB（alpha 自动补 FF）。
	// 例：0xFFFF8C00 橙 / 0xFF00E5EE 蓝绿 / 00FF00 绿
	wchar_t szColor[32] = {0};
	GetPrivateProfileString(L"Helper", L"ManaBarColor", L"0xFF00AAFF", szColor, 32, szIniPath);
	g_manaBarColor = ParseHexColor(szColor, g_manaBarColor);

	// NoDelay=1 时才加载外部 storm80.mix（见 LoadStorm80）
	g_LoadStorm80 = (GetPrivateProfileInt(L"Helper", L"NoDelay", 0, szIniPath) != 0);

	// storm80.mix 与 helper.ini 同目录
	wcscpy(g_storm80Path, szIniPath);
	if (wchar_t *slash = wcsrchr(g_storm80Path, L'\\'))
	{
		slash[1] = 0;
		wcscat(g_storm80Path, L"strom80.dll");
	}
}

static void LoadStorm80()
{
	if (!g_LoadStorm80 || g_storm80)
	{
		return;
	}

	Version ver = GetWar3Version();
	if (ver != Version::v124b && ver != Version::v124e && ver != Version::v126a)
	{
		spdlog::info("storm80: skipped, unsupported build {}", (DWORD)ver);
		return;
	}

	if (!g_storm80Path[0])
	{
		spdlog::error("storm80: path is empty, skipped");
		return;
	}

	g_storm80 = LoadLibrary(g_storm80Path);
	if (!g_storm80)
	{
		spdlog::error(L"storm80: LoadLibrary('{}') failed, err {}", g_storm80Path, GetLastError());
		return;
	}

	spdlog::info(L"storm80: loaded from '{}'", g_storm80Path);
}

void DoInit()
{
	ReadConfig();
	initLog();
	spdlog::info("ManaBarColor = 0x{:08X}", g_manaBarColor);
	g_gameDllBase = GetModuleHandleA("game.dll");
	g_stormDllBase = GetModuleHandleA("storm.dll");

	ShowManaBar(g_gameDllBase, g_stormDllBase, true);
	spdlog::info("ManaBar loaded");

	if (g_WideScreen)
	{
		UpdateWideScreen(g_gameDllBase);
		spdlog::info("WideScreen loaded");
	}

	HookCooldown();
	spdlog::info("Cooldown hooked");

	LoadStorm80();
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

Version GetWar3Version()
{
	uint8_t p124b[] = {0x80, 0xBE, 0xA8, 0x01};
	uint8_t p124e[] = {0x8B, 0x50, 0x3C, 0x3B};
	uint8_t p126a[] = {0x7c, 0x73, 0x63, 0x6f};
	uint8_t p127a[] = {0xcc, 0xcc, 0xcc, 0x55};

	const uint8_t *veraddr = (const uint8_t *)((DWORD)g_gameDllBase + 0x636F5D);
	if (0 == memcmp(p124e, veraddr, sizeof(p124e)))
	{
		return Version::v124e;
	}
	else if (0 == memcmp(p126a, veraddr, sizeof(p126a)))
	{
		return Version::v126a;
	}
	else if (0 == memcmp(p127a, veraddr, sizeof(p127a)))
	{
		return Version::v127a;
	}
	else
	{
		return Version::unknown;
	}
}
