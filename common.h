#pragma once
#include <Windows.h>

enum class Version : DWORD
{
	unknown = 0,
	v124b = 6374,
	v124e = 6387,
	v126a = 6401,
	v127a = 52240,
	v127b = 7085,
};

Version GetWar3Version();

// 通用字符字形缓存，tex 用 void* 存储，兼容 OpenGL(GLuint)/D3D8/D3D9 纹理
struct CharGlyph
{
	void *tex;        // 多后端共用：OpenGL 存储为 (void*)(uintptr_t)GLuint，D3D 存为纹理指针
	int w, h;
	int bearingX;
	int bearingY;
	int advance;
	float uMax, vMax;
	CharGlyph() : tex(nullptr), w(0), h(0), bearingX(0), bearingY(0), advance(0), uMax(0.0f), vMax(0.0f) {}
};
