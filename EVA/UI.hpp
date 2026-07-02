#pragma once
#include <EVA/Common.hpp>
#include <EVA/Math.hpp>
#include <vector>

struct UIContext;
struct UIBox;
struct DrawContext;
struct Font;
struct Sprite;
typedef union SDL_Event SDL_Event;

#define COLOR_BG                COLOR_RGBA(55, 6, 21, 220)
#define COLOR_BUTTON            COLOR_RGB(115, 18, 47)
#define COLOR_BUTTON_PRESSED    COLOR_RGB(87, 7, 31)
#define COLOR_BUTTON_HOVER      COLOR_RGB(142, 27, 62)
#define COLOR_BUTTON_ACTIVE     COLOR_RGB(255, 66, 110)

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
	UIBoxFlags_Focus         = 0x10,
	UIBoxFlags_JustCreated   = 0x20,
};
typedef U32 UIBoxFlags; 

enum UIAxis : U8
{
	UIAxis_Horizontal = 0,
	UIAxis_Vertical = 1,
};

enum UIEventType
{
	UIEventType_None = 0,
	UIEventType_Focus,
	UIEventType_Unfocus,
	UIEventType_Text,
	UIEventType_Draw,
};

struct UIEvent
{
	UIEventType type = UIEventType_None;

	union
	{
		struct
		{
			const char* text;
		} text;
		int dummy = 0;
	};
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
	bool (*event_handler)(UIBox* box, const UIEvent& event) = nullptr;

	// CALCULATED BY LAYOUT:
	float2 position = {};
	float2 size     = {};

	// CONVENIENCE FUNCTIONS:
	UIBox*   SetPadding              (int padding);
	UIBox*   SetPadding              (int vpadding, int hpadding);
	UIBox*   SetPadding              (int top, int right, int bottom, int left);
	UIBox*   SetGap                  (int gap);
	UIBox*   SetSize                 (float width, float height);
	UIBox*   SetPosition             (float x, float y);
	UIBox*   SetColor                (float4 color);
	UIBox*   SetPosition             (float2 position);
	UIBox*   SetBackgroundSprite     (Sprite* sprite);
	UIBox*   SetFlex                 (UIAxis axis, UIAlignment main = UIAlignment_Start, UIAlignment cross = UIAlignment_Start);
	UIBox*   SetFlexGrow             (float flex_grow);
	void*    GetData                 ();

	bool Clicked() { return flags & UIBoxFlags_Clicked; }
	bool Hovered() { return flags & UIBoxFlags_Hover; }
	bool Pressed() { return flags & UIBoxFlags_Pressed; }
	bool Focused() { return flags & UIBoxFlags_Focus; }
};

struct UIContext
{
	Font*               default_font     = nullptr;
	std::vector<UIBox*> all_boxes        = {};
	std::vector<U32>    id_stack         = {};
	std::vector<UIBox*> box_stack        = {};
	UIBox               root             = {};
	U32                 pressed_id       = {};
	UIBox*              focus_box        = nullptr;
	bool                captures_mouse   = false;
};


void UIInitialize();
void UIContextInit(UIContext& ui, Font* default_font);
UIBox* UIBeginBox(U32 id = 0, int data_size = 0, const void* data_default = nullptr);
void UIEndBox();
U32  UIPushId(U32 id);
U32  UIPushId(const char* str);
U32  UIPushId(const void* ptr);
void UIPopId();
void UIBeginFrame();
void UIEndFrame();
void UIDraw();

extern UILayoutMode UILayoutMode_Flex;
extern UILayoutMode UILayoutMode_Text;
extern UILayoutMode UILayoutMode_Fixed;

////////////////////////////////////////////////////////////
// Widgets
////////////////////////////////////////////////////////////

enum UIButtonFlagBits : U32
{
	UIButtonFlags_None = 0,
	UIButtonFlags_Small = 0x01,
	UIButtonFlags_Toggle = 0x02,
	UIButtonFlags_Disabled = 0x04,
};
typedef U32 UIButtonFlags;

bool UIButton(const char* text, UIButtonFlags flags = 0);
UIBox* UILabel(const char* text, int text_len = -1);
UIBox* UISprite(Sprite* sprite, U32 id = 0);

enum UITreeNodeFlagBits : U32
{
	UITreeNodeFlags_None        = 0x00,
	UITreeNodeFlags_Leaf        = 0x01,
	UITreeNodeFlags_DefaultOpen = 0x02,
	UITreeNodeFlags_Selected    = 0x04,
};
typedef U32 UITreeNodeFlags;
bool UIBeginTreeNode(const char* text,  UIBox** out_box = nullptr, UITreeNodeFlags flags = 0);
void UIBeginTreeList();
void UIEndTreeList();
void UIEndTreeNode();
void UIFocus(UIBox* box);
bool UIProcessSDLEvent(SDL_Event* event);
bool UICapturesMouse();

UIBox* UITextInput(std::vector<char>& buffer);

extern UIContext* UI; // the current context