#include <EVA/UI.hpp>
#include <EVA/Arena.hpp>
#include <EVA/Asset.hpp>
#include <EVA/Library.hpp>
#include <EVA/Font.hpp>
#include <EVA/Input.hpp>
#include <EVA/Renderer/Renderer.hpp>
#include <SDL3/SDL_events.h>

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

	if      (button->Pressed()) button->color = COLOR_BUTTON_PRESSED;
	else if (button->Hovered()) button->color = COLOR_BUTTON_HOVER;
	else                        button->color = COLOR_BUTTON;

	UILabel(text);
	UIEndBox();
	UIPopId();

	return button->Clicked();
}

bool UIBeginTreeNode(const char* text,  UIBox** out_box, UITreeNodeFlags flags)
{
	UIBox* outer_contents = UIBeginBox()
		->SetFlex(UIAxis_Vertical, UIAlignment_Start, UIAlignment_Stretch);

	UIPushId(text);

	bool* open = nullptr;

	{
		bool default_data = (bool)(flags & UITreeNodeFlags_DefaultOpen);

		UIBox* box = UIBeginBox(1, 1, &default_data)
			->SetFlex(UIAxis_Horizontal, UIAlignment_Start, UIAlignment_Center)
			->SetPadding(2, 8);

		open = (bool*)box->GetData();

		if (flags & UITreeNodeFlags_Selected)
		{
			box->color = COLOR_BUTTON_ACTIVE;
		}
		else
		{
			if      (box->Pressed())   box->color = COLOR_BUTTON_PRESSED;
			else if (box->Hovered())   box->color = COLOR_BUTTON_HOVER;
			else                       box->color = COLOR_BUTTON;
		}


		U32 arrow_id = 1337;
		UIBox* arrow_box = UIBeginBox(arrow_id)->SetPadding(6);
		UIBox* arrow = UISprite(*open ? Library::spr_ui_arrow_down : Library::spr_ui_arrow_right);
		if (flags & UITreeNodeFlags_Leaf)
		{
			arrow->color.w = 0;
		}
		UIEndBox();

		if (arrow_box->Clicked())
		{
			*open = !*open;
		}

		UILabel(text);

		UIEndBox();
		UIPopId();

		if (out_box) *out_box = box;
	}

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
	return *open;
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

// @CONSTRUCTOR_NOT_CALLED
struct TextEdit
{
	std::vector<char>* buffer;
	int    cursor;

	void FixCursor()
	{
		if (cursor > buffer->size()) cursor = buffer->size() - 1;
		if (cursor < 0) cursor = 0;
	}

	void Backspace()
	{
		int erase_size = 1;
		if (erase_size > cursor) erase_size = cursor;

		memmove(
			buffer->data() + cursor - erase_size,
			buffer->data() + cursor,
			buffer->size() - cursor);

		buffer->resize(buffer->size() - erase_size);
		cursor -= erase_size;
	}

	void Move(int delta)
	{
		cursor += delta;
		FixCursor();
	}

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
	text_edit->FixCursor();

	if (box->Focused())
	{
		box->SetColor(COLOR_BUTTON_ACTIVE);
		if (InputGetButtonDown(SDL_SCANCODE_BACKSPACE)) text_edit->Backspace();
		if (InputGetButtonDown(SDL_SCANCODE_LEFT)) text_edit->Move(-1);
		if (InputGetButtonDown(SDL_SCANCODE_RIGHT)) text_edit->Move(1);
	}
	else
	{
		box->SetColor(COLOR_BUTTON);
	}

	box->event_handler = 
		[](UIBox* box, const UIEvent& event)
		{
			TextEdit* text_edit = (TextEdit*)box->GetData();

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
					text_edit->Insert(event.text.text);
					return true;
				}
				case UIEventType_Draw:
				{
					Font* font = UI->default_font;

					float x = box->position.x + 4;
					float y = box->position.y + (box->size.y - (float)font->pixel_size) / 2.0f;

					DrawText(font, text_edit->buffer->data(), text_edit->buffer->size(), x, y, COLOR_WHITE);

					if (box->Focused())
					{
						float2 cursor_pos = MeasureText(font, text_edit->buffer->data(), text_edit->cursor);
						float cursor_offset = -(font->line_height - font->pixel_size) / 2.0f;
						DrawRectangle(COLOR_WHITE, x + cursor_pos.x, y + cursor_offset, 2, font->line_height);
					}

					return true;
				}
				default: return false;
			}
			return false;
		};

	UIEndBox();

	return box;
}