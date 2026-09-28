#include "UnitCommand.h"
#include "common.h"
#include <memory>
#include <map>

#include "cooldowndraw.h"
// spdlog
#include "spdlog/spdlog.h"

DWORD LocalHero = 0;
extern LPVOID g_gameDllBase;
extern bool g_AutoSpellSkill;

DWORD addrCmdHookConst;
DWORD addrUnitCmdHook;
DWORD addrUnitCmdHookJMP;

bool IsInGame();

static std::map<DWORD, DWORD> units;

void ZSGWS_T(DWORD hUnit, float x, float y)
{
	if (LocalHero == 0)
	{
		return;
	}

	if (MyUseItem(LocalHero, ITEM_XW))
	{
		// MyWriteTextToScreen("[|cFFFF9900JasHack|r]|cFF999999:|r触发|cFFCE0005[玄武]|r躲技能!!!");
		if (MyGetUnitTypeId(LocalHero) == HERO_ID_XUN)
		{
			// MyUseSkillTarget(LocalHero, Action_XUN_W, hUnit);
			float ax = MyGetUnitX(hUnit);
			float ay = MyGetUnitY(hUnit);
			MyUseSkillLoc(LocalHero, Action_XUN_W, ax, ay);
			return;
		}
		return;
	}

	// 当前英雄是张飞
	if (MyGetUnitTypeId(LocalHero) == HERO_ID_ZF)
	{
		MyUseSkill(LocalHero, Action_ZF_V);
	}

	if (MyUseItemTarget(LocalHero, ITEM_YZ, hUnit))
	{
		// MyWriteTextToScreen("[|cFFFF9900JasHack|r]|cFF999999:|r触发|cFFCE0005[巫术魔杖]|r躲技能!!!");
		if (MyGetUnitTypeId(LocalHero) == HERO_ID_XUN)
		{
			// MyUseSkillTarget(LocalHero, Action_XUN_W, hUnit);
			float ax = MyGetUnitX(hUnit);
			float ay = MyGetUnitY(hUnit);
			MyUseSkillLoc(LocalHero, Action_XUN_W, ax, ay);
			// 用下面方法会desync
			// MyIssueTargetOrderById(LocalHero, Action_XUN_W, hUnit);
			return;
		}
		return;
	}

	if (MyUseItemTarget(LocalHero, ITEM_FZ, hUnit))
	{
		// MyWriteTextToScreen("[|cFFFF9900JasHack|r]|cFF999999:|r触发|cFFCE0005[风暴之杖]|r躲技能!!!");
		return;
	}

	Location loc = MyGetUnitFaceLoc(LocalHero, 700);
	if (MyUseItemLoc(LocalHero, ITEM_XX, loc.X, loc.Y))
	{
		// MyWriteTextToScreen(WideCharToUTF8(L"[|cFFFF9900JasHack|r]|cFF999999:|r触发|cFFCE0005[翔靴]|r躲技能!!!"));
		return;
	}

	if (MyGetUnitTypeId(LocalHero) == HERO_ID_GY) // 关羽自动D
	{
		float ax = MyGetUnitX(hUnit);
		float ay = MyGetUnitY(hUnit);
		if (MyIsCanHurtMe(350, ax, ay))
		{
			spdlog::info("关羽D躲T");
			MyUseSkillLoc(LocalHero, Action_GY_D, ax, ay);
			return;
		}
	}

	if (MyGetUnitTypeId(LocalHero) == HERO_ID_CR) // 曹仁自动E
	{
		MyUseSkillTarget(LocalHero, Action_CR_E, hUnit);
		// MyIssueTargetOrderById(LocalHero, Action_CR_E, hUnit);
		return;
	}
#if 0
	if (MyGetUnitTypeId(LocalHero) == HERO_ID_GJ)
	{
		// MyUseSkillTarget(LocalHero, Action_GJ_F, hUnit);
		// MyIssueTargetOrderById(LocalHero, Action_GJ_C, hUnit);
		float ax = MyGetUnitX(hUnit);
		float ay = MyGetUnitY(hUnit);
		MyUseSkillLoc(LocalHero, Action_GJ_C, ax, ay);
		return;
	}
#endif
	if (MyGetUnitTypeId(LocalHero) == HERO_ID_SM) // 司马懿，自动放e
	{
		float ax = MyGetUnitX(hUnit);
		float ay = MyGetUnitY(hUnit);
		MyUseSkillLoc(LocalHero, Action_41_E, ax, ay);
		return;
	}

	if (MyGetUnitTypeId(LocalHero) == HERO_ID_XUN)
	{
		// MyUseSkillTarget(LocalHero, Action_XUN_W, hUnit);
		float ax = MyGetUnitX(hUnit);
		float ay = MyGetUnitY(hUnit);
		MyUseSkillLoc(LocalHero, Action_XUN_W, ax, ay);
		// 用下面方法会desync
		// MyIssueTargetOrderById(LocalHero, Action_XUN_W, hUnit);
		return;
	}
}

