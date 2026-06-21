#include <EVA/UI.hpp>
#include <EVA/Arena.hpp>
#include <EVA/Asset.hpp>
#include <EVA/Library.hpp>

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

bool UIBeginTreeNode(UIContext& ui, const char* text, UITreeNodeFlags flags)
{
	UIBox* outer_contents = UIBeginBox(ui);
	UISetFlex(outer_contents, UIAxis_Vertical, UIAlignment_Start, UIAlignment_Stretch);

	UIPushId(ui, text);

	bool* open = nullptr;

	{
		bool default_data = (bool)(flags & UITreeNodeFlags_DefaultOpen);
		UIBox* box = UIBeginBox(ui, 1, 1, &default_data);
		open = (bool*)UIBoxGetData(box);

		if      (box->flags & UIBoxFlags_Pressed) box->color = COLOR_RGB(87, 7, 31);
		else if (box->flags & UIBoxFlags_Hover)   box->color = COLOR_RGB(142, 27, 62);
		else                                      box->color = COLOR_RGB(115, 18, 47);

		if (box->flags & UIBoxFlags_Clicked)
		{
			*open = !*open;
		}

		UISetPadding(box, 6, 6);
		UISetGap(box, 6);
		UISetFlex(box, UIAxis_Horizontal, UIAlignment_Start, UIAlignment_Center);

		UIBox* arrow = UISprite(ui, *open ? Library::spr_ui_arrow_down : Library::spr_ui_arrow_right);
		if (flags & UITreeNodeFlags_Leaf)
		{
			arrow->color.w = 0;
		}

		UILabel(ui, text);

		UIEndBox(ui);
		UIPopId(ui);
	}

	if (*open)
	{
		UIBox* inner_contents = UIBeginBox(ui);
		UISetFlex(inner_contents, UIAxis_Vertical, UIAlignment_Start, UIAlignment_Stretch);
		UISetPadding(inner_contents, 0, 0, 0, 20);
		return true;
	}
	else
	{
		UIEndBox(ui);
		return false;
	}
}

void UIEndTreeNode(UIContext& ui)
{
	UIEndBox(ui);
	UIEndBox(ui);
}