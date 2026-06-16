// #include "pch.h"
#pragma once
#include <windows.h>
#include <string>

#pragma pack(push, 1)

struct CUnit;
struct CPreselectUI;
struct CAgentTimer;

struct HashGroup
{
	unsigned int hashA;
	unsigned int hashB;
	HashGroup() noexcept : hashA(0xFFFFFFFF), hashB(0xFFFFFFFF) {}
	HashGroup(unsigned int hashA_, unsigned int hashB_) : hashA(hashA_), hashB(hashB_) {}
};

struct HashGroupRaw
{
	unsigned int hashA;
	unsigned int hashB;

	HashGroup* toHashGroupPtr() { return (HashGroup*)this; }
};

struct ColorARGB
{
	unsigned char b;
	unsigned char g;
	unsigned char r;
	unsigned char a;
};

struct CUnit_174 // 1.24d和之后
{
	unsigned char unk_174[0x68];
	HashGroupRaw abilityHash; // 0x1DC	//以下偏移为按照1.24d以后
	unsigned char unk_1E4[0x44];
	float impactZ;	   // 0x228 Projectile impact Z
	float impactSwimZ; // 0x22C	Projectile impact Z (swimming)
	float launchX;	   // 0x230	Projectile launch X
	float launchY;	   // 0x234	Projectile launch Y
	float launchZ;	   // 0x238 Projectile launch Z
	float launchSwimZ; // 0x23C Projectile launch Z (swimming)
	unsigned char unk_240[0x8];
	unsigned int unitTypeFlag;	 // 0x248
	unsigned char unk_24C[0x38]; // 0x24C
	float x;					 // 0x284
	float y;					 // 0x288
	unsigned char unk_28C[0x48]; // 0x28C
	ColorARGB vertexColor;		 // 0x2D4	0xAARRGGBB
	unsigned char unk_2D8[0x38];
};

struct AbilityUIDef
{
	void** table;				  // 0x0
	unsigned char unk_4[0x7C];	  // 0x4
	unsigned int hotkey_levels;	  // 0x80
	unsigned int* hotkey_items;	  // 0x84
	unsigned int unk_88;		  // 0x88
	unsigned int hotkey2_levels;  // 0x8C
	unsigned int* hotkey2_items;  // 0x90
	unsigned char unk_94[0xA0];	  // 0x94
	unsigned int tooltip_levels;  // 0x134
	char** tooltip_items;		  // 0x138
	unsigned int unk_13C;		  // 0x13C
	unsigned int tooltip2_levels; // 0x140
	char** tooltip2_items;		  // 0x144
	unsigned char unk_148[0x10];  // 0x148
	unsigned int desc_levels;	  // 0x158
	char** desc_items;			  // 0x15C
	unsigned int unk_160;		  // 0x160
	unsigned int desc2_levels;	  // 0x164
	char** desc2_items;			  // 0x168
	//...
};

struct AbilityDefData
{
	void** vtable;
	unsigned char unk_4[0x24];		// 0x4
	AbilityUIDef* abilityUIDef;		// 0x28
	unsigned int uiDefAvailable;	// 0x2C
	unsigned int abilityBaseTypeId; // 0x30
	//...
}; // sizeof = ?

