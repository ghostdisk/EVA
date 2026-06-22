#include <EVA/UI.hpp>
#include <EVA/GL.hpp>
#include <EVA/Draw.hpp>
#include <EVA/Hashing.hpp>
#include <EVA/Platform.hpp>
#include <EVA/IO.hpp>
#include <EVA/Arena.hpp>

void UIFlexLayoutPass1(UIBox* box);
void UIFlexLayoutPass2(UIBox* box);
void UITextLayoutPass1(UIBox* box);
void UITextLayoutPass2(UIBox* box);
void UIFixedLayoutPass1(UIBox* box);
void UIFixedLayoutPass2(UIBox* box);

UILayoutMode UILayoutMode_Flex = {
	.Pass1 = UIFlexLayoutPass1,
	.Pass2 = UIFlexLayoutPass2,
};

UILayoutMode UILayoutMode_Text = {
	.Pass1 = UITextLayoutPass1,
	.Pass2 = UITextLayoutPass2,
};

UILayoutMode UILayoutMode_Fixed = {
	.Pass1 = UIFixedLayoutPass1,
	.Pass2 = UIFixedLayoutPass2,
};


void UIInitialize()
{
}

void UIContextInit(UIContext& ui, Font* default_font)
{
	ui.default_font = default_font;
}

UIBox* UIBeginBox(UIContext& ui, U32 id, int data_size, const void* data_default)
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
	if (!box)
	{
		box = (UIBox*)malloc(sizeof(UIBox) + data_size);
		new (box) UIBox();
		ui.all_boxes.push_back(box);
		memcpy((U8*)box + sizeof(UIBox), data_default, data_size);
	}

	assert(!(box->flags & UIBoxFlags_UsedThisFrame) && "UIBox id Conflict");

	box->id              = id;
	box->flags          |= UIBoxFlags_UsedThisFrame;
	box->next_sibling    = nullptr;
	box->first_child     = nullptr;
	box->last_child      = nullptr;
	box->layout          = &UILayoutMode_Flex;
	box->color           = {0,0,0,0};

	box->parent = ui.box_stack.back();
	if (box->parent->last_child)
	{
		box->parent->last_child->next_sibling = box;
		assert(box->parent->last_child != box);
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

U32 UIPushId(UIContext& ui, const void* ptr)
{
	return UIPushId(ui, (U32)(uintptr_t)ptr);
}

void UIPopId(UIContext& ui)
{
	ui.id_stack.pop_back();
}

void UIBeginFrame(UIContext& ui)
{
	ui.id_stack = { 0 };
	ui.box_stack = { &ui.root };

	assert(ui.root.next_sibling == nullptr);
	ui.root.first_child = nullptr;
	ui.root.last_child  = nullptr;
	ui.root.size        = float2(WindowWidth, WindowHeight);
	ui.root.min_size    = float2(WindowWidth, WindowHeight);
	ui.root.layout      = &UILayoutMode_Fixed;

	for (int i = 0; i < ui.all_boxes.size(); i++)
	{
		UIBox* box = ui.all_boxes[i];

		if (box->id && (box->flags & UIBoxFlags_UsedThisFrame))
		{
			box->flags &= ~UIBoxFlags_UsedThisFrame;
		}
		else
		{
			free(box);
			ui.all_boxes[i] = ui.all_boxes.back();
			ui.all_boxes.pop_back();
			i--;
		}
	}
}

bool UIIsBoxHovered(UIBox* box)
{
	return box->position.x <= IOMousePosition.x &&
		box->position.y <= IOMousePosition.y &&
		(box->position.x + box->size.x) > IOMousePosition.x &&
		(box->position.y + box->size.y) > IOMousePosition.y;
}

UIBox* UIFindHoveredChild(UIBox* box)
{
	if (!UIIsBoxHovered(box))
	{
		return nullptr;
	}

	UIBox* hovered_box = box;
	for (UIBox* child = box->first_child; child; child = child->next_sibling)
	{
		UIBox* hovered_descendant = UIFindHoveredChild(child);
		if (hovered_descendant) hovered_box = hovered_descendant;
	}

	return hovered_box;
}

void UIEndFrame(UIContext& ui)
{
	ui.root.layout->Pass1(&ui.root);
	ui.root.layout->Pass2(&ui.root);

	bool mouse_down = IOGetButtonDown(IO_BUTTON_MOUSE_LEFT);
	bool mouse_up   = IOGetButtonUp(IO_BUTTON_MOUSE_LEFT);

	UIBoxFlags flags_to_clear = UIBoxFlags_Hover | UIBoxFlags_Clicked;
	if (mouse_up || mouse_down) flags_to_clear |= UIBoxFlags_Pressed;

	for (UIBox* box : ui.all_boxes)
	{
		box->flags &= ~flags_to_clear;
	}

	UIBox* hovered_box = UIFindHoveredChild(&ui.root);
	for (UIBox* box = hovered_box; box; box = box->parent)
	{
		box->flags |= UIBoxFlags_Hover;
		if (mouse_down)
		{
			box->flags |= UIBoxFlags_Pressed;
		}

		if (mouse_up)
		{
			box->flags |= UIBoxFlags_Clicked;
		}
	}
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

		main_size += MAIN_AXIS(child->size) + box->flex_gap;
		if (cross_size < CROSS_AXIS(child->size))
		{
			cross_size = CROSS_AXIS(child->size);
		}
	}
	if (box->first_child) main_size -= box->flex_gap; // no gap after last child

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

	float pos_main = MAIN_AXIS(box->position);
	float pos_cross = CROSS_AXIS(box->position) + (main_axis == 0 ? box->padding_top : box->padding_left);

	float available_space = MAIN_AXIS(box->size);
	float available_cross_space = CROSS_AXIS(box->size);
	float total_flex_grow = 0;
	float total_children_size = 0;
	
	if (main_axis == 0)
	{
		pos_main += box->padding_left;
		available_space -= box->padding_left + box->padding_right;
		available_cross_space -= box->padding_top + box->padding_bottom;
	}
	else
	{
		pos_main += box->padding_top;
		available_space -= box->padding_top + box->padding_bottom;
		available_cross_space -= box->padding_left + box->padding_right;
	}

	for (UIBox* child = box->first_child; child; child = child->next_sibling)
	{
		total_children_size += MAIN_AXIS(child->size);
		total_flex_grow += child->flex_grow;
		total_children_size += box->flex_gap;
	}
	if (box->first_child) total_children_size -= box->flex_gap; // no gap after last child

	float slack = available_space - total_children_size;

	if (box->main_axis_alignment == UIAlignment_Center) pos_main += slack / 2;
	if (box->main_axis_alignment == UIAlignment_End)    pos_main += slack;

	for (UIBox* child = box->first_child; child; child = child->next_sibling)
	{
		if (box->main_axis_alignment == UIAlignment_Stretch && child->flex_grow > 0)
		{
			float ratio = child->flex_grow / total_flex_grow;
			MAIN_AXIS(child->size) += slack * ratio;
		}

		MAIN_AXIS(child->position) = pos_main;
		pos_main += MAIN_AXIS(child->size) + box->flex_gap;

		switch (box->cross_axis_alignment)
		{
			case UIAlignment_Start:
			{
				CROSS_AXIS(child->position) = pos_cross;
				break;
			}
			case UIAlignment_Center:
			{
				CROSS_AXIS(child->position) = pos_cross + (available_cross_space - CROSS_AXIS(child->size)) / 2;
				break;
			}
			case UIAlignment_End:
			{
				CROSS_AXIS(child->position) = pos_cross + (available_cross_space - CROSS_AXIS(child->size));
				break;
			}
			case UIAlignment_Stretch:
			{
				CROSS_AXIS(child->position) = pos_cross;
				CROSS_AXIS(child->size) = available_cross_space;
				break;
			}
		}
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

void UIFixedLayoutPass1(UIBox* box)
{
}

void UIFixedLayoutPass2(UIBox* box)
{
}

void UIDrawBoxRecursive(UIContext& ui, DrawContext& dc, UIBox* box)
{
	if (box->color.w && box->layout != &UILayoutMode_Text) // TODO: Dumb.
	{
		if (box->background_sprite)
		{
			DrawSprite(dc, box->background_sprite, box->position.x, box->position.y, box->color);
		}
		else
		{
			DrawRectangle(dc, box->color, box->position.x, box->position.y, box->size.x, box->size.y);
		}
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

void UIDraw(UIContext& ui, DrawContext& dc)
{
	UIDrawBoxRecursive(ui, dc, &ui.root);
}
