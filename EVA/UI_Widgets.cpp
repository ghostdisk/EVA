#include <EVA/UI.hpp>
#include <EVA/Arena.hpp>
#include <EVA/Asset.hpp>
#include <EVA/Library.hpp>
#include <SDl3/SDL_events.h>

UIBox* UILabel(const char* text)
{
	UIBox* box = UIBeginBox(0);
	box->layout = &UILayoutMode_Text;
	box->text = ArenaInternCString(FrameArena, text);
	box->color = {1,1,1,1};
	box->font = UI->default_font;
	UIEndBox();
	return box;
}

UIBox* UISprite(Sprite* sprite, U32 id)
{
	UIBox* box = UIBeginBox(id)
		->SetBackgroundSprite(sprite)
		->SetSize(sprite->w, sprite->h);
	box->color = {1,1,1,1};
	UIEndBox();
	return box;
}

bool UIButton(const char* text)
{
	UIPushId(text);
	UIBox* button = UIBeginBox(1)->SetPadding(8, 16);

	if      (button->flags & UIBoxFlags_Pressed) button->color = COLOR_RGB(87, 7, 31);
	else if (button->flags & UIBoxFlags_Hover)   button->color = COLOR_RGB(142, 27, 62);
	else                                         button->color = COLOR_RGB(115, 18, 47);

	UILabel(text);
	UIEndBox();
	UIPopId();

	return button->flags & UIBoxFlags_Clicked;
}

UITreeNodeStatus UIBeginTreeNode(const char* text, UITreeNodeFlags flags)
{
	UIBox* outer_contents = UIBeginBox()
		->SetFlex(UIAxis_Vertical, UIAlignment_Start, UIAlignment_Stretch);

	UITreeNodeStatus status = {};
	status.selected = flags & UITreeNodeFlags_Selected;

	UIPushId(text);

	bool* open = nullptr;

	{
		bool default_data = (bool)(flags & UITreeNodeFlags_DefaultOpen);

		UIBox* box = UIBeginBox(1, 1, &default_data)
			->SetFlex(UIAxis_Horizontal, UIAlignment_Start, UIAlignment_Center);

		open = (bool*)box->GetData();

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


		U32 arrow_id = 1337;
		UIBox* arrow_box = UIBeginBox(arrow_id)->SetPadding(6);
		UIBox* arrow = UISprite(*open ? Library::spr_ui_arrow_down : Library::spr_ui_arrow_right);
		if (flags & UITreeNodeFlags_Leaf)
		{
			arrow->color.w = 0;
		}
		UIEndBox();

		if (arrow_box->flags & UIBoxFlags_Clicked)
		{
			*open = !*open;
		}
		else if (box->flags & UIBoxFlags_Clicked)
		{
			status.selected = !status.selected;
		}

		status.hover = box->flags & UIBoxFlags_Hover;
		UILabel(text);

		UIEndBox();
		UIPopId();
	}

	status.open = *open;

	if (*open)
	{
		UIBox* inner_contents = UIBeginBox()
			->SetPadding(0, 0, 0, 20)
			->SetFlex(UIAxis_Vertical, UIAlignment_Start, UIAlignment_Stretch);
	}
	else
	{
		UIEndBox();
	}
	return status;
}

void UIEndTreeNode()
{
	UIEndBox();
	UIEndBox();
}

void UIBeginTreeList()
{
	UIBox* box = UIBeginBox()
		->SetFlex(UIAxis_Vertical, UIAlignment_Start, UIAlignment_Stretch)
		->SetSize(300, 0);
}

void UIEndTreeList()
{
	UIEndBox();
}

void UIFlexSpacer()
{
	UIBox* box = UIBeginBox();
	box->flex_grow = 1;
	UIEndBox();
}

UIBox* UITextInput(char* buf, size_t buf_size)
{
	UIBox* box = UIBeginBox(1)
		->SetPadding(4)
		->SetColor(COLOR_RGB(87, 7, 31));

	box->event_handler = 
		[](UIBox* box, const UIEvent& event)
		{
			switch (event.type)
			{
				case UIEventType_Focus:
				{
					printf("Focus!\n");
					return true;
				}
				case UIEventType_Unfocus:
				{
					printf("Unfocus!\n");
					return true;
				}
				case UIEventType_Text:
				{
					printf("Text!\n");
					return true;
				}
				default: return false;
			}
			return false;
		};

	UILabel("Text text text");
	UIEndBox();

	return box;
}