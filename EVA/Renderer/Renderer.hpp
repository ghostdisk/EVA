#pragma once
#include <EVA/Renderer/GL.hpp>
#include <EVA/Math.hpp>

struct Sprite;
struct Font;

enum Layer
{
	Layer_Sky     = 0,
	Layer_Main    = 1,
	Layer_Overlay = 2,
	Layer_UI      = 3,

	Layer_ENUM_SIZE,
};

enum DrawQuadMode
{
	DrawQuadMode_SolidColor,
	DrawQuadMode_Text,
	DrawQuadMode_Sprite,
};

struct DrawQuad
{
	int mode;
	int pad0;
	int pad1;
	int pad2;
	float4 position_rect;
	float4 uv_rect;
	float4 tint;
};

struct DrawQuadRecord
{
	DrawQuadMode mode          = DrawQuadMode_SolidColor;
	Texture*     texture       = nullptr;
	float4       position_rect = {};
	float4       uv_rect       = {};
	float4       tint          = { 1,1,1,1 };
};

void RendererInitialize();
void RenderFrame();

void DrawSetLayer(Layer layer);
void DrawLine(float3 a, float3 b, float4 color = COLOR_WHITE);
void DrawMesh(Mesh* mesh, Material* material, const float4x4& matrix, float4 color = COLOR_WHITE);
void DrawRectangle(float4 color, int x, int y, int w, int h);
void DrawText(Font* font, const char* text, int len, int x, int y, float4 color);
void DrawSprite(Sprite* sprite, int x, int y, float4 color);

float2 MeasureText(Font* font, const char* text, int text_len = -1);

void DrawPoint(float3 point, float4 color = COLOR_WHITE);
void DrawBox(float3 c1, float3 c2, float4 color = COLOR_WHITE);
void DrawAABB(float3 center, float3 size, float4 color = COLOR_WHITE);

Material* MaterialCreate(const char* name, Shader* shader, Texture* texture);

extern Shader* shd_main;
extern Shader* shd_brush;