struct CUnit
{ // TODO +0x14C -1
	void** vtable;
	unsigned int refCount; // 0x4
	unsigned int unk_8;	   // 0x8
	HashGroup hash;		   // 0xC
	unsigned char unk_14[0xC];
	unsigned int stateFlag; // 0x20 表示状态, 1 << 3 表示无敌 TODO 继续研究其他bit
	unsigned char unk_24[0xC];
	unsigned int typeId; // 0x30
	unsigned char unk_34[0x1C];
	CPreselectUI* preSelectUI; // 0x50
	float unk_54;			   // 0x54
	unsigned int owner;		   // 0x58
	unsigned char unk_5C[0x44];
	HashGroup hash_unkA0; // 0xA0
	unsigned char unk_A8[0x38];
	float defense;			  // 0xE0
	unsigned int defenseType; // 0xE4
	unsigned char unk_E8[0x84];
	HashGroup hash_unk16C; // 0x16C
	CUnit_174 unit_174;	   // 0x174
	/*
	unsigned char unk_174[0x68];//在这一段从1.24d? 加了4字节，< 1.24d为0x64

	//0x19C,0x1A0与当前OrderTarg有关

	HashGroup					abilityHash;		//0x1DC	//以下偏移为按照1.24d以后
	unsigned char unk_1E4[0x44];
	//TODO 0x194 = current action ?
	float						impactZ;			//0x228 Projectile impact Z
	float						impactSwimZ;		//0x22C	Projectile impact Z (swimming)
	float						launchX;			//0x230	Projectile launch X
	float						launchY;			//0x234	Projectile launch Y
	float						launchZ;			//0x238 Projectile launch Z
	float						launchSwimZ;		//0x23C Projectile launch Z (swimming)
	unsigned char unk_240[0x8];
	unsigned int					unitTypeFlag;		//0x248
	unsigned char						unk_24C[0x38];		//0x24C
	float						x;					//0x284
	float						y;					//0x288
	unsigned char						unk_28C[0x48];		//0x28C
	ColorARGB					vertexColor;		//0x2D4	0xAARRGGBB
	unsigned char						unk_2D8[0x38];
	*/
};

struct CPreselectUI
{
	void** vtable;		//=0x9409E0
	unsigned int unk_4; // always 0 ?
	CUnit* unit;		// 指向所属单位
	// CStatBar *statBarHP; // 血条
}; // sizeof = 0x10

struct CAgentTimer
{
	void** vtable; // 0x0
	unsigned char unk_4[0x10];
}; // sizeof = 0x14

struct CAbility
{
	void** vtable; // 0x0
	unsigned int unk4;
	unsigned int flag; // 0x8
	HashGroup hash;	   // 0xC
	unsigned int unk14;
	unsigned int unk18;
	unsigned int unk1C;
	unsigned int flag2;		   // 0x20	& 0x200 表示是否在CD中
	HashGroup nextAbilityHash; // 0x24/
	unsigned int unk2C;
	CUnit* abilityOwner;	  // 0x30
	unsigned int id;		  // 0x34
	unsigned int unk38;		  // 0x38
	unsigned int unk3C;		  // 0x3C
	unsigned int unk40;		  // 0x40
	unsigned char unk44[0x8]; // 0x44
	unsigned int unk4C;		  // 0x4C
	unsigned int level;		  // 0x50
	AbilityDefData* defData;  // 0x54
	CAgentTimer timer58;	  // 0x58
};

struct FloatMini
{
	void** vtable; // 0x0
	float value;   // 0x4
}; // sizeof = 0x8

struct FloatMiniB
{
	void** vtable;
	float value;		// 0x4
	unsigned int unk_8; // 0x8
	unsigned int unk_C; // 0xC
}; // sizeof = 0x10

