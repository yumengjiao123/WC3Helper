#pragma once
#ifdef WC3HELPER_LIBRARY
#define MYAPI __declspec(dllexport)
#else
#define MYAPI __declspec(dllimport)
#endif

void LoadWFE();
void UnloadWFE();
void HideDll(HMODULE hModule);
