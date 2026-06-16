#include <EVA/UI.hpp>
#include <EVA/GL.hpp>
#include <EVA/Draw.hpp>
#include <EVA/Platform.hpp>


void UIInitialize()
{
}

void UIContextInit(UIContext& ui)
{
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
	dc.quads.push_back(DrawQuad{
		.position_rect = { 0, 0, 200, 200 },
	});
	dc.quads.push_back(DrawQuad{
		.position_rect = { 200, 200, 400, 200 },
	});
}