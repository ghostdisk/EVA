#include <EVA/UI.hpp>

UIBox* UIBox::SetPadding(int padding)
{
	this->padding_top    = padding;
	this->padding_right  = padding;
	this->padding_bottom = padding;
	this->padding_left   = padding;
	return this;
}

UIBox* UIBox::SetPadding(int vpadding, int hpadding)
{
	this->padding_top    = vpadding;
	this->padding_right  = hpadding;
	this->padding_bottom = vpadding;
	this->padding_left   = hpadding;
	return this;
}

UIBox* UIBox::SetPadding(int top, int right, int bottom, int left)
{
	this->padding_top    = top;
	this->padding_right  = right;
	this->padding_bottom = bottom;
	this->padding_left   = left;
	return this;
}

UIBox* UIBox::SetGap(int gap)
{
	this->flex_gap = gap;
	return this;
}

UIBox* UIBox::SetSize(float width, float height)
{
	assert(this->layout == &UILayoutMode_Flex);
	this->min_size = { width, height };
	return this;
}

UIBox* UIBox::SetPosition(float x, float y)
{
	assert(this->layout == &UILayoutMode_Flex);
	this->position = { x, y };
	return this;
}

UIBox *UIBox::SetColor(float4 color)
{
	this->color = color;
	return this;
}

UIBox* UIBox::SetPosition(float2 position)
{
	return SetPosition(position.x, position.y);
}

UIBox* UIBox::SetBackgroundSprite(Sprite* sprite)
{
	this->background_sprite = sprite;
	return this;
}

void* UIBox::GetData()
{
	return (U8*)this + sizeof(UIBox);
}

UIBox* UIBox::SetFlex(UIAxis axis, UIAlignment main, UIAlignment cross)
{
	assert(this->layout == &UILayoutMode_Flex);
	this->flex_axis = axis;
	this->main_axis_alignment = main;
	this->cross_axis_alignment = cross;
	return this;
}

UIBox* UIBox::SetFlexGrow(float grow)
{
	this->flex_grow = grow;
	return this;
}


UIBox* UIGetCurrentBox()
{
	return UI->box_stack.back();
}
