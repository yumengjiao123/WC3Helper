#include "jass.h"
#include "common.h"
#include <cmath>

#include "spdlog/spdlog.h"

extern LPVOID g_gameDllBase;

// ============================================================================
// War3 1.24 <-> 1.27 适配说明（依据 ref/Game124.dll.c 与 ref/Game127.dll.c）
// ----------------------------------------------------------------------------
// 1) JASS 原生函数：两个版本都以「函数名 -> 实现地址」的注册表登记
//    （1.24 注册点 sub_6F455C20，1.27 注册点 sub_6F7E3710）。同名原生两两对应，
//    因此可直接按函数名在两个反编译文件中取到 1.27 的实现地址。
// 2) 非原生内部函数（SendAction / WidgetPtr / ItemPtr / UnitName 等）：
//    以「调用方一致」比对——1.27 的原生函数体会调用与 1.24 相同的内部函数序列，
//    由调用点逐一定位；下文每处均标注 1.24 与 1.27 的函数体对应关系。
// 3) 反编译签名中的 `double x@<st0>` 全部是 Hex-Rays 的伪参数（汇编未读取 ST0，
//    见 Game127.dll.asm 中 sub_6F1F3D10 / sub_6F69FFA0 / sub_6F26FF80 的序言），
//    因此 1.27 的真实参数个数/顺序与 1.24 一致，无需额外压栈。
// 4) 结构体/内存偏移：经 GetUnitTypeId（CUnit+0x30）、IsUnitAlly、SelectUnitReal
//    等逐条比对，CUnit 与 CGameUI 的相关字段偏移在 1.24/1.27 间保持不变。
// ============================================================================

typedef HITEM(__cdecl *pUnitItemInSlot)(HUNIT hUnit, int slot);
typedef int(__cdecl *pGetItemTypeId)(HITEM hItem);
typedef void(__cdecl *pSelectUnit)(HUNIT hUnit, bool flag);
typedef void(__cdecl *pClearSelection)();
typedef int(__cdecl *pGetUnitTypeId)(HUNIT hUnit);
typedef HPLAYER(__cdecl *pGetLocalPlayer)();
typedef void(__cdecl *pSetPlayerNameFunc)(void *, DWORD *);
typedef int(__cdecl *pGetPlayerNameFunc)(void *);
typedef Float(__cdecl *pGetUnitX)(HUNIT hUnit);
typedef Float(__cdecl *pGetUnitY)(HUNIT hUnit);
typedef Float(__cdecl *pGetUnitFacing)(HUNIT hUnit);
typedef bool(__cdecl *pIsUnitEnemy)(HUNIT hUnit, HPLAYER hPlayer);
typedef int(__cdecl *pGetPlayerId)(HPLAYER hPlayer);
typedef HPLAYER(__cdecl *pPlayer)(int Index);
typedef bool(__cdecl *pIsUnitIllusion)(HUNIT hUnit);
typedef bool(__cdecl *pIsUnitOwnedByPlayer)(HUNIT hUnit, HPLAYER hPlayer);
typedef int(__cdecl *pGetHeroLevel)(HUNIT hUnit);

typedef bool(__cdecl *pIssueTargetOrderById)(HUNIT hUnit, int order, HUNIT hTarget);
typedef bool(__cdecl *pSetCameraField)(DWORD hUnit, float *value, float *duration);

using pIsPlayerObserver = int(__cdecl*)(HPLAYER a1);
using pIsPlayerEnemy = BOOL(__cdecl*)(HPLAYER a1, HPLAYER a2);
using pGetOwningPlayer = HPLAYER(__cdecl*)(HUNIT hUnit);
using pSelectUnitReal = void(__thiscall*)(int pPlayerSelectData, HUNIT pUnit, int id, int unk1, int unk2, int unk3);
using pUpdatePlayerSelection = void(__thiscall*)(int pPlayerSelectData, int unk);
// 1.24 sub_6F333240 / 1.27 sub_6F3599F0：选中流程收尾（CGameUI 相关）
using pCGameUIReset = int(__thiscall*)(void *a1);

