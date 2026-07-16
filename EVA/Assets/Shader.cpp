#include <EVA/Assets/Shader.hpp>
#include <EVA/Core/Serialization.hpp>
#include <EVA/Core/OS.hpp>
#include <stdio.h>

void Convert(const ShaderPipelineState_v1& v1, ShaderPipelineState_v2& v2) {
	v2.cullMode = v1.cullMode;
}

void Convert(const ShaderPipelineState_v2& v2, ShaderPipelineState& v3) {
	v3.blendMode = v2.blendMode;
	v3.cullMode = v2.cullMode;
}

static Result CompileShaderStage(String name, String stage, const Vector<String>& defines, ZTString intermediatePath, void** out_spv, size_t* out_spv_size) {
	StringBuilder cmdline;
	cmdline.Push("glslc -fshader-stage=%.*s", STRING_PRINTF_ARGS(stage));
	for (String define : defines)
		cmdline.Push(" -D%.*s", STRING_PRINTF_ARGS(define));
	cmdline.Push(" \"%s/EVA/Shaders/%.*s\" -o \"%s\"", EVA_BASE_DIR, STRING_PRINTF_ARGS(name), intermediatePath.c_str());
	TRY(ExecProcess(cmdline.Build()));

	if (!ReadEntireFile(intermediatePath, out_spv, out_spv_size))
		return Err("Failed to read %s", intermediatePath.c_str());

	return Success();
}

Result BuildShader(ZTString input_path, ZTString output_path) {
	Result err = Success();
	ScratchArena scratch;

	FILE* in = fopen(input_path.c_str(), "rb");
	if (!in) return Err("Failed to open %s", input_path.c_str());
	DEFER(fclose(in));

	TextDeserializer d(in, scratch);
	ShaderAssetSourceData data;
	Deserialize(d, data);

	ShaderAssetCompiledData compiledData = {};
	ZTString vertexIntermediatePath = scratch->Fmt("%s.vertex.spv", output_path.c_str());
	TRY(CompileShaderStage(data.vs, "vertex", data.defines, vertexIntermediatePath, &compiledData.vs.data, &compiledData.vs.size));
	remove(vertexIntermediatePath.c_str());
	DEFER(free(compiledData.vs.data));
	ZTString fragmentIntermediatePath = scratch->Fmt("%s.fragment.spv", output_path.c_str());
	TRY(CompileShaderStage(data.fs, "fragment", data.defines, fragmentIntermediatePath, &compiledData.fs.data, &compiledData.fs.size));
	remove(fragmentIntermediatePath.c_str());
	DEFER(free(compiledData.fs.data));
	compiledData.pipelineState = data.pipelineState;

	FILE* out = fopen(output_path, "wb");
	if (!out) return Err("Failed to open %s for writing", output_path.c_str());
	DEFER(fclose(out));
	TextSerializer s(out);

	Serialize(s, compiledData);
	return d.res;
}

Result Shader::LoadImpl(FILE* f) {
	ScratchArena scratch;
	TextDeserializer d(f, scratch);

	ShaderAssetCompiledData data;
	Deserialize(d, data);
	if (d.res.error) return d.res;

	GPUDevice* device = GPUDevice::Get();

	GPUShaderModule* vertexModule = device->CreateShaderModule(GPUShaderModuleDesc{
		.code     = (const U32*)data.vs.data,
		.codeSize = data.vs.size,
	});
	if (!vertexModule) return Err("Failed to create vertex shader module");
	DEFER(device->DestroyShaderModule(vertexModule));

	GPUShaderModule* fragmentModule = device->CreateShaderModule(GPUShaderModuleDesc{
		.code     = (const U32*)data.fs.data,
		.codeSize = data.fs.size,
	});
	if (!fragmentModule) {
		return Err("Failed to create fragment shader module");
	}
	DEFER(device->DestroyShaderModule(fragmentModule));

	m_pipeline = device->CreateGraphicsPipeline(GPUGraphicsPipelineDesc{
		.vertexShader     = vertexModule,
		.fragmentShader   = fragmentModule,
		.cullMode         = data.pipelineState.cullMode,
		.depthTestEnable  = data.pipelineState.depthTest,
		.depthWriteEnable = data.pipelineState.depthTest,
		.blendMode        = data.pipelineState.blendMode,
		.format = {
			.colorFormat = { device->GetCurrentBackbuffer()->m_format },
			.depthFormat = GPUFormat::D32_FLOAT,
		},
		.pushConstantSize = 128,
	});
	if (!m_pipeline)
		return Err("Failed to create graphics pipeline");

	return Success();
}
