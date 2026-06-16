#pragma once
#include <EVA/Common.hpp>
#include <EVA/Math.hpp>
#include <vector>

struct UIContext;
struct UIBox;


struct UIQuad
{
	float4 position_rect;
};

struct UIContext
{
	std::vector<UIBox*> all_boxes;
	std::vector<UIQuad> quads;
};

struct UIBox
{
	U32 id; 
	bool used_this_frame = false;
};

void UIInitialize();
void UIContextInit(UIContext& ui);
UIBox* UIBeginBox(UIContext* ui, U32 id);
void UIEndBox(UIContext& ui);
void UIPushId(UIContext& ui, U32 id);
void UIPushId(UIContext& ui, const char* str);
void UIPopId(UIContext& ui);
void UIBeginFrame(UIContext& ui);
void UIEndFrame(UIContext& ui);
void UIRender(UIContext& ui);


// Widgets:
