#pragma once
#ifdef WC3HELPER_LIBRARY

#define MYAPI __declspec(dllexport)
#else
#define MYAPI __declspec(dllimport)
#endif

MYAPI void DreamUiInit(LPVOID, LPVOID, bool);
MYAPI void GetHpRegen();
MYAPI void GetMpRegen();
MYAPI void AsToInt();
MYAPI void MsToInt();
MYAPI void ShowText();
