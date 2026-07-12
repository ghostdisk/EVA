#include <EVA/Core/Basic.hpp>
#include <EVA/Core/Hashing.hpp>
#include <EVA/UI.hpp>
#include <EVA/Renderer/Renderer.hpp>
#include <EVA/Platform.hpp>
#include <EVA/Input.hpp>
#include <EVA/Library.hpp>
#include <SDL3/SDL_events.h>

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

static UIContext g_main_ui = {};
UIContext* UI = &g_main_ui;

void UIInitialize() {
	g_main_ui.default_font = Library::fnt_arial;
}

void UIContextMakeCurrent(UIContext& ui) {
	UI = &ui;
}

UIBox* UIBeginBox(U32 id, int data_size, const void* data_default) {
	UIBox* box = nullptr;
	if (id > 0) {
		id = UIPushId(id);

		for (UIBox* b : UI->all_boxes) {
			if (b->id == id) {
				box = b;
				break;
			}
		}
	}
	if (!box) {
		box = (UIBox*)malloc(sizeof(UIBox) + data_size);
		new (box) UIBox();
		box->flags |= UIBoxFlags_JustCreated;
		UI->all_boxes.push_back(box);

		if (data_default) memcpy((U8*)box + sizeof(UIBox), data_default, data_size);
		else              memset((U8*)box + sizeof(UIBox), 0,            data_size);

	}

	assert(!(box->flags & UIBoxFlags_UsedThisFrame) && "UIBox id Conflict");

	box->id              = id;
	box->flags          |= UIBoxFlags_UsedThisFrame;
	box->next_sibling    = nullptr;
	box->first_child     = nullptr;
	box->last_child      = nullptr;
	box->layout          = &UILayoutMode_Flex;
	box->color           = {0,0,0,0};

	box->parent = UI->box_stack.back();
	if (box->parent->last_child) {
		box->parent->last_child->next_sibling = box;
		assert(box->parent->last_child != box);
		box->parent->last_child = box;
	} else {
		box->parent->first_child = box;
		box->parent->last_child = box;
	}

	UI->box_stack.push_back(box);
	return box;
}

void UIEndBox() {
	UIBox* box = UI->box_stack.back();

	if (box->id > 0) UIPopId();
	UI->box_stack.pop_back();
}

U32 UIPushId(U32 id) {
	UI->id_stack.push_back(UI->id_stack.back() ^ HashU32(id));
	return UI->id_stack.back();
}

U32 UIPushId(const char* str) {
	return UIPushId(HashBytes(str, strlen(str)));
}

U32 UIPushId(const void* ptr) {
	return UIPushId((U32)(uintptr_t)ptr);
}

void UIPopId() {
	UI->id_stack.pop_back();
}

static void Emit(UIBox* box, const UIEvent& event) {
	if (box->event_handler) box->event_handler(box, event);
}

void UIBeginFrame() {
	UI->id_stack = { 0 };
	UI->box_stack = { &UI->root };

	assert(UI->root.next_sibling == nullptr);
	UI->root.first_child = nullptr;
	UI->root.last_child  = nullptr;
	UI->root.size        = g_window_size;
	UI->root.min_size    = g_window_size;
	UI->root.layout      = &UILayoutMode_Fixed;
	if (UI->focus_box && !(UI->focus_box->flags & UIBoxFlags_UsedThisFrame)) UIFocus(nullptr);

	for (int i = 0; i < UI->all_boxes.size(); i++) {
		UIBox* box = UI->all_boxes[i];

		if (box->id && (box->flags & UIBoxFlags_UsedThisFrame)) {
			box->flags &= ~UIBoxFlags_UsedThisFrame;
		} else {
			free(box);
			UI->all_boxes[i] = UI->all_boxes.back();
			UI->all_boxes.pop_back();
			i--;
		}
	}
}

