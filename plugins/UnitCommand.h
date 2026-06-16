#pragma once
#include <Windows.h>
#include <string>
#include <jass.h>

#define ITEM_XX 0x64657363 // 翔靴
#define ITEM_XW 0x49303243 // 玄武 0x49303243
#define ITEM_FZ 0x736E6567 // 风暴之杖
#define ITEM_YZ 0x7373616E // 巫术魔杖 0x7373616E

#define HERO_ID_ZG 0x48303038 // 诸葛
#define HERO_ID_ZF 0x4F303032 // 张飞
#define HERO_ID_GY 0x4F303033 // 关羽
#define HERO_ID_ZY 0x45303030 // 赵云
#define HERO_ID_WY 0x48303034 // 魏延

#define HERO_ID_DW 0x48303036  // 典韦 0x48303036
#define HERO_ID_CR 0x55303037  // 曹仁 0x55303037
#define HERO_ID_XUN 0x55303049 // 荀彧
#define HERO_ID_GJ 0x55303030  // 郭嘉
#define HERO_ID_SM 0x55303041  // 司马

struct Command
{
	DWORD _6F942254;
	DWORD IDUNCARE[8];
	DWORD CommandId;
	DWORD UNK[8];
	float X;
	DWORD UNK1;
	float Y;
};

enum Cmd
{
	MOVE = 0xD0003,	  // 移动
	ATTACK = 0xD000F, // 攻击
	HOLD = 0xD0019,	  // 停止 H
	STOP = 0xD0004,	  // 停止 S


	ITEM1 = SLOT_INDEX_START,	  // 物品1
	ITEM2 = SLOT_INDEX_START + 1, // 物品2
	ITEM3 = SLOT_INDEX_START + 2, // 物品3
	ITEM4 = SLOT_INDEX_START + 3, // 物品4
	ITEM5 = SLOT_INDEX_START + 4, // 物品5
	ITEM6 = SLOT_INDEX_START + 5, // 物品6
	
	Action_T = 0xD009F,			  // 张飞典韦T
	Action_41_E = 0xD024B,		  // 司马 焰击波
	Action_41_N = 0xD0108,		  // 司马 星落 852232
	Action_CR_E = 0xD0103,		  // 曹仁 睡
	Action_CR_G = 0xD0270,		  // 曹仁 鬼神
	Action_XUN_W = 0xD0218,		  // 荀彧 蛇形结界
	Action_GJ_C = 0xD024B,		  // 郭嘉 冰河爆裂破
	Action_GJ_F = 0xD0101,		  // 郭嘉 寒冰甲 852225
	Action_DW_R = 0xD00B5,		  // 典韦 妖火 852149

	Action_ZG_E = 0xD0270, // 诸葛 极冻凝结
	Action_ZG_X = 0xD009F, // 诸葛 卧龙光线
	Action_ZF_V = 0xD0076, // 张飞 万夫莫敌
	Action_GY_X = 0xD0107, // 关羽 X字斩
	Action_GY_D = 0xD0079, // 关羽 D
	Action_GY_B = 0xD00A0, // 关羽 B 关羽开旋风斩
	Action_ZY_B = 0xD0251, // 赵云开无双
	Action_WY_R = 0xD009B, // 魏延 分身 852123

	Action_XX = 0xD022D, // 翔靴 852525
};

// 真三技能
void ZSGWS_T(DWORD hUnit, float x, float y);

// 处理游戏命令
void ProcessGameCmd(Unit *unit, Command *CommandData, DWORD targetUint);

// 获取本地英雄
bool GetLocalHero(std::string &name);

void UnitCommandON();
