#include <EVA/Script/Test/OutputValidation.hpp>

#ifdef EVA_HAVE_SPIRV_TOOLS
#include <spirv-tools/libspirv.h>
#endif

#ifdef EVA_WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3dcompiler.h>
#include <d3d11shader.h>
#endif

#ifdef EVA_MACOS
#include <Metal.hpp>
#endif

namespace EVA::Script::Validation
{

bool HaveSPIRVTools()
{
#ifdef EVA_HAVE_SPIRV_TOOLS
	return true;
#else
	return false;
#endif
}

bool HaveFXC()
{
#ifdef EVA_WIN32
	return true;
#else
	return false;
#endif
}

#ifdef EVA_MACOS
static MTL::Device* MetalDevice()
{
	static MTL::Device* device = MTL::CreateSystemDefaultDevice();
	return device;
}
#endif

bool HaveMetal()
{
#ifdef EVA_MACOS
	return MetalDevice() != nullptr;
#else
	return false;
#endif
}

ZTStringView ValidateSPIRV(Slice<uint32> words, Arena* arena)
{
#ifdef EVA_HAVE_SPIRV_TOOLS
	static spv_context context = spvContextCreate(SPV_ENV_VULKAN_1_0);
	spv_diagnostic diagnostic = nullptr;
	spv_result_t result = spvValidateBinary(context, words.data, words.count, &diagnostic);
	ZTStringView error;
	if (result != SPV_SUCCESS)
		error = aprintf(arena, "%s", diagnostic && diagnostic->error ? diagnostic->error : "invalid, no message");
	spvDiagnosticDestroy(diagnostic);
	return error;
#else
	(void)words;
	(void)arena;
	return {};
#endif
}

ZTStringView DisassembleSPIRVWithTools(Slice<uint32> words, Arena* arena)
{
#ifdef EVA_HAVE_SPIRV_TOOLS
	static spv_context context = spvContextCreate(SPV_ENV_VULKAN_1_0);
	spv_text text = nullptr;
	spv_diagnostic diagnostic = nullptr;
	ZTStringView result;
	if (spvBinaryToText(context, words.data, words.count, SPV_BINARY_TO_TEXT_OPTION_NO_HEADER, &text, &diagnostic) == SPV_SUCCESS)
		result = InternString(arena, StringView(text->str, text->length));
	else
		result = aprintf(arena, "can't disassemble: %s", diagnostic && diagnostic->error ? diagnostic->error : "no message");
	spvTextDestroy(text);
	spvDiagnosticDestroy(diagnostic);
	return result;
#else
	(void)words;
	(void)arena;
	return {};
#endif
}

ZTStringView CompileHLSL(StringView text, ShaderStage stage, Arena* arena)
{
#ifdef EVA_WIN32
	const char* profile = stage == ShaderStage::VERTEX ? "vs_5_0" : "ps_5_0";
	ID3DBlob* code = nullptr;
	ID3DBlob* errors = nullptr;
	HRESULT result = D3DCompile(text.data, text.length, "shader.hlsl", nullptr, nullptr, "main", profile, 0, 0, &code, &errors);
	ZTStringView error;
	if (FAILED(result))
	{
		const char* message = errors ? (const char*)errors->GetBufferPointer() : "failed without a message";
		error = aprintf(arena, "%s", message);
	}
	if (code)
		code->Release();
	if (errors)
		errors->Release();
	return error;
#else
	(void)text;
	(void)stage;
	(void)arena;
	return {};
#endif
}

#ifdef EVA_WIN32
// Where fxc's reflection of a type differs from our layout, or empty.
static ZTStringView CompareTypes(ID3D11ShaderReflectionType* d3d, const GPU::TypeLayout* ours, const char* path, Arena* arena)
{
	D3D11_SHADER_TYPE_DESC desc;
	d3d->GetDesc(&desc);
	// HLSL's arrays of arrays are one array of all their elements in fxc's reflection.
	uint32 elements = 0;
	while (ours->kind == GPU::ReflectedTypeKind::ARRAY)
	{
		elements = (elements ? elements : 1) * ours->length;
		ours = ours->element;
	}
	if (desc.Elements != elements)
		return aprintf(arena, "%s: fxc has %u elements, we have %u", path, desc.Elements, elements);
	switch (ours->kind)
	{
	case GPU::ReflectedTypeKind::SCALAR:
		if (desc.Class != D3D_SVC_SCALAR)
			return aprintf(arena, "%s: fxc's class is %u, we have a scalar", path, (uint32)desc.Class);
		return {};
	case GPU::ReflectedTypeKind::VECTOR:
		if (desc.Class != D3D_SVC_VECTOR || desc.Columns != ours->columns)
			return aprintf(arena, "%s: fxc's class is %u with %u columns, we have a vector of %u", path, (uint32)desc.Class,
				desc.Columns, ours->columns);
		return {};
	case GPU::ReflectedTypeKind::MATRIX:
		// row_major, with HLSL's rows being our columns.
		if (desc.Class != D3D_SVC_MATRIX_ROWS || desc.Rows != ours->columns || desc.Columns != ours->rows)
			return aprintf(arena, "%s: fxc's class is %u, %ux%u, we have %u columns of %u", path, (uint32)desc.Class, desc.Rows,
				desc.Columns, ours->columns, ours->rows);
		return {};
	case GPU::ReflectedTypeKind::STRUCT:
	{
		if (desc.Class != D3D_SVC_STRUCT || desc.Members != ours->fields.count)
			return aprintf(arena, "%s: fxc's class is %u with %u members, we have a struct of %u", path, (uint32)desc.Class,
				desc.Members, ours->fields.count);
		for (uint32 i = 0; i < ours->fields.count; ++i)
		{
			const GPU::VarLayout& field = ours->fields[i];
			ZTStringView field_path = aprintf(arena, "%s.%s", path, GetAtomString(field.name, arena).CString());
			ID3D11ShaderReflectionType* member = d3d->GetMemberTypeByIndex(i);
			D3D11_SHADER_TYPE_DESC member_desc;
			member->GetDesc(&member_desc);
			if (member_desc.Offset != field.offset.bytes)
				return aprintf(arena, "%s: fxc's offset is %u, ours is %u", field_path.CString(), member_desc.Offset,
					field.offset.bytes);
			ZTStringView problem = CompareTypes(member, field.type, field_path.CString(), arena);
			if (problem.length)
				return problem;
		}
		return {};
	}
	case GPU::ReflectedTypeKind::ARRAY: break;
	}
	return {};
}
#endif

ZTStringView CheckHLSLBindGroups(const GPU::CompiledEntryPoint& entry_point, Slice<GPU::ReflectedBindGroup> bind_groups,
	Arena* arena)
{
#ifdef EVA_WIN32
	const char* profile = entry_point.stage == ShaderStage::VERTEX ? "vs_5_0" : "ps_5_0";
	ID3DBlob* code = nullptr;
	// Unoptimized, so fxc keeps cbuffers whose values are dead.
	HRESULT result = D3DCompile(entry_point.code.data, entry_point.code.count, "shader.hlsl", nullptr, nullptr, "main",
		profile, D3DCOMPILE_SKIP_OPTIMIZATION, 0, &code, nullptr);
	if (FAILED(result))
		return {}; // CompileHLSL reports it
	DEFER(code->Release());
	ID3D11ShaderReflection* reflection = nullptr;
	if (FAILED(D3DReflect(code->GetBufferPointer(), code->GetBufferSize(), __uuidof(ID3D11ShaderReflection), (void**)&reflection)))
		return aprintf(arena, "D3DReflect failed");
	DEFER(reflection->Release());

	for (uint32 i = 0; i < bind_groups.count; ++i)
	{
		const GPU::ReflectedBindGroup& group = bind_groups[i];
		if (!(entry_point.bind_groups & (1u << group.index)) || !group.layout.uniform_size)
			continue;
		ZTStringView name = aprintf(arena, "B%u", group.index);
		D3D11_SHADER_INPUT_BIND_DESC bind;
		if (FAILED(reflection->GetResourceBindingDescByName(name.CString(), &bind)))
			continue; // still removed as unused
		uint32 expected = entry_point.d3d11_bind_group_registers[group.index].cbv;
		if (bind.Type != D3D_SIT_CBUFFER || bind.BindPoint != expected)
			return aprintf(arena, "%s is at register %u, expected b%u", name.CString(), bind.BindPoint, expected);
		ID3D11ShaderReflectionConstantBuffer* buffer = reflection->GetConstantBufferByName(name.CString());
		ID3D11ShaderReflectionVariable* variable = buffer->GetVariableByIndex(0);
		D3D11_SHADER_VARIABLE_DESC variable_desc;
		if (FAILED(variable->GetDesc(&variable_desc)))
			return aprintf(arena, "%s has no variable", name.CString());
		if (variable_desc.StartOffset != 0 || variable_desc.Size != group.layout.uniform_size)
			return aprintf(arena, "%s's variable is %u bytes at %u, we have %u bytes", name.CString(), variable_desc.Size,
				variable_desc.StartOffset, group.layout.uniform_size);
		ZTStringView problem = CompareTypes(variable->GetType(), group.type, GetAtomString(group.name, arena).CString(), arena);
		if (problem.length)
			return problem;
	}
	return {};
#else
	(void)entry_point;
	(void)bind_groups;
	(void)arena;
	return {};
#endif
}

ZTStringView CompileMSL(StringView text, Arena* arena)
{
#ifdef EVA_MACOS
	MTL::Device* device = MetalDevice();
	if (!device)
		return {};
	NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
	DEFER(pool->release());
	MTL::CompileOptions* options = MTL::CompileOptions::alloc()->init();
	DEFER(options->release());
	options->setLanguageVersion(MTL::LanguageVersion2_0);
	options->setFastMathEnabled(false);
	NS::String* source = NS::String::string(InternString(arena, text).CString(), NS::UTF8StringEncoding);
	NS::Error* error = nullptr;
	MTL::Library* library = device->newLibrary(source, options, &error);
	if (!library)
		return aprintf(arena, "%s", error ? error->localizedDescription()->utf8String() : "failed without a message");
	DEFER(library->release());
	MTL::Function* function = library->newFunction(NS::String::string(GPU::MSL_ENTRY_POINT_NAME, NS::UTF8StringEncoding));
	if (!function)
		return aprintf(arena, "no function %s", GPU::MSL_ENTRY_POINT_NAME);
	function->release();
	return {};
#else
	(void)text;
	(void)arena;
	return {};
#endif
}

}