void ProcessGameCmd(Unit *unit, Command *CommandData, DWORD targetUint)
{
	if (!g_AutoSpellSkill || LocalHero == 0)
	{
		return;
	}

	DWORD handle = MyGetUnitByOffset((DWORD)unit);

	switch (CommandData->CommandId)
	{
	case 852414:
	{
		spdlog::info("852414");
		break;
	}
	case Action_GJ_C: // Action_41_E
	{
		// 郭嘉冰河爆裂破
		if (MyIsUnitEnemy(handle, MyGetLocalPlayer()))
		{

			float ax = MyGetUnitX(handle);
			float ay = MyGetUnitY(handle);
			float distance = 490;

			if (unit->dwClassId == HERO_ID_GJ)
			{
				distance = 600;
			}

			if (MyIsCanHurtMe(490, ax, ay))
			{
				// 当前英雄是张飞
				if (MyGetUnitTypeId(LocalHero) == HERO_ID_ZF)
				{
					MyUseSkill(LocalHero, Action_ZF_V);
				}
			}
		}

		break;
	}
	case Action_GY_B:
	case Action_ZY_B:
	{
		// 赵云或者关羽开大
		if (MyIsUnitEnemy(handle, MyGetLocalPlayer()) && (unit->dwClassId == HERO_ID_GY || unit->dwClassId == HERO_ID_ZY))
		{
			float ax = MyGetUnitX(handle);
			float ay = MyGetUnitY(handle);
			if (MyIsCanHurtMe(490, ax, ay))
			{
				// 当前英雄是荀彧
				if (MyGetUnitTypeId(LocalHero) == HERO_ID_XUN)
				{
					MyUseSkillLoc(LocalHero, Action_XUN_W, ax, ay);
					// MyUseSkillTarget(LocalHero, Action_XUN_W, handle);
				}
			}
		}
		break;
	}
	case Action_ZG_E:
	{
		// 诸葛 极冻凝结
		if (true)
		//if (MyIsUnitEnemy(handle, MyGetLocalPlayer()) && unit->dwClassId == HERO_ID_ZG)
		{
			float ax = MyGetUnitX(handle);
			float ay = MyGetUnitY(handle);
			// 如果有风就吹起，可能是飞e
			if (MyIsCanHurtMe(350, ax, ay) && MyUseItemTarget(LocalHero, ITEM_FZ, handle))
			{
				break;
			}

			if (MyGetUnitTypeId(LocalHero) == HERO_ID_XUN)
			{
				if (MyIsCanHurtMe(490, ax, ay)) // 寻的w施法距离是500，给10的误差
				{
					MyUseSkillLoc(LocalHero, Action_XUN_W, ax, ay);
					// MyUseSkillTarget(LocalHero, Action_XUN_W, handle);
				}
			}
			// 当前英雄是点位
			else if (MyGetUnitTypeId(LocalHero) == HERO_ID_DW || MyGetUnitTypeId(LocalHero) == HERO_ID_ZF)
			{
				// float ax = MyGetUnitX(handle);
				// float ay = MyGetUnitY(handle);
				if (MyIsCanHurtMe(349, ax, ay)) // 点位的t有效距离是350
				{
					MyUseSkill(LocalHero, Action_T);
					// MyUseSkillTarget(LocalHero, Action_XUN_W, handle);
				}
			}
			else if (MyGetUnitTypeId(LocalHero) == HERO_ID_GJ)
			{
				// float ax = MyGetUnitX(handle);
				// float ay = MyGetUnitY(handle);
				MyUseSkillLoc(LocalHero, Action_GJ_C, ax, ay);
			}
		}
		break;
	}
	case Action_T:
	{
		if (MyIsUnitEnemy(handle, MyGetLocalPlayer()))
		{
			// 张飞或者典韦 飞t
			if (unit->dwClassId == HERO_ID_ZF || unit->dwClassId == HERO_ID_DW)
			{
				float ax = MyGetUnitX(handle);
				float ay = MyGetUnitY(handle);
				if (MyIsCanHurtMe(400, ax, ay))
				{
					ZSGWS_T(handle, ax, ay);
				}
			}
		}
		break;
	}
#if 0
	case SLOT_INDEX_START:
	case SLOT_INDEX_START + 1:
	case SLOT_INDEX_START + 2:
	case SLOT_INDEX_START + 3:
	case SLOT_INDEX_START + 4:
	case SLOT_INDEX_START + 5: {
		if (MyIsUnitEnemy(handle, MyGetLocalPlayer()) && unit->dwClassId != HERO_ID_GY)
		{
			float ax = MyGetUnitX(handle);
			float ay = MyGetUnitY(handle);
			if (MyIsCanHurtMe(490, ax, ay))
			{
				//使用了物品栏
				auto itemid = GetUnitSlotItemID(handle, CommandData->CommandId - SLOT_INDEX_START);
				// 当前英雄是荀彧
				if (ITEM_XX == itemid && MyGetUnitTypeId(LocalHero) == HERO_ID_XUN)
				{
					MyUseSkillLoc(LocalHero, Action_XUN_W, ax, ay);
					// MyUseSkillTarget(LocalHero, Action_XUN_W, handle);
				}
			}
		}
		break;
	}
#endif
	default:
		break;
	}
}