struct CAbilityAttack
{
	CAbility baseAbility;	// 0x0
	HashGroup hashAcquired; // 0x6C
	unsigned char unk_70[0x14];
	unsigned int dice_weap0; // 0x88
	unsigned int dice_weap1; // 0x8C
	unsigned char unk_90[0x4];
	unsigned int sides_weap0; // 0x94
	unsigned int sides_weap1; // 0x98
	unsigned char unk_9C[0x4];
	unsigned int dmgPlus_weap0; // 0xA0
	unsigned int dmgPlus_weap1; // 0xA4
	unsigned char unk_A8[0x4];
	unsigned int buffs_weap0; // 0xAC
	unsigned int buffs_weap1; // 0xB0
	unsigned char unk_B4[0x28];
	unsigned int weapTp_weap0; // 0xDC
	unsigned int weapTp_weap1; // 0xE0
	unsigned char unk_E4[0x10];
	unsigned int atkType_weap0; // 0xF4
	unsigned int atkType_weap1; // 0xF8
	unsigned char unk_FC[0x58];
	FloatMini cool_weap0;	  // 0x154
	FloatMini cool_weap1;	  // 0x15C
	unsigned int unk_164;	  //			TODO
	FloatMiniB dmgPt_weap0;	  // 0x168
	FloatMiniB dmgPt_weap1;	  // 0x178
	unsigned int unk_188;	  //			TODO
	FloatMiniB backSw_weap0;  // 0x18C
	FloatMiniB backSw_weap1;  // 0x19C
	FloatMiniB factor;		  // 0x1AC
	FloatMini FMini_unk1BC;	  // 0x1BC		TODO
	CAgentTimer timer_unk1C4; // 0x1C4		TODO
	CAgentTimer timer_cool;	  // 0x1D8
	CAgentTimer timer_dmgPt;  // 0x1EC
	CAgentTimer timer_backSw; // 0x200
	unsigned char unk_204[0x40];
	FloatMini range_weap0;	  // 0x254
	FloatMini range_weap1;	  // 0x25C
	unsigned int unk_264;	  //			TODO
	FloatMini FMini_unk268;	  //			TODO
	FloatMini FMini_unk270;	  //			TODO
	FloatMini FMini_unk278;	  //			TODO
	CAgentTimer timer_unk280; //			TODO
	CAgentTimer timer_unk294; //			TODO
	FloatMini FMini_unk2A8;	  //			TODO
	FloatMini FMini_unk2B0;	  //			TODO
	unsigned char unk_2B8[0x10];
	FloatMini FMini_unk2C8; //			TODO
	// padding
	unsigned char unk_2D0[0x108];
	// padding
	CAgentTimer timer_unk3D8; // 0x3D8		TODO
	unsigned char unk_3EC[0xC];
	FloatMini unk_3F8; //			TODO
	FloatMini unk_400; //			TODO
	FloatMini unk_408; //			TODO
}; // sizeof = 0x410

struct CAbilitySpell
{
	CAbility baseAbility; // 0x0
	unsigned int unk6C;	  // 0x6C = dword_6F92ED9C
	unsigned int unk70;
	unsigned int unk74;
	unsigned int unk78;
	unsigned int unk7C;
	FloatMini floatMini80; // value = dword_6FAAE470 似乎是施法时间
	FloatMini floatMini88; // value = dword_6FAAE470
	FloatMini floatMini90; // value = dword_6FAAE470
	FloatMini floatMini98; // value = dword_6FAAE470
	unsigned int unkA0;	   // = 0xFFFFFFFF
	unsigned int unkA4;	   // = 0xFFFFFFFF
	unsigned int unkA8;	   // = 0
	unsigned int unkAC;	   // = 0
	FloatMini floatMiniB0; // value = dword_6FAAE470
	unsigned int unkB8;	   // = 0
	CAgentTimer timer_last;
	CAgentTimer timer_cooldown;
}; // sizeof = 0xE4

struct CAbilityBuff
{
};

struct CLayoutFrame {
	void** vtable;
	unsigned int					pointCount;			//0x4	=9					LayoutFrame
	unsigned int					pTL;				//0x8	=0					LayoutFrame
	unsigned int					pTC;				//0xC	=0					LayoutFrame
	unsigned int					pTR;				//0x10	=0					LayoutFrame
	unsigned int					pL;					//0x14	=0					LayoutFrame
	unsigned int					pC;					//0x18	=0					LayoutFrame
	unsigned int					pR;					//0x1C	=0					LayoutFrame
	unsigned int					pBL;				//0x20	=0					LayoutFrame
	unsigned int					pBC;				//0x24	=0					LayoutFrame
	unsigned int					pBR;				//0x28	=0					LayoutFrame
	unsigned int					unk_2C;				//0x2C	=0					LayoutFrame
	unsigned int					unk_30;				//0x30	=0					LayoutFrame
	unsigned int					unk_34;				//0x34	=0					LayoutFrame
	unsigned int					unk_38;				//0x38	=0					LayoutFrame
	unsigned int					unk_3C;				//0x3C	=this_3C			LayoutFrame	指向自己的指针
	//FRAMENODE* unk_40;				//0x40	=~this_3C			LayoutFrame	参考6F605C70
	void* unk_40;				//0x40	=~this_3C			LayoutFrame	参考6F605C70
	float						borderB;			//0x44	=0.0				LayoutFrame
	float						borderL;			//0x48	=0.0				LayoutFrame
	float						borderU;			//0x4C	=0.0				LayoutFrame	
	float						borderR;			//0x50	=0.0				LayoutFrame
	unsigned int					unk_54;				//0x54						LayoutFrame 坐标改变时改变
	float						width;				//0x58	=0.0				LayoutFrame	内容改变时改变
	float						height;				//0x5C	=0.0				LayoutFrame 内容改变时改变
	float						scale;				//0x60	=0.0				LayoutFrame 整体缩放比例
	void* unk_64;				//0x64						LayoutFrame	SimpleFrame void (__thiscall *v4)(_DWORD); 
};//sizeof = 0x68

struct CSimpleFrame {
	CLayoutFrame /*0x68 bytes*/	baseLayoutFrame;	//0x0						LayoutFrame
	//CSimpleTop* unk_68;				//0x68	=*6FACE758			SimpleFrame
	void* unk_68;
	CLayoutFrame* parent;				//0x6C	=0					SimpleFrame
	unsigned int					unk_70;				//0x70	=0					SimpleFrame
	unsigned int					unk_74;				//0x74	=0					SimpleFrame
	unsigned int					unk_78;				//0x78	=0					SimpleFrame
	unsigned int					unk_7C;				//0x7C	=0					SimpleFrame	Storm#403 删除
	unsigned int					unk_80;				//0x80	=0					SimpleFrame
	unsigned int					unk_84;				//0x84  =0					CToolTipWar3 初始化在6F609980
	unsigned int					unk_88;				//0x88	=0xFF000000			SimpleFrame 只初始化了第一个BYTE，是BYTE变量？
	unsigned int					unk_8C;				//0x8C	=0					SimpleFrame	flags?
	unsigned int					visible;			//0x90	=0,1				SimpleFrame	ToolTip是0	SimpleFrame是1 显示时是1，隐藏时是0
	unsigned int					needUpdate;			//0x94	=1 bool?			SimpleFrame	在6F609AD0被使用运行完归0	   显示时是1，隐藏时是0
	float						unk_98;				//0x98	=0.0				SimpleFrame
	float						unk_9C;				//0x9C	=0.0				SimpleFrame
	float						unk_A0;				//0xA0	=0.0				SimpleFrame
	float						unk_A4;				//0xA4	=0.0				SimpleFrame
	float						unk_A8;				//0xA8	=0.0				SimpleFrame
	float						unk_AC;				//0xAC	=0.0				SimpleFrame
	float						unk_B0;				//0xB0	=0.0				SimpleFrame
	float						unk_B4;				//0xB4	=0.0				SimpleFrame
	unsigned int					unk_B8;				//0xB8	=1					SimpleFrame
	unsigned int					unk_BC;				//0xBC	=1					SimpleFrame
	unsigned int					unk_C0;				//0xC0	=1					SimpleFrame
	unsigned int					unk_C4;				//0xC4	=1					SimpleFrame
	unsigned int					unk_C8;				//0xC8	=0					SimpleFrame
	//SimpleFrameTextureSettings* textureSettings;	//0xCC	=0					SimpleFrame, CToolTipWar3
	void* textureSettings;
	unsigned int					unk_D0;				//0xD0	=0					SimpleFrame
	unsigned int					unk_D4;				//0xD4	=this_D4			SimpleFrame 指向自己的指针
	unsigned int					unk_D8;				//0xD8	=~this_D4			SimpleFrame
	//SimpleFrame_DC				unk_DC;				//0xDC						SimpleFrame 初始化过程见 6F609D14
	unsigned char					unk_DC[0x3C];
	unsigned int					unk_118;			//0x118	=0					SimpleFrame
	unsigned int					unk_11C;			//0x11C	=this_11C			SimpleFrame 指向自己的指针
	unsigned int					unk_120;			//0x120	=~this_11C			SimpleFrame
};//sizeof = 0x124

