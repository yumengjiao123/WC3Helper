#pragma once
#include <d3d9.h>
#include "common.h"

// D3D9 版本的文字绘制
void DrawTextD3D9(float startX, float startY, DWORD color, const wchar_t *text, bool bCenter, int fontSize);