pUnitItemInSlot UnitItemInSlot = nullptr;
pGetItemTypeId GetItemTypeId = nullptr;
pSelectUnit SelectUnit = nullptr;
pClearSelection ClearSelection = nullptr;
pGetUnitTypeId GetUnitTypeId = nullptr;
pGetLocalPlayer GetLocalPlayer = nullptr;
pSetPlayerNameFunc SetPlayerName = nullptr;
pGetPlayerNameFunc GetPlayerName = nullptr;
pGetUnitX GetUnitX = nullptr;
pGetUnitY GetUnitY = nullptr;
pGetUnitFacing GetUnitFacing = nullptr;
pIsUnitEnemy IsUnitEnemy = nullptr;
pGetPlayerId GetPlayerId = nullptr;
pPlayer Player = nullptr;
pIsUnitIllusion IsUnitIllusion = nullptr;
pIsUnitOwnedByPlayer IsUnitOwnedByPlayer = nullptr;
pGetHeroLevel GetHeroLevel = nullptr;
pSetCameraField SetCameraField = nullptr;
pGetOwningPlayer GetOwningPlayer = nullptr;
pIsPlayerEnemy IsPlayerEnemy = nullptr;
pIsPlayerObserver IsPlayerObserver = nullptr;
pIssueTargetOrderById IssueTargetOrderById = nullptr;

pSelectUnitReal SelectUnitReal = nullptr;
pUpdatePlayerSelection UpdatePlayerSelection = nullptr;
pCGameUIReset CGameUIReset = nullptr;

DWORD addrGetItemPtr;
DWORD addrSendActionNoNaked;
DWORD addrSendActionNaked;
DWORD addrSendActionNaked2;
DWORD addrGetItemState;
DWORD addrGetStatePtr;
DWORD addrGetStateEcx;
DWORD addrGetWidgetPtr;
DWORD addrUseItemNoLoc;
DWORD addrLocalPlayerOffset;
DWORD addrGetUnitHandle1;
DWORD addrGetUnitHandle2;
DWORD addrUnitName1;
DWORD addrUnitName2;
DWORD addrGetPlayerName1;
DWORD addrGetPlayerName2;

DWORD W3XGlobalClass;
DWORD PrintToScreen;
DWORD addrGlobalClass;
DWORD addrGetUnitArrayPtr;

DWORD UnitVtable;

// 1.24 sub_6F012040 / 1.27 sub_6F0506D0：构造游戏内 RCString（原为 GetJassString 内局部变量）
DWORD addrMakeStringFunc;

extern DWORD LocalHero;

char tmpStr[100] = {0};

