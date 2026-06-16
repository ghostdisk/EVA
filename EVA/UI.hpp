#pragma once
#include <EVA/Common.hpp>
#include <EVA/Math.hpp>
#include <vector>

struct UIContext;
struct UIBox;
struct DrawContext;
struct Font;

struct UIContext
{
	Font*               default_font = nullptr;
	std::vector<UIBox*> all_boxes    = {};
};

struct UIBox
{
	U32 id; 
	bool used_this_frame = false;
};

void UIInitialize();
void UIContextInit(UIContext& ui, Font* default_font);
UIBox* UIBeginBox(UIContext* ui, U32 id);
void UIEndBox(UIContext& ui);
void UIPushId(UIContext& ui, U32 id);
void UIPushId(UIContext& ui, const char* str);
void UIPopId(UIContext& ui);
void UIBeginFrame(UIContext& ui);
void UIEndFrame(UIContext& ui);
void UIDraw(UIContext& ui, DrawContext& dc);

// Widgets:
