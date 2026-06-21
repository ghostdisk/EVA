#pragma once
#include <EVA/Common.hpp>
#include <EVA/Math.hpp>
#include <vector>

struct UIContext;
struct UIBox;
struct DrawContext;
struct Font;
struct Sprite;

struct UILayoutMode
{
	void (*Pass1)(UIBox* box) = nullptr;
	void (*Pass2)(UIBox* box) = nullptr;
};

enum UIAlignment : U8
{
	UIAlignment_Start   = 0,
	UIAlignment_Center  = 1,
	UIAlignment_End     = 2,
	UIAlignment_Stretch = 3,
};

enum UIBoxFlagBits : U32
{
	UIBoxFlags_None          = 0,
	UIBoxFlags_UsedThisFrame = 0x01,
	UIBoxFlags_Hover         = 0x02,
	UIBoxFlags_Pressed       = 0x04,
	UIBoxFlags_Clicked       = 0x08,
};
typedef U32 UIBoxFlags; 

enum UIAxis : U8
{
	UIAxis_Horizontal = 0,
	UIAxis_Vertical = 1,
};

struct UIBox
{
	U32        id              = 0; 
	UIBoxFlags flags           = UIBoxFlags_None;
	UIBox*     parent          = nullptr;
	UIBox*     first_child     = nullptr;
	UIBox*     last_child      = nullptr;
	UIBox*     next_sibling    = nullptr;

	UILayoutMode* layout = nullptr;

	// PROPERTIES:
	const char* text                 = nullptr;
	Font*       font                 = nullptr;
	float2      min_size             = {};
	U8          padding_top          = 0;
	U8          padding_right        = 0;
	U8          padding_bottom       = 0;
	U8          padding_left         = 0;
	UIAxis      flex_axis            = UIAxis_Horizontal;
	U8          flex_gap             = 0;
	UIAlignment main_axis_alignment  = UIAlignment_Start;
	UIAlignment cross_axis_alignment = UIAlignment_Start;
	float       flex_grow            = 0;
	float4      color                = {0,0,0,0};
	Sprite*     background_sprite    = nullptr;

	// CALCULATED BY LAYOUT:
	float2 position = {};
	float2 size = {};
};

struct UIContext
{
	Font*               default_font = nullptr;
	std::vector<UIBox*> all_boxes    = {};
	std::vector<U32>    id_stack     = {};
	std::vector<UIBox*> box_stack    = {};
	UIBox               root         = {};
	U32                 pressed_id   = {};
};


void UIInitialize();
void UIContextInit(UIContext& ui, Font* default_font);
UIBox* UIBeginBox(UIContext& ui, U32 id = 0, int data_size = 0, const void* data_default = nullptr);
void UIEndBox(UIContext& ui);
U32  UIPushId(UIContext& ui, U32 id);
U32  UIPushId(UIContext& ui, const char* str);
void UIPopId(UIContext& ui);
void UIBeginFrame(UIContext& ui);
void UIEndFrame(UIContext& ui);
void UIDraw(UIContext& ui, DrawContext& dc);

extern UILayoutMode UILayoutMode_Flex;
extern UILayoutMode UILayoutMode_Text;

////////////////////////////////////////////////////////////
// Convenience functions
////////////////////////////////////////////////////////////

void  UISetPadding              (UIBox* box, int padding);
void  UISetPadding              (UIBox* box, int vpadding, int hpadding);
void  UISetPadding              (UIBox* box, int top, int right, int bottom, int left);
void  UISetGap                  (UIBox* box, int gap);
void  UISetSize                 (UIBox* box, float width, float height);
void  UISetBackgroundSprite     (UIBox* box, Sprite* sprite);
void* UIBoxGetData              (UIBox* box);
void  UISetFlex                 (UIBox* box, UIAxis axis, UIAlignment main = UIAlignment_Start, UIAlignment cross = UIAlignment_Start);

////////////////////////////////////////////////////////////
// Widgets
////////////////////////////////////////////////////////////

bool UIButton(UIContext& ui, const char* text);
UIBox* UILabel(UIContext& ui, const char* text);
UIBox* UISprite(UIContext& ui, Sprite* sprite);

enum UITreeNodeFlagBits : U32
{
	UITreeNodeFlags_None        = 0x00,
	UITreeNodeFlags_Leaf        = 0x01,
	UITreeNodeFlags_Selected    = 0x02,
	UITreeNodeFlags_DefaultOpen = 0x04,
};
typedef U32 UITreeNodeFlags;

bool UIBeginTreeNode(UIContext& ui, const char* text, UITreeNodeFlags flags = 0);
void UIEndTreeNode(UIContext& ui);