void initJASS()
{
	// 版本判定：决定下面每个偏移取 1.24 还是 1.27 的值
	const Version ver = GetWar3Version();
	const bool is127 = (ver == Version::v127a);

	// 取偏移：v124 为 1.24 偏移，v127 为 1.27 偏移（均相对 game.dll 基址）
	const DWORD base = (DWORD)g_gameDllBase;
	auto A = [base, is127](DWORD v124, DWORD v127) -> DWORD
	{
		return base + (is127 ? v127 : v124);
	};

	// ---------------- JASS 原生函数（按注册表函数名对应） ----------------
	UnitItemInSlot = (pUnitItemInSlot)A(0x3C8270, 0x1FAF50); // "UnitItemInSlot"
	GetItemTypeId = (pGetItemTypeId)A(0x3C57A0, 0x1E2CC0);	 // "GetItemTypeId"
	SelectUnit = (pSelectUnit)A(0x3C8450, 0x1F3D10);		 // "SelectUnit"
	ClearSelection = (pClearSelection)A(0x3BC5E0, 0x1DB310);	 // "ClearSelection"
	GetUnitTypeId = (pGetUnitTypeId)A(0x3C6450, 0x1E6670);	 // "GetUnitTypeId"
	GetLocalPlayer = (pGetLocalPlayer)A(0x3BC6A0, 0x1E3150); // "GetLocalPlayer"
	SetPlayerName = (pSetPlayerNameFunc)A(0x3C1A50, 0x1F6240); // "SetPlayerName"
	GetPlayerName = (pGetPlayerNameFunc)A(0x3C1AA0, 0x1E3D40); // "GetPlayerName"
	GetUnitX = (pGetUnitX)A(0x3C6050, 0x1E66B0);			 // "GetUnitX"
	GetUnitY = (pGetUnitY)A(0x3C6090, 0x1E66F0);			 // "GetUnitY"
	GetUnitFacing = (pGetUnitFacing)A(0x3C62D0, 0x1E6130);	 // "GetUnitFacing"
	IsUnitEnemy = (pIsUnitEnemy)A(0x3C8610, 0x1E85C0);		 // "IsUnitEnemy"
	GetPlayerId = (pGetPlayerId)A(0x3CA180, 0x1E3D20);		 // "GetPlayerId"
	Player = (pPlayer)A(0x3BC670, 0x1F1E70);				 // "Player"
	GetHeroLevel = (pGetHeroLevel)A(0x3C7A10, 0x1E2870);	 // "GetHeroLevel"
	IsPlayerObserver = (pIsPlayerObserver)A(0x3CA140, 0x1E8170);	  // "IsPlayerObserver"
	GetOwningPlayer = (pGetOwningPlayer)A(0x3C8CD0, 0x1E3BA0);		  // "GetOwningPlayer"
	IsPlayerEnemy = (pIsPlayerEnemy)A(0x3CA0C0, 0x1E8090);			  // "IsPlayerEnemy"
	SetCameraField = (pSetCameraField)A(0x3B53F0, 0x1F4170);		  // "SetCameraField"
	IssueTargetOrderById = (pIssueTargetOrderById)A(0x3C9510, 0x1E96C0); // "IssueTargetOrderById"
	IsUnitOwnedByPlayer = (pIsUnitOwnedByPlayer)A(0x3C8570, 0x1E8B80);	 // "IsUnitOwnedByPlayer"
	IsUnitIllusion = (pIsUnitIllusion)A(0x3C8690, 0x1E88C0);			 // "IsUnitIllusion"

	// ---------------- 上层选中流程（非原生） ----------------
	// 1.24 sub_6F4256C0「Add local %s %x:%x for player %d」 -> 1.27 sub_6F26FF80（函数体逐行一致）
	SelectUnitReal = (pSelectUnitReal)A(0x4256C0, 0x26FF80);
	// 1.24 sub_6F425FD0 -> 1.27 sub_6F2735E0（SendAction 内紧随 sub_6F3A2190/sub_6F1C3330 之后调用）
	UpdatePlayerSelection = (pUpdatePlayerSelection)A(0x425FD0, 0x2735E0);
	// 1.24 sub_6F333240 -> 1.27 sub_6F3599F0（均为 GetInstance 后调用同构的 CGameUI 方法）
	CGameUIReset = (pCGameUIReset)A(0x333240, 0x3599F0);

	// ---------------- 全局单例 / 数据 ----------------
	W3XGlobalClass = A(0xACBDD8, 0xBE6350); // CGameUI 单例（均以 ".\\CGameUI.cpp" 6831 / 1108 字节创建）
	PrintToScreen = A(0x2F9980, 0x357640);	// 1.27 由 DisplayTextToPlayer(sub_6F1DFCE0) 调用，__thiscall(CGameUI*, pos, text, dur, -1)
	// 需要修正确认
	addrGetItemPtr = A(0x3BF690, 0x1CFC50); // 1.24 sub_6F3BF690 / 1.27 sub_6F1CFC50（GetItemTypeId 均经由它取 item）
	// 1.24 sub_6F33A890 -> sub_6F2CC5F0(CNetCommandUnitOrderTargetImage)
	// 1.27 sub_6F3AE810 -> sub_6F6A0060(CNetCommandUnitOrderTargetImage)
	addrSendActionNoNaked = A(0x33A890, 0x3AE810);
	// 1.24 sub_6F33A910 -> sub_6F2CC730(CNetCommandUnitOrderTargetImage)
	// 1.27 sub_6F3AE660 -> sub_6F69FEB0(CNetCommandUnitOrderTargetImage)
	addrSendActionNaked = A(0x33A910, 0x3AE660);
	// 1.24 sub_6F33A7A0 -> sub_6F2CC460(CNetCommandUnitOrderBasic)
	// 1.27 sub_6F3AE4E0 -> sub_6F69FFA0(CNetCommandUnitOrderBasic)
	addrSendActionNaked2 = A(0x33A7A0, 0x3AE4E0);
	addrGetItemState = A(0x421680, 0x276490); // 1.24 sub_6F421680 / 1.27 sub_6F276490（221 初值 + this[131] 遍历，逐行一致）
	addrGetStatePtr = A(0xACD44C, 0xBE4238);  // 游戏全局对象（1.24 dword_6FACD44C / 1.27 dword_6FBE4238）
	addrGetStateEcx = A(0x3A2190, 0x1C3330);  // 1.24 this[a2+22] / 1.27 同构（UnitItemInSlot、SendAction 一致调用）
	addrGetWidgetPtr = A(0x3BF0F0, 0x1D17D0); // 1.24 sub_6F3BF0F0 / 1.27 sub_6F1D17D0（IssueTargetOrderById 一致调用）
	addrLocalPlayerOffset = A(0xACD44C, 0xBE4238); // 同为上面的游戏全局对象
	addrGetUnitHandle1 = A(0x3A8BA0, 0x1C3200);	   // 1.24 sub_6F3A8BA0 / 1.27 sub_6F1C3200
	addrGetUnitHandle2 = A(0x4317C0, 0x2651D0);	   // 1.24 sub_6F4317C0 / 1.27 sub_6F2651D0
	addrUnitName1 = A(0x3BE7F0, 0x1D1550); // 单位句柄 -> CUnit*（GetUnitX/UnitItemInSlot 一致调用）
	addrUnitName2 = A(0x32E720, 0x326BA0); // CUnit+0x30 处字符串表取串（返回 "Default string" 的同构函数）
	addrGetPlayerName1 = A(0x3BE010, 0x1D03D0); // 玩家句柄 -> CPlayer*
	addrGetPlayerName2 = A(0x40BB30, 0x24A890); // CPlayer+0x24 名字串（函数体逐行一致）
	addrGlobalClass = A(0xACBDD8, 0xBE6350);		 // 同 CGameUI 单例
	addrGetUnitArrayPtr = A(0x39C220, 0x364A40); // 1.24 sub_6F39C220 / 1.27 sub_6F364A40（return this+384，逐行一致）
	addrUseItemNoLoc = A(0x33A7A0, 0x3AE4E0); // 同 addrSendActionNaked2
	UnitVtable = A(0x943A94, 0xA4A704); // 1.24 CUnit::`vftable'(0x6F943A94) / 1.27 CUnit::`vftable'(0x6FA4A704)
	addrMakeStringFunc = A(0x012040, 0x0506D0); // 1.24 sub_6F012040 / 1.27 sub_6F0506D0（RCString 构造）
}

