#include <EVA/UI.hpp>
#include <EVA/Arena.hpp>
#include <EVA/Asset.hpp>
#include <EVA/Library.hpp>
#include <SDl3/SDL_events.h>

#define COLOR_BUTTON            COLOR_RGB(115, 18, 47)
#define COLOR_BUTTON_PRESSED    COLOR_RGB(87, 7, 31)
#define COLOR_BUTTON_HOVER      COLOR_RGB(142, 27, 62)
#define COLOR_BUTTON_ACTIVE     COLOR_RGB(255, 66, 110)

UIBox* UILabel(const char* text, int text_len)
{
	UIBox* box = UIBeginBox(0);
	box->layout = &UILayoutMode_Text;
	box->text = ArenaInternCString(FrameArena, text, text_len);
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

	if      (button->flags & UIBoxFlags_Pressed) button->color = COLOR_BUTTON_PRESSED;
	else if (button->flags & UIBoxFlags_Hover)   button->color = COLOR_BUTTON_HOVER;
	else                                         button->color = COLOR_BUTTON;

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
			box->color = COLOR_BUTTON_ACTIVE;
		}
		else
		{
			if      (box->flags & UIBoxFlags_Pressed) box->color = COLOR_BUTTON_PRESSED;
			else if (box->flags & UIBoxFlags_Hover)   box->color = COLOR_BUTTON_HOVER;
			else                                      box->color = COLOR_BUTTON;
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

// @CONSTRUCTOR_NOT_CALLED
struct TextEdit
{
	std::vector<char>* buffer;
	int    cursor;

	void Insert(const char* chunk)
	{
		int chunk_len = strlen(chunk);
		int old_text_len = buffer->size();
		buffer->resize(buffer->size() + chunk_len);

		memmove(buffer->data() + cursor + chunk_len, buffer->data() + cursor, old_text_len - cursor);
		memcpy(buffer->data() + cursor, chunk, chunk_len);
		cursor += chunk_len;
	}
};

UIBox* UITextInput(std::vector<char>& buffer)
{
	UIBox* box = UIBeginBox(1, sizeof(TextEdit))
		->SetPadding(4);

	TextEdit* text_edit = (TextEdit*)box->GetData();
	text_edit->buffer = &buffer;
	if (box->flags & UIBoxFlags_JustCreated) text_edit->cursor = buffer.size();

	if (box->flags & UIBoxFlags_Focus) box->SetColor(COLOR_BUTTON_ACTIVE);
	else                               box->SetColor(COLOR_BUTTON);

	box->event_handler = 
		[](UIBox* box, const UIEvent& event)
		{
			switch (event.type)
			{
				case UIEventType_Focus:
				{
					SDL_StartTextInput(GameWindow);
					return true;
				}
				case UIEventType_Unfocus:
				{
					SDL_StopTextInput(GameWindow);
					return true;
				}
				case UIEventType_Text:
				{
					TextEdit* text_edit = (TextEdit*)box->GetData();
					text_edit->Insert(event.text);
					return true;
				}
				default: return false;
			}
			return false;
		};

	if (buffer.size() > 0)
		UILabel(buffer.data(), buffer.size());
	UIEndBox();

	return box;
}