void _declspec(naked) UnitCommandHook()
{
	__asm
	{
		PUSH EBP;
		MOV EBP, ESP;
		SUB ESP, 0x38;
		PUSHAD;
	}
	Unit *unt;
	Command *CommandData;
	DWORD targetUint;
	__asm
	{
		MOV EAX, DWORD PTR DS : [EBP + 0xC] ;
		mov unt, EAX;
		MOV EAX, DWORD PTR DS : [EBP + 0x10] ;
		mov CommandData, EAX;
		MOV EAX, DWORD PTR DS : [EBP + 0x80] ;
		mov targetUint, EAX;
	}
	ProcessGameCmd(unt, CommandData, targetUint);
	__asm
	{
		POPAD;
		MOV ESP, EBP;
		POP EBP;
		PUSH addrCmdHookConst;
		JMP addrUnitCmdHookJMP;
	}
}

// ============================================================================
// War3 1.27 订单派发入口 hook
// ----------------------------------------------------------------------------
// 依据 ref/Game124.dll.c 与 ref/Game127.dll.c 比对：
//   1.24 sub_6F2CB320(unsigned __int16 a1, int a2, int *a3, int *a4, int a5)
//        a1=flags, a2=orderId, a3=CUnit*, a4=COrderTarget*
//        （__fastcall：flags@ECX、orderId@EDX，CUnit*/COrderTarget* 在栈上）
//        该函数为 FPO（无 push ebp/mov ebp,esp），入口即 `push -1; push offset SEH`；
//        旧 hook 打在 +2 的 `push offset SEH_6F2CB320`(=0x6F82AE00) 处，
//        此时 EBP 仍指向调用者帧，故 [EBP+0xC]/[EBP+0x10] 正好是 a3/a4。
//   1.27 sub_6F69BC60(int *a1@<edx>, int *a2@<ecx>, ..., unsigned __int16 a4, int a5)
//        a2(ECX)=CUnit*、a1(EDX)=COrderTarget*、a4/a5(栈)=flags/orderId
//        对应关系由 v6[144]/v6[145]、a4[8]|=4u、sub_6F3332A0+sub_6F333240
//        ≙ a2[144]/a2[145]、a1[8]|=4u、sub_6F359A30+sub_6F3599F0 逐行确认。
// 因此 1.27 直接在函数入口取 ECX/EDX（此时尚未被覆盖）即可拿到
// CUnit* 与 COrderTarget*，无需像 1.24 那样从栈帧偏移读取。
// 入口被覆盖的原始 5 字节为：55 8B EC 6A FF
//        push ebp / mov ebp,esp / push 0FFFFFFFFh
// 结束时原样回放，再跳到 +5 处的 `push offset SEH_6F69BC60` 继续执行。
// ============================================================================
void _declspec(naked) UnitCommandHook127()
{
	__asm
	{
		PUSH EBP;
		MOV EBP, ESP;
		SUB ESP, 0x20;
		PUSHAD;
	}
	Unit *unt;
	Command *CommandData;
	__asm
	{
		// PUSHAD 保存区（自上而下 EDI/ESI/EBP/ESP/EBX/EDX/ECX/EAX）
		MOV EAX, DWORD PTR DS : [ESP + 0x18] ; // ECX = CUnit*
		mov unt, EAX;
		MOV EAX, DWORD PTR DS : [ESP + 0x14] ; // EDX = COrderTarget* (Command*)
		mov CommandData, EAX;
	}
	ProcessGameCmd(unt, CommandData, 0);
	__asm
	{
		POPAD;
		MOV ESP, EBP;
		POP EBP;
		// 回放被覆盖的原始序言，保证 EH 帧建立过程不变
		PUSH EBP;
		MOV EBP, ESP;
		PUSH 0xFFFFFFFF;
		JMP addrUnitCmdHookJMP;
	}
}

