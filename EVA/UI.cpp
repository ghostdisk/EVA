#include <EVA/UI.hpp>
#include <EVA/GL.hpp>
#include <EVA/Draw.hpp>
#include <EVA/Hashing.hpp>
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
	UIBox* box = nullptr;
	if (id > 0)
	{
		id = UIPushId(ui, id);

		for (UIBox* b : ui.all_boxes)
		{
			if (b->id == id)
			{
				box = b;
				break;
			}
		}
	}
	if (!box) box = new UIBox();

	box->id              = id;
	box->used_this_frame = true;
	box->next_sibling    = nullptr;
	box->first_child     = nullptr;
	box->last_child      = nullptr;
	box->layout          = &UILayoutMode_Flex;
	box->color           = {0,0,0,0};

	box->parent = ui.box_stack.back();
	if (box->parent->last_child)
	{
		box->parent->last_child->next_sibling = box;
		box->parent->last_child = box;
	}
	else
	{
		box->parent->first_child = box;
		box->parent->last_child = box;
	}

	ui.box_stack.push_back(box);
	return box;
}

void UIEndBox(UIContext& ui)
{
	UIBox* box = ui.box_stack.back();

	if (box->id > 0) UIPopId(ui);
	ui.box_stack.pop_back();
}

U32 UIPushId(UIContext& ui, U32 id)
{
	ui.id_stack.push_back(ui.id_stack.back() ^ HashU32(id));
	return ui.id_stack.back();
}

U32 UIPushId(UIContext& ui, const char* str)
{
	return UIPushId(ui, HashBytes(str, strlen(str)));
}

void UIPopId(UIContext& ui)
{
	ui.id_stack.pop_back();
}

void UIBeginFrame(UIContext& ui)
{
	ui.id_stack = { 0 };
	ui.box_stack = { &ui.root };

	ui.root.first_child = nullptr;
	ui.root.last_child  = nullptr;
	ui.root.size        = float2(WindowWidth, WindowHeight);
	ui.root.min_size    = float2(WindowWidth, WindowHeight);
	ui.root.layout      = &UILayoutMode_Flex;
}


void UIEndFrame(UIContext& ui)
{
	ui.root.layout->Pass1(&ui.root);
	ui.root.layout->Pass2(&ui.root);
}


#define MAIN_AXIS(vec) vec[main_axis]
#define CROSS_AXIS(vec) vec[cross_axis]

void UIFlexLayoutPass1(UIBox* box)
{
	int main_axis = box->flex_axis;
	int cross_axis = 1 - main_axis;

	float main_size = 0;
	float cross_size = 0;
	
	float2 padding(
		box->padding_left + box->padding_right,
		box->padding_top + box->padding_bottom);

	for (UIBox* child = box->first_child; child; child = child->next_sibling)
	{
		child->layout->Pass1(child);

		main_size += MAIN_AXIS(child->size);
		if (cross_size < CROSS_AXIS(child->size))
		{
			cross_size = CROSS_AXIS(child->size);
		}
	}

	main_size += MAIN_AXIS(padding);
	cross_size += CROSS_AXIS(padding);

	if (main_size  < MAIN_AXIS(box->min_size))  main_size  = MAIN_AXIS(box->min_size);
	if (cross_size < CROSS_AXIS(box->min_size)) cross_size = CROSS_AXIS(box->min_size);

	MAIN_AXIS(box->size) = main_size;
	CROSS_AXIS(box->size) = cross_size;
}

void UIFlexLayoutPass2(UIBox* box)
{
	int main_axis = box->flex_axis;
	int cross_axis = 1 - main_axis;

	float pos_main = MAIN_AXIS(box->position) + main_axis == 0 ? box->padding_left : box->padding_top;
	float pos_cross = CROSS_AXIS(box->position) + (main_axis == 0 ? box->padding_top : box->padding_left);

	for (UIBox* child = box->first_child; child; child = child->next_sibling)
	{
		MAIN_AXIS(child->position) = pos_main;
		CROSS_AXIS(child->position) = pos_cross;

		child->layout->Pass2(child);
		pos_main += MAIN_AXIS(child->size) + box->flex_gap;
	}

	for (UIBox* child = box->first_child; child; child = child->next_sibling)
	{
		child->layout->Pass2(child);
	}
}

void UITextLayoutPass1(UIBox* box)
{
	box->size = float2(MeasureText(box->font, box->text), box->font->yoffset);
}

void UITextLayoutPass2(UIBox* box)
{
}

void UIDrawBoxRecursive(UIContext& ui, DrawContext& dc, UIBox* box)
{
	if (box->color.w && box->layout != &UILayoutMode_Text)
	{
		DrawRectangle(dc, box->position.x, box->position.y, box->size.x, box->size.y, box->color);
	}

	if (box->text)
	{
		DrawText(dc, box->font, box->text, box->position.x, box->position.y, box->color);
	}

	for (UIBox* child = box->first_child; child; child = child->next_sibling)
	{
		UIDrawBoxRecursive(ui, dc, child);
	}
}

UIBox* UILabel(UIContext& ui, const char* text)
{
	UIBox* box = UIBeginBox(ui, 0);
	box->layout = &UILayoutMode_Text;
	box->text = text; // TODO - use arena for text storage!!!
	box->color = {1,1,1,1};
	box->font = ui.default_font;
	UIEndBox(ui);
	return box;
}

void UIDraw(UIContext& ui, DrawContext& dc)
{
	UIDrawBoxRecursive(ui, dc, &ui.root);
}

UILayoutMode UILayoutMode_Flex = {
	.Pass1 = UIFlexLayoutPass1,
	.Pass2 = UIFlexLayoutPass2,
};

UILayoutMode UILayoutMode_Text = {
	.Pass1 = UITextLayoutPass1,
	.Pass2 = UITextLayoutPass2,
};


void UISetPadding(UIBox* box, int padding)
{
	box->padding_top    = padding;
	box->padding_right  = padding;
	box->padding_bottom = padding;
	box->padding_left   = padding;

}

void UISetPadding(UIBox* box, int vpadding, int hpadding)
{
	box->padding_top    = vpadding;
	box->padding_right  = hpadding;
	box->padding_bottom = vpadding;
	box->padding_left   = hpadding;
}

void UISetPadding(UIBox* box, int top, int right, int bottom, int left)
{
	box->padding_top    = top;
	box->padding_right  = right;
	box->padding_bottom = bottom;
	box->padding_left   = left;
}

void UISetGap(UIBox* box, int gap)
{
	box->flex_gap = gap;
}