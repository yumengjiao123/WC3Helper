#pragma once
#include <Windows.h>

typedef DWORD HPLAYER;
typedef DWORD HPLAYERSTATE;
typedef DWORD HUNIT;
typedef DWORD HUNITSTATE;
typedef DWORD HUNITTYPE;
typedef DWORD HITEM;
typedef DWORD HTEXTTAG;
typedef DWORD HBUTTON;

#define SLOT_INDEX_START 0xD0028 // 852008

#define M_PI 3.141592653589793238462643
#define deg2rad(a) ((a) * M_PI / 180)

#define ADDR(X, REG) \
	__asm MOV REG, DWORD PTR DS : [X] __asm MOV REG, DWORD PTR DS : [REG]

struct CJassStringData
{
	DWORD vtable;
	DWORD refCount;
	DWORD dwUnk1;
	DWORD pUnk2;
	DWORD pUnk3;
	DWORD pUnk4;
	DWORD pUnk5;
	char *data;
};

struct CJassString
{
	DWORD vtable;
	DWORD dw0;
	CJassStringData *data;
	DWORD dw1;
};

struct Unit
{
	DWORD dwDummy[3];
	DWORD dwID1;
	DWORD dwID2;
	BYTE _1[0x1C];
	DWORD dwClassId;
	BYTE _2[0x1C];
	DWORD HealthBar;
	DWORD UNK;
	DWORD dwOwnerSlot;
};

struct Location
{
	float X;
	float Y;
	float Z;
};

union Float
{
	float fl;
	DWORD dw;
};

HITEM MyUnitItemInSlot(HUNIT hUnit, int slot);

HUNIT MyGetUnitByOffset(DWORD addr);
void MyUseSkill(DWORD myhUnit, DWORD cmdId);
void MyUseSkillEx(DWORD myhUnit, DWORD cmdId, bool needspell, bool cstatus);
void MyUseSkillTarget(DWORD myhUnit, DWORD cmdId, DWORD target);
void MyUseSkillLoc(DWORD myhUnit, DWORD cmdId, float X, float Y);

bool MyIssueTargetOrderById(DWORD myhUnit, DWORD cmdId, DWORD target);

bool MyUseItem(HUNIT myUnit, DWORD itemTypeId);
bool MyUseItemTarget(HUNIT myUnit, DWORD itemTypeId, HUNIT target);
bool MyUseItemLoc(HUNIT myUnit, DWORD itemTypeId, float x, float y);

void MySelectUnit(HUNIT hUnit, bool flag);
void MySelectUnitReal(HUNIT unit);
int MyGetUnitTypeId(HUNIT hUnit);

HPLAYER MyGetLocalPlayer();
int MyGetLocalPlayerID();

void MySetPlayerName(HPLAYER hPlayer, const char *szName);
const char* MyGetPlayerName(HPLAYER hPlayer);

Location MyGetUnitFaceLoc(HUNIT hUnit, float dis);

bool MyIsUnitEnemy(HUNIT hUnit, HPLAYER hPlayer);

float MyGetUnitX(HUNIT hUnit);
float MyGetUnitY(HUNIT hUnit);
float MyGetUnitFacing(HUNIT hUnit);

int MyGetPlayerId(HPLAYER hPlayer);
HPLAYER MyPlayer(int Index);
bool MyIsPlayerObserver(HPLAYER hPlayer);

int IsNotBadUnit(unsigned char* unitaddr, int onlymem = 0);
int IsEnemy(unsigned char * UnitAddr);
int GetUnitOwnerSlot(unsigned char* unitaddr);

void MySetCameraField(DWORD field, float *v, float *dur);

bool MyIsUnitOwnedByPlayer(HUNIT hUnit, HPLAYER hPlayer);
bool MyIsUnitIllusion(HUNIT hUnit);
bool MyIsUnitHero(HUNIT hUnit);

DWORD __stdcall GetItemState(DWORD slotPos, DWORD opt, DWORD itemPtr);
DWORD __stdcall GetItemPtr(HITEM hitem);
void UseItemWithNoLocation(DWORD myhUnit, DWORD orderId, DWORD item);
void GetJassString(char *szString, CJassString *String);
float MyGetDistance(float x1, float y1, float x2, float y2);
bool MyIsCanHurtMe(float area, float x, float y);
char *MyGetUnitName(HUNIT hUnit);

DWORD JGetUnitArray(DWORD &Sz);
DWORD GetUnitSlotItemID(HUNIT myUnit, DWORD slotoffset);
void TextPrint(const char *szText, float fDuration = 3.0f);

void initJASS();
