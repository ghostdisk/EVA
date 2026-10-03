#pragma once
#include <EVA/Core/Common.hpp>
#include <EVA/Core/Atom.hpp>

// What the shader compiler (Script) and the GPU layer share: which API a shader is compiled for, the compiled shaders
// the GPU layer creates pipelines from, and their reflection. See Docs/Plan/Bindings.md (5).

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

// The entry function's name in every compiled shader. Metal doesn't allow main, so MSL's is main0, like SPIRV-Cross's.
inline constexpr const char* ENTRY_POINT_NAME = "main";
inline constexpr const char* MSL_ENTRY_POINT_NAME = "main0";

inline constexpr uint32 MAX_BIND_GROUPS = 4;

struct D3DRegisters
{
	uint32 cbv = 0; // b
	uint32 srv = 0; // t
	uint32 uav = 0; // u
	uint32 sampler = 0; // s
};

// An amount in every layout unit of one target: a type's size, a field's offset relative to its struct, an array's
// stride. Which member of the union is valid follows from the backend the shader was compiled for.
struct LayoutAmount
{
	uint32 bytes = 0;
	uint32 binding_ranges = 0; // resource leaves, an array of resources being one; the same on every target
	union
	{
		struct
		{
			uint32 binding;
		} vulkan;
		D3DRegisters d3d = {}; // one register per array element
		struct
		{
			uint32 id;
		} metal; // one id per array element
	};
};

enum class ScalarKind : uint8
{
	BOOL,
	INT,
	UINT,
	FLOAT,
};

enum class ReflectedTypeKind : uint8
{
	SCALAR,
	VECTOR,
	MATRIX,
	ARRAY,
	STRUCT,
};

struct VarLayout;

// A type laid out by the rules of the target a shader was compiled for.
struct TypeLayout
{
	ReflectedTypeKind kind = ReflectedTypeKind::SCALAR;
	ScalarKind scalar = ScalarKind::FLOAT; // SCALAR, VECTOR, MATRIX
	uint32 columns = 1;                    // VECTOR: components. MATRIX: columns, each a vector of rows, column-major
	uint32 rows = 1;                       // MATRIX
	uint32 length = 0;                     // ARRAY
	Atom name = Atom::NONE;                // STRUCT
	LayoutAmount size;
	uint32 alignment = 1; // bytes
	LayoutAmount stride;  // ARRAY
	const TypeLayout* element = nullptr; // ARRAY
	Slice<VarLayout> fields;             // STRUCT, in declaration order
};

struct VarLayout
{
	Atom name = Atom::NONE;
	const TypeLayout* type = nullptr;
	LayoutAmount offset; // relative to the enclosing struct
};

enum class BindingKind : uint8
{
	UNIFORM_BUFFER, // the group's implicit one
};

// One resource of a group, or its implicit uniform buffer: what it takes and where it goes.
struct BindingRange
{
	BindingKind kind = BindingKind::UNIFORM_BUFFER;
	uint32 count = 1;       // descriptors, for arrays of resources
	uint32 buffer_size = 0; // UNIFORM_BUFFER: the bytes the shader reads
	LayoutAmount offset;    // within the group
};

// What a group holds. Equal layouts are compatible whatever the struct is named.
struct BindGroupLayout
{
	Slice<BindingRange> ranges; // the implicit uniform buffer first if there's plain data, then the resources
	uint32 uniform_size = 0;    // bytes of the implicit uniform buffer
	uint64 hash = 0;            // of the ranges and size
};

// An @bind_group(n) let.
struct ReflectedBindGroup
{
	uint32 index = 0;
	Atom name = Atom::NONE;
	const TypeLayout* type = nullptr; // its STRUCT
	LayoutAmount element_offset;      // where the struct's offsets start: after the implicit uniform buffer, if any
	BindGroupLayout layout;
};

// One @entry(...) function compiled for a backend. Its code holds only what the entry point uses.
struct CompiledEntryPoint
{
	ShaderStage stage = ShaderStage::VERTEX;
	Atom name = Atom::NONE; // the user's name, e.g. VSMain
	Slice<uint8> code;      // VULKAN: SPIR-V words. D3D11: HLSL text, zero terminated (not counted), compiled with fxc
	                        // when the pipeline is created. METAL: MSL text the same way, compiled by Metal
	uint32 bind_groups = 0; // bit per group index the entry point reads
	// D3D11, which has no register spaces: each group's first register of each class. A group's registers are these
	// plus its ranges' offsets.
	D3DRegisters d3d11_bind_group_registers[MAX_BIND_GROUPS];
};

}
