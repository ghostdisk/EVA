#include <EVA/UI.hpp>

void UISetPadding(UIBox* box, int padding)
{
	assert(box->layout == &UILayoutMode_Flex);
	box->padding_top    = padding;
	box->padding_right  = padding;
	box->padding_bottom = padding;
	box->padding_left   = padding;
}

void UISetPadding(UIBox* box, int vpadding, int hpadding)
{
	assert(box->layout == &UILayoutMode_Flex);
	box->padding_top    = vpadding;
	box->padding_right  = hpadding;
	box->padding_bottom = vpadding;
	box->padding_left   = hpadding;
}

void UISetPadding(UIBox* box, int top, int right, int bottom, int left)
{
	assert(box->layout == &UILayoutMode_Flex);
	box->padding_top    = top;
	box->padding_right  = right;
	box->padding_bottom = bottom;
	box->padding_left   = left;
}

void UISetGap(UIBox* box, int gap)
{
	assert(box->layout == &UILayoutMode_Flex);
	box->flex_gap = gap;
}

void UISetSize(UIBox* box, float width, float height)
{
	assert(box->layout == &UILayoutMode_Flex);
	box->min_size = { width, height };
}

void UISetBackgroundSprite(UIBox* box, Sprite* sprite)
{
	box->background_sprite = sprite;
}

void* UIBoxGetData(UIBox* box)
{
	return (U8*)box + sizeof(UIBox);
}

void UISetFlex(UIBox* box, UIAxis axis, UIAlignment main, UIAlignment cross)
{
	assert(box->layout == &UILayoutMode_Flex);
	box->flex_axis = axis;
	box->main_axis_alignment = main;
	box->cross_axis_alignment = cross;
}

void UISetMainAxisAlignment(UIBox* box, UIAlignment alignment)
{
	assert(box->layout == &UILayoutMode_Flex);
	box->main_axis_alignment = alignment;
}

void UISetCrossAxisAlignment(UIBox* box, UIAlignment alignment)
{
	assert(box->layout == &UILayoutMode_Flex);
	box->cross_axis_alignment = alignment;
}