#pragma once
#include <EVA/GL.hpp>
#include <EVA/Math.hpp>

void RendererInitialize();
void RenderPendingLines();

void DrawLine(float3 a, float3 b, float4 color);
void DrawGrid(int size);