void PatchJMP(BYTE *source, BYTE *destination, const int length)
{
	BYTE *jump = (BYTE *)malloc(length + 5);
	DWORD oldProtection;
	VirtualProtect(source, length, PAGE_EXECUTE_READWRITE, &oldProtection);
	memcpy(jump, source, length);
	jump[length] = 0xE9;
	*(DWORD *)(jump + length) = (DWORD)((source + length) - (jump + length)) - 5;
	source[0] = 0xE9;
	*(DWORD *)(source + 1) = (DWORD)(destination - source) - 5;
	for (int i = 5; i < length; i++)
		source[i] = 0x90;
	VirtualProtect(source, length, oldProtection, &oldProtection);
}

void UnitCommandON()
{
	const Version ver = GetWar3Version();
	if (ver == Version::v127a)
	{
		// 1.27：hook 订单派发函数 sub_6F69BC60 的入口
		//   覆盖 5 字节：55 8B EC 6A FF (push ebp / mov ebp,esp / push -1)
		//   回跳地址 +5 => push offset SEH_6F69BC60
		addrUnitCmdHook = (DWORD)g_gameDllBase + 0x69BC60;
		addrUnitCmdHookJMP = (DWORD)g_gameDllBase + 0x69BC65;
		PatchJMP((BYTE *)(addrUnitCmdHook), (BYTE *)UnitCommandHook127, 5);
	}
	else
	{
		// 1.24b/1.24e：沿用原字节补丁（+2 处 `push offset SEH_6F2CB320`，非函数入口）
		addrCmdHookConst = (DWORD)g_gameDllBase + 0x82AE00;
		addrUnitCmdHook = (DWORD)g_gameDllBase + 0x2CB322; // 2CB320
		addrUnitCmdHookJMP = (DWORD)g_gameDllBase + 0x2CB327;
		PatchJMP((BYTE *)(addrUnitCmdHook), (BYTE *)UnitCommandHook, 5);
	}
}

bool GetLocalHero(std::string &name)
{
#ifndef WC3HELPER_BASIC
	if (LocalHero != 0)
	{
		return false;
	}

	if (IsInGame())
	{
		DWORD len = 0;
		DWORD arr = JGetUnitArray(len);

		for (DWORD i = 0; i < len; i++)
		{
			CUnit *unt = (CUnit *)*(DWORD *)(arr + i * 4);
			DWORD handle = MyGetUnitByOffset((DWORD)unt);
			if (!MyIsUnitIllusion(handle) && MyIsUnitHero(handle))
			{
				name = MyGetUnitName(handle);
				spdlog::info("英雄： {} 地址： 0x{:08X}", name, (DWORD)unt);
				if (MyIsUnitOwnedByPlayer(handle, MyGetLocalPlayer()))
				{
					LocalHero = handle;
					// spdlog::info("玩家选择英雄: {}", name);
					return true;
				}
			}
		}
	}

#endif
	return false;
}
