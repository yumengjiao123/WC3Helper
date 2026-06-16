// #include "pch.h"
#include <windows.h>
#include <detours.h>
#include <shlwapi.h>
#include "jass.h"

// spdlog
#include "spdlog/spdlog.h"

extern LPVOID g_gameDllBase;
extern DWORD LocalHero;

// 聊天玩家组， 12个位置，0表示禁言，1表示正常聊天
bool g_PlayerGroup[12] = {true, true, true, true, true, true, true, true, true, true, true, true};

void FunHook(void *pOldFuncAddr, void *pNewFuncAddr, void *&pCallBackFuncAddr);
void UnFunHook(void *pOldFuncAddr, void *pNewFuncAddr);

void(__fastcall *p_orgOnMyChatMessage)(int a1, int unused, int PlayerID, char *message, int a4, float a5) = 0;

void SetLocalPlayerName(const char *szName);

bool isRenameCommand(const std::string &input, std::string &outName)
{
	const std::string prefix = "-cpn ";

	if (input.length() <= prefix.length())
	{
		return false;
	}

	if (input.substr(0, prefix.length()) != prefix)
	{
		return false;
	}

	outName = input.substr(prefix.length());

	if (outName.empty())
	{
		return false;
	}

	return true;
}

// CREG真三地图，-repick命令
bool isCREGRepickCommand(const std::string &input)
{
	const std::string prefix = "-repick";

	if (input.length() < prefix.length())
	{
		return false;
	}

	if (input.substr(0, prefix.length()) != prefix)
	{
		return false;
	}

	return true;
}

bool isMuteCommand(const std::string &input, std::string &pid)
{
	const std::string prefix = "-mute ";

	if (input.length() <= prefix.length())
	{
		return false;
	}

	if (input.substr(0, prefix.length()) != prefix)
	{
		return false;
	}

	pid = input.substr(prefix.length());

	if (pid.empty())
	{
		return false;
	}

	return true;
}

void SetLocalPlayerName(const char *szName)
{
	MySetPlayerName(MyGetLocalPlayer(), szName);
}

// sub_6F2FBFC0
void __fastcall OnMyChatMessage(int a1, int unused, int PlayerID, char *message, int a4, float a5)
{
	if (message && MyGetPlayerId(MyGetLocalPlayer()) == PlayerID)
	{
		std::string input(message);
		std::string outName;
		// if (isRenameCommand(input, outName))
		// {
		// 	spdlog::info("Rename to: {}", outName);
		// 	SetLocalPlayerName(outName.c_str());
		// 	TextPrint(std::format("|CFFFCD211 DMF|R: Rename to |CFF00FF00{}|R.", outName).c_str());
		// 	return;
		// }
		// else 
		if (isMuteCommand(input, outName))
		{
			spdlog::info("玩家输入了-mute，屏蔽某人消息");
			int num = std::stoi(outName);
			if (num > 0 && num <= 10)
			{
				int pid = num;
				if (num > 5)
				{
					pid = num + 1;
				}
				g_PlayerGroup[pid] = !g_PlayerGroup[pid];
				TextPrint(std::format("|CFFFCD211 DMF|R: {}玩家|CFF00FF00{}|R发言.", g_PlayerGroup[pid] == 0 ? "屏蔽" : "允许", MyGetPlayerName(MyPlayer(pid))).c_str());
			}
		}
		else if (isCREGRepickCommand(input))
		{
			spdlog::info("玩家输入了-repick，需要重新获取当前英雄");
			LocalHero = 0;
		}
		else
		{
			spdlog::info("Unknown command: {}", input);
		}
	}

	if (p_orgOnMyChatMessage && g_PlayerGroup[PlayerID] == 1)
	{
		p_orgOnMyChatMessage(a1, unused, PlayerID, message, a4, a5);
	}
}

void HookChatMessage()
{
	DWORD offset = (DWORD)g_gameDllBase;
	offset += 0x2FBFC0;
	FunHook((void *)offset, (void *)OnMyChatMessage, (void *&)p_orgOnMyChatMessage);
}
