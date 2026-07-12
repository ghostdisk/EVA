#pragma once
#include <EVA/Core/Basic.hpp>
#include <EVA/Math.hpp>

class Mesh;
class Material;
class Texture;
class Shader;
struct MeshVertex;

void GLPreInitialize();
void GLInitialize();
Shader* GLCompileShader(const char* name, int num_defines = 0, const char** defines = nullptr);
