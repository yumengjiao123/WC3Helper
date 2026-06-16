#include "jass.h"
#include <cmath>

#include "spdlog/spdlog.h"

extern LPVOID g_gameDllBase;

typedef HITEM(__cdecl *pUnitItemInSlot)(HUNIT hUnit, int slot);
typedef int(__cdecl *pGetItemTypeId)(HITEM hItem);
typedef void(__cdecl *pSelectUnit)(HUNIT hUnit, bool flag);
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

pUnitItemInSlot UnitItemInSlot = nullptr;
pGetItemTypeId GetItemTypeId = nullptr;
pSelectUnit SelectUnit = nullptr;
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

extern DWORD LocalHero;

char tmpStr[100] = {0};

void initJASS()
{
	UnitItemInSlot = (pUnitItemInSlot)((DWORD)g_gameDllBase + 0x3C8270);
	GetItemTypeId = (pGetItemTypeId)((DWORD)g_gameDllBase + 0x3C57A0);
	SelectUnit = (pSelectUnit)((DWORD)g_gameDllBase + 0x3C8450);
	GetUnitTypeId = (pGetUnitTypeId)((DWORD)g_gameDllBase + 0x3C6450);
	GetLocalPlayer = (pGetLocalPlayer)((DWORD)g_gameDllBase + 0x3BC6A0);
	SetPlayerName = (pSetPlayerNameFunc)((DWORD)g_gameDllBase + 0x3C1A50);
	GetPlayerName = (pGetPlayerNameFunc)((DWORD)g_gameDllBase + 0x3C1AA0);
	GetUnitX = (pGetUnitX)((DWORD)g_gameDllBase + 0x3C6050);
	GetUnitY = (pGetUnitY)((DWORD)g_gameDllBase + 0x3C6090);
	GetUnitFacing = (pGetUnitFacing)((DWORD)g_gameDllBase + 0x3C62D0);
	IsUnitEnemy = (pIsUnitEnemy)((DWORD)g_gameDllBase + 0x3C8610);
	GetPlayerId = (pGetPlayerId)((DWORD)g_gameDllBase + 0x3CA180);
	Player = (pPlayer)((DWORD)g_gameDllBase + 0x3BC670);
	GetHeroLevel = (pGetHeroLevel)((DWORD)g_gameDllBase + 0x3C7A10);

	IsPlayerObserver = (pIsPlayerObserver)((DWORD)g_gameDllBase + 0x3CA140);
	GetOwningPlayer = (pGetOwningPlayer)((DWORD)g_gameDllBase + 0x3C8CD0);
	IsPlayerEnemy = (pIsPlayerEnemy)((DWORD)g_gameDllBase + 0x3CA0C0);

	SetCameraField = (pSetCameraField)((DWORD)g_gameDllBase + 0x3B53F0);

	SelectUnitReal = (pSelectUnitReal)((DWORD)g_gameDllBase + 0x4256C0);
	UpdatePlayerSelection = (pUpdatePlayerSelection)((DWORD)g_gameDllBase + 0x425FD0);
	
	IssueTargetOrderById = (pIssueTargetOrderById)((DWORD)g_gameDllBase + 0x3C9510);

	IsUnitOwnedByPlayer = (pIsUnitOwnedByPlayer)((DWORD)g_gameDllBase + 0x3C8570);
	IsUnitIllusion = (pIsUnitIllusion)((DWORD)g_gameDllBase + 0x3C8690);

	W3XGlobalClass = (DWORD)g_gameDllBase + 0xACBDD8; // 1.24b和1.24e一样
	PrintToScreen = (DWORD)g_gameDllBase + 0x2F9980;  // 1.24b和1.24e一样
	// 需要修正确认
	addrGetItemPtr = (DWORD)g_gameDllBase + 0x3BF690;		 // 6F3BF570, 这里发现了问题
	addrSendActionNoNaked = (DWORD)g_gameDllBase + 0x33A890; // 换成0x33AA40试试//0x33A890; // 6F33A890   1.24b 0x33A7D0
	addrSendActionNaked = (DWORD)g_gameDllBase + 0x33A910;	 // 6F33A910   1.24b 0x33A850
	addrSendActionNaked2 = (DWORD)g_gameDllBase + 0x33A7A0;
	addrGetItemState = (DWORD)g_gameDllBase + 0x421680;		 // 6F421680
	addrGetStatePtr = (DWORD)g_gameDllBase + 0xACD44C;		 // 1.24b和1.24e一样
	addrGetStateEcx = (DWORD)g_gameDllBase + 0x3A2190;		 // 6F3A2190
	addrGetWidgetPtr = (DWORD)g_gameDllBase + 0x3BF0F0;		 // 6F3BF0F0
	addrLocalPlayerOffset = (DWORD)g_gameDllBase + 0xACD44C; // 1.24b和1.24e一样
	addrGetUnitHandle1 = (DWORD)g_gameDllBase + 0x3A8BA0;	 // 6F3A8BA0
	addrGetUnitHandle2 = (DWORD)g_gameDllBase + 0x4317C0;	 // 6F4317C0

	addrUnitName1 = (DWORD)g_gameDllBase + 0x3BE7F0; // 6F3BE7F0
	addrUnitName2 = (DWORD)g_gameDllBase + 0x32E720; // 6F32E720

	addrGetPlayerName1 = (DWORD)g_gameDllBase + 0x3BE010;
	addrGetPlayerName2 = (DWORD)g_gameDllBase + 0x40BB30;

	addrGlobalClass = (DWORD)g_gameDllBase + 0xACBDD8;	   // 1.24b和1.24e一样
	addrGetUnitArrayPtr = (DWORD)g_gameDllBase + 0x39C220; // //1.24e 6F39C220

	addrUseItemNoLoc = (DWORD)g_gameDllBase + 0x33A7A0; // 6F33A7A0

	UnitVtable = (DWORD)g_gameDllBase + 0x943A94;
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
		//MySelectUnit(myhUnit, true);
		MySelectUnitReal(myhUnit);
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
			// if (true || MyIsItemUseable(tmpItem, i))
			MySelectUnitReal(myUnit);
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
		int xaddraddr = (int)&UnitVtable;//6F943A94

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

int(__thiscall* sub_6F333240)(void* a1);
//sub_6F4256C0
void MySelectUnitReal(HUNIT unit)
{
	if (SelectUnitReal && UpdatePlayerSelection && IsNotBadUnit((unsigned char*)unit,0))
	{
		sub_6F333240 = (int(__thiscall*)(void* a1))((DWORD)g_gameDllBase + 0x333240);
		int localPlayer = GetLocalPlayer();
		int playerslot = GetPlayerId(localPlayer);
		int playerseldata = *(int*)(localPlayer + 0x34);
		SelectUnitReal(playerseldata, unit, playerslot, 0, 1, 1);
		UpdatePlayerSelection((int)playerseldata, 0);
		sub_6F333240(0);
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
	UseItemWithNoLocation(myhUnit, cmdId, 0);
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
		MySelectUnit(myhUnit, true);
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
	// sub_6F3C9510
	return IssueTargetOrderById(myhUnit, cmdId, target);
}

void GetJassString(char *szString, CJassString *String)
{
	DWORD addrMakeStringFunc = (DWORD)g_gameDllBase + 0x012040;
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
	//sub_6F3C8CD0
	return GetOwningPlayer(unit);
}

int GetUnitOwnerSlot(unsigned char* unitaddr)
{
	if (unitaddr)
		return *(int*)(unitaddr + 88);
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

// sub_6F301250
// sub_6F2F9980
// sub_10022C30