HITEM MyUnitItemInSlot(HUNIT hUnit, int slot)
{
	return UnitItemInSlot(hUnit, slot);
}

int MyGetItemTypeId(HITEM hItem)
{
	return GetItemTypeId(hItem);
}

bool MyIsUnitOwnedByPlayer(HUNIT hUnit, HPLAYER hPlayer)
{
	return IsUnitOwnedByPlayer(hUnit, hPlayer);
}

bool MyIsUnitIllusion(HUNIT hUnit)
{
	return IsUnitIllusion(hUnit);
}

bool MyIsUnitHero(HUNIT hUnit)
{
	return GetHeroLevel(hUnit) > 0;
}

// 判断单位是否魔法免疫
bool MyIsUnitMagicImmune(HUNIT hUnit)
{
	if (!hUnit)
	{
		return false;
	}

	unsigned char *unit = nullptr;
	__asm
	{
		mov ecx, hUnit;
		call addrUnitName1; // 单位句柄 -> CUnit*（1.24 0x3BE7F0 / 1.27 0x1D1550）
		mov unit, eax;
	}

	if (!unit)
	{
		return false;
	}

	return *(int *)(unit + 0x15C) > 0;
}

DWORD __stdcall GetItemState(DWORD slotPos, DWORD opt, DWORD itemPtr)
{
	DWORD re = 0xD2;
	__asm
	{
		PUSH  itemPtr
		PUSH  opt
		PUSH  slotPos

		MOV   ECX, addrGetStatePtr
		MOV   ECX, DWORD PTR[ECX]
		MOVZX EAX, WORD PTR[ECX + 0x28]
		PUSH  EAX
		MOV   EAX, addrGetStateEcx
		CALL  EAX

		MOV   EBX, DWORD PTR[EAX + 0x34]
		MOV   ECX, EBX
		MOV   EAX, addrGetItemState
		CALL  EAX
		MOV   re, EAX
	}
	return re;
}

