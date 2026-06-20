#pragma once
#include <EVA/GL.hpp>
#include <EVA/Math.hpp>

void RendererInitialize();
void RendererBeginFrame();
void RenderScene();

void DrawLine(float3 a, float3 b, float4 color);
void DrawGrid(int size);

void DrawMesh(Mesh* mesh, Material* material, const float4x4& matrix, float4 color);
void DrawAABB(float3 center, float3 size, float4 color);
void DrawPoint(float3 point, float4 color);