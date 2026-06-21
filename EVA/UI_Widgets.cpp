#include <EVA/UI.hpp>
#include <EVA/Arena.hpp>
#include <EVA/Asset.hpp>

UIBox* UILabel(UIContext& ui, const char* text)
{
	UIBox* box = UIBeginBox(ui, 0);
	box->layout = &UILayoutMode_Text;
	box->text = ArenaInternCString(FrameArena, text);
	box->color = {1,1,1,1};
	box->font = ui.default_font;
	UIEndBox(ui);
	return box;
}

UIBox* UISprite(UIContext& ui, Sprite* sprite)
{
	UIBox* box = UIBeginBox(ui, 0);
	box->color = {1,1,1,1};
	UISetBackgroundSprite(box, sprite);
	UISetSize(box, sprite->w, sprite->h);
	UIEndBox(ui);
	return box;
}

bool UIButton(UIContext& ui, const char* text)
{
	UIPushId(ui, text);
	UIBox* button = UIBeginBox(ui, 1);

	if      (button->flags & UIBoxFlags_Pressed) button->color = COLOR_RGB(87, 7, 31);
	else if (button->flags & UIBoxFlags_Hover)   button->color = COLOR_RGB(142, 27, 62);
	else                                         button->color = COLOR_RGB(115, 18, 47);

	UISetPadding(button, 8, 16);
	UILabel(ui, text);
	UIEndBox(ui);
	UIPopId(ui);

	return button->flags & UIBoxFlags_Clicked;
}

bool UITreeNode(UIContext& ui, const char* text, UITreeNodeFlags flags)
{
	UIPushId(ui, text);
	UIBox* box = UIBeginBox(ui, 1);

	if      (box->flags & UIBoxFlags_Pressed) box->color = COLOR_RGB(87, 7, 31);
	else if (box->flags & UIBoxFlags_Hover)   box->color = COLOR_RGB(142, 27, 62);
	else                                      box->color = COLOR_RGB(115, 18, 47);

	UISetPadding(box, 4, 16);
	UILabel(ui, text);
	UIEndBox(ui);
	UIPopId(ui);

	return false;
}