// 获取Item的指针
DWORD __stdcall GetItemPtr(HITEM hitem)
{
	DWORD re = 0;
	__asm
	{
		MOV   ECX, hitem
		MOV   EAX, addrGetItemPtr
		CALL  EAX
		MOV   re, EAX
	}
	return re;
}

void TargetOrderIssue(DWORD myhUnit, DWORD orderId, DWORD item, float x, float y, DWORD widget, DWORD option1, DWORD option2)
{
	__try
	{
		ClearSelection();
		MySelectUnit(myhUnit, true);
		__asm
		{
			PUSH  option2;
			PUSH  option1;
			MOV  ECX, widget;
			MOV  EAX, addrGetWidgetPtr;
			CALL EAX;
			PUSH EAX;
			MOV  EAX, y;
			PUSH EAX;
			MOV  EAX, x;
			PUSH EAX;
			MOV  ECX, item;
			MOV  EAX, addrGetItemPtr;
			CALL EAX;
			PUSH EAX;
			PUSH orderId;
			MOV  EAX, addrSendActionNaked;
			CALL EAX;
		}
	}
	__except (1)
	{
	}
}

void UseItemWithNoLocation(DWORD myhUnit, DWORD orderId, DWORD item)
{
	__try
	{
		ClearSelection();
		MySelectUnit(myhUnit, true);
		//MySelectUnitReal(myhUnit);
		__asm
		{
			PUSH 4;
			PUSH 0;
			MOV  ECX, item;
			MOV   EAX, addrGetItemPtr;
			CALL  EAX;
			PUSH EAX;
			PUSH orderId;
			MOV   EAX, addrUseItemNoLoc;
			CALL  EAX;
		}
	}
	__except (1)
	{
	}
}

char *MyGetUnitName(HUNIT hUnit)
{
	const char *retaddr = "Null";
	if (hUnit == NULL)
	{
		return (char *)retaddr;
	}
	__asm
	{
		mov ecx, hUnit;
		call addrUnitName1;
		test eax, eax;
		je NoUnit;

		mov ecx, [eax + 0x30];
		xor edx, edx;
		call addrUnitName2;
		mov retaddr, eax;
	NoUnit:
	}
	return (char *)retaddr;
}

bool MyIsItemUseable(HITEM hitem, DWORD i) // 物品可否使用 物品，物品栏位置0-5
{
	if (GetItemState(i + SLOT_INDEX_START, 0, GetItemPtr(hitem)) == 0)
	{
		return true;
	}
	return false;
}

bool MyIsSkillUseable(DWORD skill) // 技能可否使用
{
	if (GetItemState(skill, 1, 0) == 0)
	{
		return true;
	}
	return false;
}

bool MyUseItem(HUNIT myUnit, DWORD itemTypeId)
{
	for (int i = 0; i < 6; i++) // 遍历物品栏
	{
		DWORD tmpItem = MyUnitItemInSlot(myUnit, i);
		DWORD tmpTypeId = MyGetItemTypeId(tmpItem);

		if (tmpTypeId == itemTypeId) // 玄武
		{
			//MySelectUnitReal(myUnit);
			if (MyIsItemUseable(tmpItem, i))
			{
				spdlog::info("item useable");
				UseItemWithNoLocation(myUnit, i + SLOT_INDEX_START, tmpItem);
				return true;
			}
			else
			{
				spdlog::info("item unuseable");
				return false;
			}
		}
	}
	return false;
}

// slotoffset  0-5
DWORD GetUnitSlotItemID(HUNIT myUnit, DWORD slotoffset)
{
	DWORD tmpItem = MyUnitItemInSlot(myUnit, slotoffset);
	if (tmpItem)
	{
		return MyGetItemTypeId(tmpItem);
	}
	return 0;
}

bool MyUseItemTarget(HUNIT myUnit, DWORD itemTypeId, HUNIT target)
{
	for (int i = 0; i < 6; i++) // 遍历物品栏
	{
		DWORD tmpItem = MyUnitItemInSlot(myUnit, i);
		DWORD tmpTypeId = MyGetItemTypeId(tmpItem);
		if (tmpTypeId == itemTypeId)
		{
			if (MyIsItemUseable(tmpItem, i))
			{
				TargetOrderIssue(myUnit, i + SLOT_INDEX_START, tmpItem, MyGetUnitX(target), MyGetUnitY(target), target, 0, 4);
				return true;
			}
			else
			{
				return false;
			}
		}
	}
	return false;
}

