#include <EVA/Assets/Shader.hpp>
#include <EVA/Core/Serialization.hpp>
#include <EVA/Core/OS.hpp>
#include <stdio.h>

static Result CompileShaderStage(String name, String stage, const Vector<String>& defines, ZTString intermediatePath, void** out_spv, size_t* out_spv_size) {
	StringBuilder cmdline;
	cmdline.Push("glslc -fshader-stage=%.*s", STRING_PRINTF_ARGS(stage));
	for (String define : defines)
		cmdline.Push(" -D%.*s", STRING_PRINTF_ARGS(define));
	cmdline.Push(" \"%.*s\" -o \"%s\"", STRING_PRINTF_ARGS(name), intermediatePath.c_str());
	TRY(ExecProcess(cmdline.Build()));

	if (!ReadEntireFile(intermediatePath, out_spv, out_spv_size))
		return Err("Failed to read %s", intermediatePath.c_str());

	remove(intermediatePath.c_str());
	return Success();
}

Result ShaderSource::LoadImpl(FILE* in) {
	Result err = Success();
	ScratchArena scratch;

	TextDeserializer d(in, scratch);
	ShaderAssetSourceData data;
	Deserialize(d, data);

	String fsPathWithoutExt = FS::WithoutExtension(m_fsPath);
	ZTString outputPath = scratch->Fmt("%.*s.shader", STRING_PRINTF_ARGS(fsPathWithoutExt));

	ShaderAssetCompiledData compiledData = {};
	compiledData.pipelineState = data.pipelineState;

	GLSLShader* vs = Asset::Get<GLSLShader>(data.vs);
	GLSLShader* fs = Asset::Get<GLSLShader>(data.fs);
	AddInput(vs);
	AddInput(fs);
	if (!AreAllInputsLoaded()) return Success();
	
	ZTString vsTempPath = scratch->Fmt("%s.fs.spv", outputPath.c_str());
	TRY(CompileShaderStage(vs->m_fsPath, "vertex", data.defines, vsTempPath, &compiledData.vs.data, &compiledData.vs.size));
	DEFER(free(compiledData.vs.data));

	ZTString fsTempPath = scratch->Fmt("%s.fs.spv", outputPath.c_str());
	TRY(CompileShaderStage(fs->m_fsPath, "fragment", data.defines, fsTempPath, &compiledData.fs.data, &compiledData.fs.size));
	DEFER(free(compiledData.fs.data));

	FILE* out = fopen(outputPath.c_str(), "wb");
	if (!out) return Err("Failed to open %s for writing", outputPath.c_str());
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
		.topology         = data.pipelineState.topology,
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
