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

UIBox* UISprite(UIContext& ui, Sprite* sprite, U32 id)
{
	UIBox* box = UIBeginBox(ui, id);
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

UITreeNodeStatus UIBeginTreeNode(UIContext& ui, const char* text, UITreeNodeFlags flags)
{
	UIBox* outer_contents = UIBeginBox(ui);
	UISetFlex(outer_contents, UIAxis_Vertical, UIAlignment_Start, UIAlignment_Stretch);

	UITreeNodeStatus status = {};
	status.selected = flags & UITreeNodeFlags_Selected;

	UIPushId(ui, text);

	bool* open = nullptr;

	{
		bool default_data = (bool)(flags & UITreeNodeFlags_DefaultOpen);
		UIBox* box = UIBeginBox(ui, 1, 1, &default_data);
		open = (bool*)UIBoxGetData(box);


		if (status.selected)
		{
			box->color = COLOR_RGB(255, 66, 110);
		}
		else
		{
			if      (box->flags & UIBoxFlags_Pressed) box->color = COLOR_RGB(87, 7, 31);
			else if (box->flags & UIBoxFlags_Hover)   box->color = COLOR_RGB(142, 27, 62);
			else                                      box->color = COLOR_RGB(115, 18, 47);
		}

		UISetFlex(box, UIAxis_Horizontal, UIAlignment_Start, UIAlignment_Center);

		U32 arrow_id = 1337;
		UIBox* arrow_box = UIBeginBox(ui, arrow_id);
		UISetPadding(arrow_box, 6, 6);
		UIBox* arrow = UISprite(ui, *open ? Library::spr_ui_arrow_down : Library::spr_ui_arrow_right);
		if (flags & UITreeNodeFlags_Leaf)
		{
			arrow->color.w = 0;
		}
		UIEndBox(ui);

		if (arrow_box->flags & UIBoxFlags_Clicked)
		{
			*open = !*open;
		}
		else if (box->flags & UIBoxFlags_Clicked)
		{
			status.selected = !status.selected;
		}

		status.hover = box->flags & UIBoxFlags_Hover;
		UILabel(ui, text);

		UIEndBox(ui);
		UIPopId(ui);
	}

	status.open = *open;

	if (*open)
	{
		UIBox* inner_contents = UIBeginBox(ui);
		UISetFlex(inner_contents, UIAxis_Vertical, UIAlignment_Start, UIAlignment_Stretch);
		UISetPadding(inner_contents, 0, 0, 0, 20);
	}
	else
	{
		UIEndBox(ui);
	}
	return status;
}

void UIEndTreeNode(UIContext& ui)
{
	UIEndBox(ui);
	UIEndBox(ui);
}

void UIBeginTreeList(UIContext& ui)
{
	UIBox* box = UIBeginBox(ui);
	UISetSize(box, 300, 0);
	UISetFlex(box, UIAxis_Vertical, UIAlignment_Start, UIAlignment_Stretch);
}

void UIEndTreeList(UIContext& ui)
{
	UIEndBox(ui);
}