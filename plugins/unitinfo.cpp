#include <windows.h>
#include "unitinfo.h"

DWORD Storm503 = 0;
DWORD Storm578 = 0;
DWORD GetRegen = (DWORD)0xAB82C8;//0xAB7788

const char *AsFormat   = "%0.02f";
const char *MsFormat   = "%.0f";
const char *HpMpFormat = "%s\nAA\tBB\nCC\tDD\n|CFFF9D112HP/s:|r |CFF00FF00%.02f|r\n|CFFF9D112MP/s:|r |CFFC3DBFF%.02f|r";

float HpRegen=0.00f, MpRegen=0.00f;

DWORD dwGameDll = 0;

void DreamUiInit(LPVOID gameDllBase, LPVOID hMod, bool ShowMana)
{
	DWORD dwOldProtection = NULL;

	Storm503 = (DWORD)gameDllBase + 0x6EBD5E;//1.26 0x6EB5BE
	Storm578 = (DWORD)gameDllBase + 0x6EBD46;//1.26 0x6EB5A6

	dwGameDll = (DWORD)gameDllBase;

	// Get AS & MS in Number
	//if (SettingGet("Show AS & MS in Number"))
	{
		VirtualProtect((LPVOID)(dwGameDll + 0x339000), 534000, PAGE_EXECUTE_READWRITE, &dwOldProtection);
		// *(DWORD *) (dwGameDll + 0x339C5C) = (DWORD)MsToInt - (dwGameDll + 0x339C60);//85 DB 5B 74 0C 57 68 68
		// *(DWORD *) (dwGameDll + 0x339DFC) = (DWORD)AsToInt - (dwGameDll + 0x339E00);
		VirtualProtect((LPVOID)(dwGameDll + 0x339000), 534000, dwOldProtection, NULL);
	}

	// Show HP & MP Regen
	//if (SettingGet("Show HP & MP Regen"))
	if(false)
	{
		// VirtualProtect((LPVOID)(dwGameDll + 0x358000), 515000, PAGE_EXECUTE_READWRITE, &dwOldProtection);
		// *(BYTE *)(dwGameDll + 0x358C77) = 0xE8;
		// *(DWORD *) (dwGameDll + 0x358C78) = (DWORD)GetHpRegen - (dwGameDll + 0x358C7C);//00 00 50 8B 01 8B 80 30 01 00
		// *(BYTE *)(dwGameDll + 0x358C7C) = 0x90;
		// *(BYTE *)(dwGameDll + 0x358C7D) = 0x90;
		// *(BYTE *)(dwGameDll + 0x358322) = 0xE8;
		// *(DWORD *) (dwGameDll + 0x358323) = (DWORD)GetMpRegen - (dwGameDll + 0x358327);//00 E8 73 F0 11 00 8B 44 24 14
		// *(BYTE *)(dwGameDll + 0x358327) = 0x90;
		// VirtualProtect((LPVOID)(dwGameDll + 0x358000), 515000, dwOldProtection, NULL);
 
		VirtualProtect((LPVOID)(dwGameDll + 0x35564C), 6, PAGE_EXECUTE_READWRITE, &dwOldProtection);
		*(BYTE *)(dwGameDll + 0x35564C) = 0xE8;
		*(DWORD *) (dwGameDll + 0x35564D) = (DWORD)ShowText - (dwGameDll + 0x355651);//0x354B11 //00 8D 74 24 28 E8 45 1B FF FF
		*(BYTE *)(dwGameDll + 0x355651) = 0x90;
		VirtualProtect((LPVOID)(dwGameDll + 0x35564C), 6, dwOldProtection, NULL);
	}
}

void __declspec(naked) GetHpRegen()
{
	_asm
	{
		LEA  EAX, [ESP+0x0D8]

		PUSH EAX
		PUSH ECX
		PUSH ESI

		ADD  ECX, 0x98
		MOV  ECX, DWORD PTR DS:[ECX+8]
		MOV  ESI, dwGameDll
		MOV  EAX, GetRegen
		MOV  ESI, DWORD PTR DS:[ESI+EAX]
		MOV  EAX, DWORD PTR DS:[ESI+0x0C]
		MOV  ECX, DWORD PTR DS:[ECX*8+EAX+4]
		MOV  ECX, DWORD PTR DS:[ECX+0x7C]
		MOV  HpRegen, ECX

		POP  ESI
		POP  ECX
		POP  EAX

		RETN
	}
}

void __declspec(naked) GetMpRegen()
{
	_asm
	{
		ADD  ECX, 0x0B8

		PUSH EAX
		PUSH ECX
		PUSH ESI

		MOV  ECX, DWORD PTR DS:[ECX+8]
		MOV  ESI, dwGameDll
		MOV  EAX, GetRegen
		MOV  ESI, DWORD PTR DS:[ESI+EAX]
		MOV  EAX, DWORD PTR DS:[ESI+0x0C]
		MOV  ECX, DWORD PTR DS:[ECX*8+EAX+4]
		MOV  ECX, DWORD PTR DS:[ECX+0x7C]
		MOV  MpRegen, ECX

		POP  ESI
		POP  ECX
		POP  EAX

		RETN
	}
}

void __declspec(naked) AsToInt()
{
	DWORD dwAsToIntBuffer;

	_asm
	{
		MOV  EAX, DWORD PTR DS:[ESP]
		MOV  dwAsToIntBuffer, EAX

		FLD  DWORD PTR SS:[ESP+0x6C]
		SUB  ESP, 8
		FSTP QWORD PTR SS:[ESP]
		PUSH AsFormat
		PUSH 7
		PUSH ECX
		CALL Storm578

		ADD  ESP, 0x18

		CALL Storm503
		PUSH dwAsToIntBuffer

		RETN
	}
}

void __declspec(naked) MsToInt()
{
	DWORD dwMsToIntBuffer;

	_asm
	{
		MOV  EAX, DWORD PTR DS:[ESP]
		MOV  dwMsToIntBuffer, EAX

		FLD  DWORD PTR SS:[ESP+0x90]
		SUB  ESP, 8
		FSTP QWORD PTR SS:[ESP]
		PUSH MsFormat
		PUSH 7
		PUSH ECX
		CALL Storm578

		ADD  ESP, 0x18

		CALL Storm503
		PUSH dwMsToIntBuffer

		RETN
	}
}

void __declspec(naked) ShowText()
{
	_asm
	{
		PUSH ECX
		LEA  ECX, [ESP+0x30]
		FLD  MpRegen
		FLD  HpRegen
		SUB  ESP, 0x10
		FSTP QWORD PTR SS:[ESP]
		FSTP QWORD PTR SS:[ESP+8]
		PUSH ECX
		PUSH HpMpFormat
		PUSH 0x80
		PUSH ECX
		CALL Storm578;
		ADD  ESP, 0x20
		POP  ECX
		MOV  EDI, [EBP+0x134]
		RETN
	}
}
