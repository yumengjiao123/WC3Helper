#pragma once
#ifdef WC3HELPER_LIBRARY
#define MYAPI __declspec(dllexport)
#else
#define MYAPI __declspec(dllimport)
#endif

void UpdateWideScreen(LPVOID gameDllBase);
HWND GetGameWindow();
