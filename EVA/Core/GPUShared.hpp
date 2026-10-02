#pragma once
#include <EVA/Core/Common.hpp>
#include <EVA/Core/Atom.hpp>

// What the shader compiler (Script) and the GPU layer share: which API a shader is compiled for, and the compiled
// shaders the GPU layer creates pipelines from. Shader reflection goes here too once there is some.

namespace EVA::GPU
{

enum class Backend
{
	NONE = 0,
	D3D11,
	VULKAN,
	METAL,
};

// The values of the language's ShaderStage enum, the argument of @entry(...).
enum class ShaderStage : uint8
{
	VERTEX,
	FRAGMENT,
};

// The entry function's name in every compiled shader.
inline constexpr const char* ENTRY_POINT_NAME = "main";

// One @entry(...) function compiled for a backend. Its code holds only what the entry point uses.
struct CompiledEntryPoint
{
	ShaderStage stage = ShaderStage::VERTEX;
	Atom name = Atom::NONE; // the user's name, e.g. VSMain
	Slice<uint8> code;      // VULKAN: SPIR-V words. D3D11: HLSL text, zero terminated (not counted), compiled with fxc
	                        // when the pipeline is created
};

}