bool MyUseItemLoc(HUNIT myUnit, DWORD itemTypeId, float x, float y)
{
	for (int i = 0; i < 6; i++) // 遍历物品栏
	{
		DWORD tmpItem = MyUnitItemInSlot(myUnit, i);
		DWORD tmpTypeId = MyGetItemTypeId(tmpItem);
		if (tmpTypeId == itemTypeId)
		{
			if (MyIsItemUseable(tmpItem, i))
			{
				TargetOrderIssue(myUnit, i + SLOT_INDEX_START, tmpItem, x, y, 0, 0, 4);
				return true;
			}
			else
			{
				return false;
			}
		}
	}
	return false;
}

void MySelectUnit(HUNIT hUnit, bool flag)
{
	SelectUnit(hUnit, flag);
}

int IsNotBadUnit(unsigned char* unitaddr, int onlymem)
{
	if (unitaddr)
	{
		int xaddraddr = (int)&UnitVtable;// 1.24: 6F943A94, 1.27: 6FA4A704
		// CUnit::`vftable' 首地址比对（CUnit 布局 1.24/1.27 一致）
		if (*(unsigned char*)xaddraddr != *(unsigned char*)unitaddr)
			return FALSE;
		else if (*(unsigned char*)(xaddraddr + 1) != *(unsigned char*)(unitaddr + 1))
			return FALSE;
		else if (*(unsigned char*)(xaddraddr + 2) != *(unsigned char*)(unitaddr + 2))
			return FALSE;
		else if (*(unsigned char*)(xaddraddr + 3) != *(unsigned char*)(unitaddr + 3))
			return FALSE;

		unsigned int x1 = *(unsigned int*)(unitaddr + 0xC);
		unsigned int y1 = *(unsigned int*)(unitaddr + 0x10);

		int udata = *(int*)(unitaddr + 0x28);


		if (x1 == 0xFFFFFFFF || y1 == 0xFFFFFFFF || udata == 0)
		{
			spdlog::info("Coordinates or 0x28 offset bad");
			return FALSE;
		}

		if (onlymem)
			return TRUE;

		unsigned int unitflag = *(unsigned int*)(unitaddr + 0x20);
		unsigned int unitflag2 = *(unsigned int*)(unitaddr + 0x5C);

		if (unitflag & 1u)
		{
			spdlog::info("Flag 1 bad");
			return FALSE;
		}

		if (!(unitflag & 2u))
		{
			spdlog::info("Flag 2 bad");
			return FALSE;
		}

		if ((unitflag2 & 0x100u) > 0)
		{
			spdlog::info("Flag 3 bad");
			return FALSE;
		}

		return TRUE;
	}

	spdlog::info("FATAL ERROR. NO UNIT ADDRESS FOUND");
	return FALSE;
}

//sub_6F4256C0 (1.24) / sub_6F26FF80 (1.27)
void MySelectUnitReal(HUNIT unit)
{
	if (SelectUnitReal && UpdatePlayerSelection && IsNotBadUnit((unsigned char*)unit, 0))
	{
		int localPlayer = GetLocalPlayer();
		int playerslot = GetPlayerId(localPlayer);
		int playerseldata = *(int*)(localPlayer + 0x34);
		SelectUnitReal(playerseldata, unit, playerslot, 0, 1, 1);
		UpdatePlayerSelection((int)playerseldata, 0);
		CGameUIReset(0);
	}
}

float MyGetUnitX(HUNIT hUnit)
{
	return GetUnitX(hUnit).fl;
}

float MyGetUnitY(HUNIT hUnit)
{
	return GetUnitY(hUnit).fl;
}

Location MyGetUnitFaceLoc(HUNIT hUnit, float dis)
{
	float unitFacing = (float)deg2rad(MyGetUnitFacing(hUnit));
	Location ret;
	ret.X = MyGetUnitX(hUnit) + std::cos(unitFacing) * dis;
	ret.Y = MyGetUnitY(hUnit) + std::sin(unitFacing) * dis;
	return ret;
}

