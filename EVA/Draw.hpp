#pragma once
#include <EVA/Math.hpp>
#include <vector>

struct Font
{
};

struct DrawQuad
{
	float4 position_rect;
};


struct DrawContext
{
	std::vector<DrawQuad> quads;
};

void DrawInitialize();
void DrawContextInit(DrawContext& dc);
void DrawRender(DrawContext& dc);