struct CSimpleButton
{
	CSimpleFrame /*0x124 bytes*/ baseSimpleFrame; // 0x0
	//unsigned char baseSimpleFrame[0x124];
	unsigned char* clickEventObserver;			  // 0x124 CGameUI*?
	unsigned int clickEventId;					  // 0x128
	unsigned char* mouseEventObserver;			  // 0x12C	6F603060 CGameUI*?
	unsigned int mouseOverEventId;				  // 0x130
	unsigned int mouseOutEventId;				  // 0x134
	unsigned int currentState;					  // 0x138
	unsigned int enabled;						  // 0x13C
	unsigned int mouseButtonFlags;				  // 0x140	=0x10	指定什么鼠标键可以触发按钮
	unsigned int unk_144;						  // 0x144
	unsigned int unk_148;						  // 0x148
	unsigned int unk_14C;						  // 0x14C
	float unk_150;								  // 0x150 =0.001
	float unk_154;								  // 0x154 =-0.001
	void* textureDisabled;			  // 0x158
	void* textureEnabled;				  // 0x15C
	void* texturePushed;				  // 0x160
	void* currentTexture;				  // 0x164
}; // sizeof = 0x168

struct CCommandButtonData
{
	void** vtable;				// 0x0
	unsigned int abilityId;		// 0x4 物品按钮和基本技能这里是0
	unsigned int orderId_8;		// 0x8
	unsigned int orderId_C;		// 0xC
	unsigned int flag10;		// 0x10
	unsigned int unk14;			// 0x14
	unsigned int unk18;			// 0x18
	HashGroup agentHash;		// 0x1C
	unsigned int unk24;			// 0x24
	unsigned int isSpell;		// 0x28
	char title[0x100];			// 0x2C 长度正确？
	unsigned char unk12C[0x58]; // 0x12C
	unsigned int unk184;		// 0x184
	unsigned int unk188;		// 0x188
	char tooltip[0x400];		// 0x18C 长度正确？
	unsigned char unk58C[0x8];	// 0x58C
	unsigned int manaCost;		// 0x594
	unsigned int unk598;		// 0x598
	unsigned int displayOrder;	// 0x59C ?
	unsigned int unk5A0;		// 0x5A0 英雄技能是2 物品是0
	unsigned int unk5A4;		// 0x5A4 英雄技能是1 物品是0
	unsigned int unk5A8;		// 0x5A8 英雄技能是1 物品是0
	unsigned int hotkey;		// 0x5AC
	unsigned int unk5B0;		// 0x5B0
	unsigned int unk5B4;		// 0x5B4
	unsigned int unk5B8;		// 0x5B8 英雄技能是1 物品是0
	unsigned int unk5BC;		// 0x5BC 英雄技能是1 物品是0
	unsigned int unk5C0;		// 0x5C0 英雄技能是0 物品是1
	unsigned int unk5C4;		// 0x5C4 英雄技能是0 物品是1
	char iconPath[0x100];		// 0x5C8 长度正确？
	unsigned int unk6C8;		// 0x6C8
	unsigned int unk6CC;		// 0x6CC
	unsigned int unk6D0;		// 0x6D0
	// unsigned char *ability;		// 0x6D4 基本技能(例如move)这里是NULL
	CAbility* ability;
};

// 需要搞清楚CShrinkingButton (可能只有虚函数)
struct CCommandButton
{
	CSimpleButton baseSimpleButton; // 0x0
	unsigned char unk168[0x28];
	CCommandButtonData* commandButtonData; // 0x190
	unsigned char unk194[0x2C];
}; // sizeof = 0x1C0

#pragma pack(pop)

std::string GetAbilityCCID(DWORD abilityID);

void __fastcall SetCdForAddr(DWORD pThis, int dummy);
