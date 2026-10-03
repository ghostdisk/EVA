#include <EVA/Script/Test/OutputValidation.hpp>

#ifdef EVA_HAVE_SPIRV_TOOLS
#include <spirv-tools/libspirv.h>
#endif

#ifdef EVA_WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3dcompiler.h>
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