bool UIIsBoxHovered(UIBox* box) {
	return box->position.x <= g_mouse_position.x &&
		box->position.y <= g_mouse_position.y &&
		(box->position.x + box->size.x) > g_mouse_position.x &&
		(box->position.y + box->size.y) > g_mouse_position.y;
}

UIBox* UIFindHoveredChild(UIBox* box) {
	if (!UIIsBoxHovered(box)) {
		return nullptr;
	}

	UIBox* hovered_box = box;
	for (UIBox* child = box->first_child; child; child = child->next_sibling) {
		UIBox* hovered_descendant = UIFindHoveredChild(child);
		if (hovered_descendant) hovered_box = hovered_descendant;
	}

	return hovered_box;
}

void UIEndFrame() {
	UI->root.layout->Pass1(&UI->root);
	UI->root.layout->Pass2(&UI->root);

	bool mouse_down = InputGetButtonDown(INPUT_BUTTON_MOUSE_LEFT);
	bool mouse_up   = InputGetButtonUp(INPUT_BUTTON_MOUSE_LEFT);

	UIBoxFlags flags_to_clear = UIBoxFlags_Hover | UIBoxFlags_Clicked | UIBoxFlags_JustCreated;
	if (mouse_up || mouse_down) flags_to_clear |= UIBoxFlags_Pressed;

	for (UIBox* box : UI->all_boxes) {
		box->flags &= ~flags_to_clear;
	}

	UIBox* hovered_box = UIFindHoveredChild(&UI->root);

	if (mouse_down) {
		UIBox* focus_box = hovered_box;
		while (focus_box && !focus_box->id) {
			focus_box = focus_box->parent;
		}
		UIFocus(focus_box);
	}

	UI->captures_mouse = hovered_box && hovered_box != &UI->root;

	for (UIBox* box = hovered_box; box; box = box->parent) {
		box->flags |= UIBoxFlags_Hover;
		if (mouse_down) {
			box->flags |= UIBoxFlags_Pressed;
		}
		if (mouse_up) {
			box->flags |= UIBoxFlags_Clicked;
		}
	}
}

#define MAIN_AXIS(vec) vec[main_axis]
#define CROSS_AXIS(vec) vec[cross_axis]

