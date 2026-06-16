#include <EVA/UI.hpp>
#include <EVA/GL.hpp>
#include <EVA/Draw.hpp>
#include <EVA/Platform.hpp>


void UIInitialize()
{
}

void UIContextInit(UIContext& ui, Font* default_font)
{
	ui.default_font = default_font;
}

UIBox* UIBeginBox(UIContext& ui, U32 id)
{
	return 0;
}

void UIEndBox(UIContext& ui)
{

}

void UIPushId(UIContext& ui, U32 id)
{

}

void UIPushId(UIContext& ui, const char* str)
{

}

void UIPopId(UIContext& ui)
{

}

void UIBeginFrame(UIContext& ui)
{
}

void UIEndFrame(UIContext& ui)
{

}

void UIDraw(UIContext& ui, DrawContext& dc)
{
	DrawRectangle(dc, 100, 100, 300, 300, {0,0,0.2,1});
	DrawRectangle(dc, 400, 100, 300, 300, {0.2,0,0,1});
	DrawText(dc, ui.default_font, "The quick brown fox jumps over the lazy dog.", 100, 100, { 1, 1, 1, 1});
}