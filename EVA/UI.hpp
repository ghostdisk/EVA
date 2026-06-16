#pragma once
#include <EVA/Common.hpp>
#include <EVA/Math.hpp>
#include <vector>

struct UIContext;
struct UIBox;
struct DrawContext;
struct Font;

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

struct UIBox
{
	U32    id              = 0; 
	bool   used_this_frame = false;
	UIBox* parent          = nullptr;
	UIBox* first_child     = nullptr;
	UIBox* last_child      = nullptr;
	UIBox* next_sibling    = nullptr;
	float4 color           = {0,0,0,0};

	UILayoutMode* layout = nullptr;

	// PROPERTIES:
	const char* text                 = nullptr;
	Font*       font                 = nullptr;
	float2      min_size             = {};
	U8          padding_top          = 0;
	U8          padding_right        = 0;
	U8          padding_bottom       = 0;
	U8          padding_left         = 0;
	U8          flex_axis            = 0;
	U8          flex_gap             = 0;
	UIAlignment main_axis_alignment  = UIAlignment_Start;
	UIAlignment cross_axis_alignment = UIAlignment_Start;
	float       flex_grow            = 0;

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
};


void UIInitialize();
void UIContextInit(UIContext& ui, Font* default_font);
UIBox* UIBeginBox(UIContext& ui, U32 id);
void UIEndBox(UIContext& ui);
U32  UIPushId(UIContext& ui, U32 id);
U32  UIPushId(UIContext& ui, const char* str);
void UIPopId(UIContext& ui);
void UIBeginFrame(UIContext& ui);
void UIEndFrame(UIContext& ui);
void UIDraw(UIContext& ui, DrawContext& dc);

UIBox* UILabel(UIContext& ui, const char* text);

// Convenience functions:
void UISetPadding   (UIBox* box, int padding);
void UISetPadding   (UIBox* box, int vpadding, int hpadding);
void UISetPadding   (UIBox* box, int top, int right, int bottom, int left);
void UISetGap       (UIBox* box, int gap);

extern UILayoutMode UILayoutMode_Flex;
extern UILayoutMode UILayoutMode_Text;