void UIFlexLayoutPass1(UIBox* box) {
	int main_axis = box->flex_axis;
	int cross_axis = 1 - main_axis;

	float main_size = 0;
	float cross_size = 0;
	
	float2 padding(
		box->padding_left + box->padding_right,
		box->padding_top + box->padding_bottom);

	for (UIBox* child = box->first_child; child; child = child->next_sibling) {
		child->layout->Pass1(child);

		main_size += MAIN_AXIS(child->size) + box->flex_gap;
		if (cross_size < CROSS_AXIS(child->size)) {
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

void UIFlexLayoutPass2(UIBox* box) {
	int main_axis = box->flex_axis;
	int cross_axis = 1 - main_axis;

	float pos_main = MAIN_AXIS(box->position);
	float pos_cross = CROSS_AXIS(box->position) + (main_axis == 0 ? box->padding_top : box->padding_left);

	float available_space = MAIN_AXIS(box->size);
	float available_cross_space = CROSS_AXIS(box->size);
	float total_flex_grow = 0;
	float total_children_size = 0;
	
	if (main_axis == 0) {
		pos_main += box->padding_left;
		available_space -= box->padding_left + box->padding_right;
		available_cross_space -= box->padding_top + box->padding_bottom;
	} else {
		pos_main += box->padding_top;
		available_space -= box->padding_top + box->padding_bottom;
		available_cross_space -= box->padding_left + box->padding_right;
	}

	for (UIBox* child = box->first_child; child; child = child->next_sibling) {
		total_children_size += MAIN_AXIS(child->size);
		total_flex_grow += child->flex_grow;
		total_children_size += box->flex_gap;
	}
	if (box->first_child) total_children_size -= box->flex_gap; // no gap after last child

	float slack = available_space - total_children_size;

	if (box->main_axis_alignment == UIAlignment_Center) pos_main += slack / 2;
	if (box->main_axis_alignment == UIAlignment_End)    pos_main += slack;

	for (UIBox* child = box->first_child; child; child = child->next_sibling) {
		if (box->main_axis_alignment == UIAlignment_Stretch && child->flex_grow > 0) {
			float ratio = child->flex_grow / total_flex_grow;
			MAIN_AXIS(child->size) += slack * ratio;
		}

		MAIN_AXIS(child->position) = pos_main;
		pos_main += MAIN_AXIS(child->size) + box->flex_gap;

		switch (box->cross_axis_alignment) {
			case UIAlignment_Start: {
				CROSS_AXIS(child->position) = pos_cross;
				break;
			}
			case UIAlignment_Center: {
				CROSS_AXIS(child->position) = pos_cross + (available_cross_space - CROSS_AXIS(child->size)) / 2;
				break;
			}
			case UIAlignment_End: {
				CROSS_AXIS(child->position) = pos_cross + (available_cross_space - CROSS_AXIS(child->size));
				break;
			}
			case UIAlignment_Stretch: {
				CROSS_AXIS(child->position) = pos_cross;
				CROSS_AXIS(child->size) = available_cross_space;
				break;
			}
		}
	}

	for (UIBox* child = box->first_child; child; child = child->next_sibling) {
		child->layout->Pass2(child);
	}
}

void UITextLayoutPass1(UIBox* box) {
	box->size = MeasureText(box->font, box->text);
	box->size.x += box->padding_left + box->padding_right;
	box->size.y += box->padding_top + box->padding_bottom;
}

void UITextLayoutPass2(UIBox* box) {
}

void UIFixedLayoutPass1(UIBox* box) {
	for (UIBox* child = box->first_child; child; child = child->next_sibling) {
		child->layout->Pass1(child);
	}
}

void UIFixedLayoutPass2(UIBox* box) {
	for (UIBox* child = box->first_child; child; child = child->next_sibling) {
		child->layout->Pass2(child);
	}
}

void UIDrawBoxRecursive(UIBox* box) {
	if (box->color.w && box->layout != &UILayoutMode_Text) { // TODO: dumb.
		if (box->background_sprite) {
			DrawSprite(box->background_sprite, box->position.x, box->position.y, box->color);
		} else {
			DrawRectangle(box->color, box->position.x, box->position.y, box->size.x, box->size.y);
		}
	}

	if (box->text) {
		DrawText(box->font, box->text, -1, box->position.x + box->padding_left, box->position.y + box->padding_top, box->color);
	}

	if (box->event_handler) {
		Emit(box, UIEvent{
			.type = UIEventType_Draw,
		});
	}

	for (UIBox* child = box->first_child; child; child = child->next_sibling) {
		UIDrawBoxRecursive(child);
	}
}

void UIDraw() {
	DrawSetLayer(Layer_UI);
	UIDrawBoxRecursive(&UI->root);
}

void UIFocus(UIBox* box) {
	if (UI->focus_box) {
		UI->focus_box->flags &= ~UIBoxFlags_Focus;
		Emit(UI->focus_box, UIEvent{
			.type = UIEventType_Unfocus,
		});
	}
	UI->focus_box = box;
	if (box) {
		assert(box->id);
		UI->focus_box->flags |= UIBoxFlags_Focus;
		Emit(box, UIEvent{
			.type = UIEventType_Focus,
		});
	}
}

bool UIProcessSDLEvent(SDL_Event* event) {
	switch (event->type) {
		case SDL_EVENT_TEXT_INPUT: {
			if (UI->focus_box) {
				Emit(UI->focus_box, UIEvent{
					.type = UIEventType_Text,
					.text = {
						.text = event->text.text,
					},
				});
			}
			break;
		}
	}
	return false;
}

bool UICapturesMouse() {
	return UI->captures_mouse;
}