int MyGetUnitTypeId(HUNIT hUnit)
{
	return GetUnitTypeId(hUnit);
}

HUNIT MyGetUnitByOffset(DWORD addr)
{
	DWORD hand;
	_asm
	{
		pushad;
		mov esi, addr;
		mov ecx, addrLocalPlayerOffset;
		mov ecx, [ecx];
		call addrGetUnitHandle1;
		push 0;
		push esi;
		mov ecx, eax;
		call addrGetUnitHandle2;
		mov hand, eax;
		popad;
	}
	return hand;
}

void MyUseSkill(DWORD myhUnit, DWORD cmdId)
{
	__try
	{
		ClearSelection();
		MySelectUnit(myhUnit, true);
		//MySelectUnitReal(myhUnit);
		__asm
		{
			PUSH 4;
			PUSH 0;
			PUSH 0;
			PUSH cmdId;
			MOV   EAX, addrUseItemNoLoc;
			CALL  EAX;
		}
	}
	__except (1)
	{
	}
}

void MyUseSkillTarget(DWORD myhUnit, DWORD cmdId, DWORD target)
{
	__try
	{
		MySelectUnit(myhUnit, true);
		__asm
		{
			PUSH 0;
			PUSH 4;
			PUSH target;
			PUSH 0;
			PUSH cmdId;
			ADDR(W3XGlobalClass, ECX);
			MOV ECX, DWORD PTR DS : [ECX + 0x1B4] ;
			CALL addrSendActionNoNaked;
		}
	}
	__except (1)
	{
	}
}

void MyUseSkillLoc(DWORD myhUnit, DWORD cmdId, float X, float Y)
{
	__try
	{
		MySelectUnit(myhUnit, true);
		__asm
		{
			PUSH 0;
			PUSH 6;
			PUSH 0;
			ADDR(W3XGlobalClass, ECX);
			MOV ECX, DWORD PTR DS : [ECX + 0x1B4] ;
			PUSH Y;
			PUSH X;
			PUSH 0;
			PUSH cmdId;
			CALL addrSendActionNaked;
		}
	}
	__except (1)
	{
		spdlog::error("MyUseSkillLoc error");
	}
}

// 无需目标的技能
void MyUseSkillEx(DWORD myhUnit, DWORD cmdId, bool needspell, bool cstatus)
{
	DWORD flag = needspell ? 0x300001 : 0x200001;
	DWORD flag2 = cstatus ? 6 : 4;
	__try
	{
		ClearSelection();
		MySelectUnit(myhUnit, true);
		//MySelectUnitReal(myhUnit);
		__asm
		{
			PUSH flag2;
			PUSH flag; // 300001
			ADDR(W3XGlobalClass, ECX);
			MOV ECX, DWORD PTR DS : [ECX + 0x1B4] ;
			PUSH 0;
			PUSH cmdId;
			CALL addrSendActionNaked2;
		}
	}
	__except (1)
	{
		spdlog::error("MyUseSkillEx error");
	}
}

bool MyIssueTargetOrderById(DWORD myhUnit, DWORD cmdId, DWORD target)
{
	// 1.24 sub_6F3C9510 / 1.27 sub_6F1E96C0
	return IssueTargetOrderById(myhUnit, cmdId, target);
}

void GetJassString(char *szString, CJassString *String)
{
	strcpy_s(tmpStr, sizeof(tmpStr), szString);
	__asm
	{
		PUSH szString;
		MOV ECX, String;
		CALL addrMakeStringFunc;
	}
}

HPLAYER MyGetLocalPlayer()
{
	auto p = GetLocalPlayer();
	// spdlog::info("MyGetLocalPlayer p = 0x{:08X}", p);
	return p;
}

int MyGetLocalPlayerID()
{
	return GetPlayerId(GetLocalPlayer());
}

void MySetPlayerName(HPLAYER hPlayer, const char *szName)
{
	CJassString JSTest;
	GetJassString((char *)szName, &JSTest);
	SetPlayerName((LPVOID)hPlayer, (DWORD *)&JSTest);
}

const char *MyGetPlayerName(HPLAYER hPlayer)
{
	const char *retaddr = "null";

	__asm
	{
		mov ecx, hPlayer;
		call addrGetPlayerName1;
		test eax, eax;
		jz NOPLAYER;
		push 1;
		mov ecx, eax;
		call addrGetPlayerName2;
	NOPLAYER:
		mov retaddr, eax;
	}

	return retaddr;
}

float MyGetUnitFacing(HUNIT hUnit)
{
	return GetUnitFacing(hUnit).fl;
}

bool MyIsUnitEnemy(HUNIT hUnit, HPLAYER hPlayer)
{
	return IsUnitEnemy(hUnit, hPlayer);
}

void MySetCameraField(DWORD field, float *v, float *dur)
{
	SetCameraField(field, v, dur);
}

float MyGetDistance(float x1, float y1, float x2, float y2)
{
	return (std::sqrt(std::pow(x1 - x2, 2) + std::pow(y1 - y2, 2)));
}

// AOE
bool MyIsCanHurtMe(float area, float x, float y)
{
	if (LocalHero)
	{
		float myX = MyGetUnitX(LocalHero);
		float myY = MyGetUnitY(LocalHero);
		float distance = MyGetDistance(myX, myY, x, y);
		// spdlog::info("MyIsCanHurtMe myX = {}, myY = {}", myX, myY);
		// spdlog::info("MyIsCanHurtMe distance = {}, area = {}", distance, area);
		if (distance < area)
		{
			// spdlog::info("IsCanHurtMe ==> yes");
			return true;
		}
	}
	return false;
}

int MyGetPlayerId(HPLAYER hPlayer)
{
	return GetPlayerId(hPlayer);
}

HPLAYER MyPlayer(int Index)
{
	return Player(Index);
}

bool MyIsPlayerObserver(HPLAYER hPlayer)
{
	return IsPlayerObserver(hPlayer) != FALSE;
}

HPLAYER MyGetOwnerPlayer(HUNIT unit)
{
	// 1.24 sub_6F3C8CD0 / 1.27 sub_6F1E3BA0
	return GetOwningPlayer(unit);
}

int GetUnitOwnerSlot(unsigned char* unitaddr)
{
	if (unitaddr)
		return *(int*)(unitaddr + 88); // CUnit+0x58，1.24/1.27 一致
	return 15;
}

bool MyIsPlayerEnemy(HPLAYER hPlayer1, HPLAYER hPlayer2)
{
	return IsPlayerEnemy(hPlayer1, hPlayer2) != FALSE;
}

int IsEnemy(unsigned char * UnitAddr)
{
	if (UnitAddr && IsNotBadUnit(UnitAddr))
	{
		int unitownerslot = GetUnitOwnerSlot(UnitAddr);
		return MyIsPlayerEnemy(MyGetLocalPlayer(), Player(unitownerslot));
	}
	return FALSE;
}

template <typename... Args>
inline void TextPrintEx(float fDuration, std::format_string<Args...> fmt, Args &&...args)
{
	TextPrint(std::format(fmt, args...).c_str(), fDuration);
}

void TextPrint(const char *szText, float fDuration)
{
	DWORD dwDuration = *((DWORD *)&fDuration);
	__asm
	{
		PUSH	0xFFFFFFFF;
		PUSH	dwDuration;
		PUSH	szText;
		PUSH	0x0;
		PUSH	0x0;
		MOV		ECX, [W3XGlobalClass];
		MOV		ECX, [ECX];
		CALL	PrintToScreen;
	}
}

DWORD JGetUnitArray(DWORD &Sz)
{
	__asm
	{
		MOV EAX, DWORD PTR DS : [addrGlobalClass];
		MOV EAX, DWORD PTR DS : [EAX];
		MOV EAX, DWORD PTR DS : [EAX + 0x3BC];
		PUSH 0; // if 0 here it will just return the pointer if 1 it will update the array
		MOV ECX, EAX;
		CALL addrGetUnitArrayPtr;
		MOV ECX, DWORD PTR DS : [EAX + 4];
		MOV EDX, DWORD PTR DS : [Sz];
		MOV DWORD PTR DS : [EDX] , ECX;
		MOV EAX, DWORD PTR DS : [EAX + 8];
	}
}

// 1.24 sub_6F301250 / 1.27 sub_6F34F3A0：CGameUI::GetInstance（1.27 由 helper 已可用）
// 1.24 sub_6F2F9980 已被 PrintToScreen 取代
