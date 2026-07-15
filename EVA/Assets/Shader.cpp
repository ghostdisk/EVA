#include <EVA/Assets/Shader.hpp>
#include <EVA/Core/Serialization.hpp>
#include <EVA/Core/OS.hpp>

void Convert(const ShaderPipelineState_v1& v1, ShaderPipelineState_v2& v2) {
	v2.cullMode = v1.cullMode;
}

void Convert(const ShaderPipelineState_v2& v2, ShaderPipelineState& v3) {
	v3.blendMode = v2.blendMode;
	v3.cullMode = v2.cullMode;
}

static Result CompileShaderStage(String name, String stage, const std::vector<String>& defines, void** out_spv, size_t* out_spv_size) {
	StringBuilder cmdline;
	cmdline.Push("glslc -fshader-stage=%.*s", STRING_PRINTF_ARGS(stage));
	for (String define : defines)
		cmdline.Push(" -D%.*s", STRING_PRINTF_ARGS(define));
	cmdline.Push(" %s/EVA/Shaders/%.*s -o tmp.spv", EVA_BASE_DIR, STRING_PRINTF_ARGS(name));
	TRY(ExecProcess(cmdline.Build()));

	if (!ReadEntireFile("tmp.spv", out_spv, out_spv_size))
		return Err("Failed to read spv");

	return Success();
}

Result BuildShader(ZTString input_path, ZTString output_path) {
	FILE* in = fopen(input_path.c_str(), "rb");
	if (!in) return Err("Failed to open %s", input_path.c_str());
	DEFER(fclose(in));

	TextDeserializer d(in, FrameArena);

	d.BeginObject();

	d.Key("version");
	U32 version = d.DeserializeU8();
	if (version > 3)
		return Err("Unexpected version");

	d.Key("vs");
	String vs_name = d.DeserializeString();

	d.Key("fs");
	String fs_name = d.DeserializeString();

	d.Key("defines");
	std::vector<String> defines;
	defines.resize(d.BeginArray());

	for (int i = 0; i < defines.size(); i++) {
		defines[i] = d.DeserializeString();
	}

	d.EndArray();

	ShaderPipelineState ps = {};
	if (version >= 2) {
		d.Key("pipelineState");
		Deserialize(d, ps);
	}
	d.EndObject();

	void *vs, *fs;
	size_t vs_size, fs_size;
	TRY(CompileShaderStage(vs_name, "vertex", defines, &vs, &vs_size));
	DEFER(free(vs));
	TRY(CompileShaderStage(fs_name, "fragment", defines, &fs, &fs_size));
	DEFER(free(fs));

	FILE* out = fopen(output_path, "wb");
	if (!out) return Err("Failed to open %s for writing", output_path.c_str());
	DEFER(fclose(out));
	TextSerializer s(out);

	s.BeginObject();
	s.Key("version");
	s.SerializeU32(3);
	s.Key("vs");
	s.SerializeBytes(vs, vs_size);
	s.Key("fs");
	s.SerializeBytes(fs, fs_size);
	s.Key("pipelineState");
	Serialize(s, ps);
	s.EndObject();

	return d.res;
}

Result Shader::LoadImpl(FILE* f) {
	TextDeserializer d(f);

	d.BeginObject();
	d.Key("version");
	U32 version = d.DeserializeU32();

	if (version > 3) {
		return Err("Unsupported version");
	}

	size_t vs_size, fs_size;
	void *vs, *fs;

	d.Key("vs");
	d.DeserializeBytes(FrameArena->Alloc(), &vs_size, &vs);
	d.Key("fs");
	d.DeserializeBytes(FrameArena->Alloc(), &fs_size, &fs);

	ShaderPipelineState ps = {};
	if (version >= 2) {
		d.Key("pipelineState");
		Deserialize(d, ps);
	}

	d.EndObject();

	if (d.res.error) return d.res;
	if (!vs_size || vs_size % sizeof(U32) != 0) return Err("Invalid vertex shader SPIR-V");
	if (!fs_size || fs_size % sizeof(U32) != 0) return Err("Invalid fragment shader SPIR-V");

	GFX::GraphicsDevice* device = GFX::GraphicsDevice::Get();
	m_vertexModule = device->CreateShaderModule(GFX::ShaderModuleDesc{
		.code     = (const U32*)vs,
		.codeSize = vs_size,
	});
	if (!m_vertexModule) return Err("Failed to create vertex shader module");

	m_fragmentModule = device->CreateShaderModule(GFX::ShaderModuleDesc{
		.code     = (const U32*)fs,
		.codeSize = fs_size,
	});
	if (!m_fragmentModule) {
		device->DestroyShaderModule(m_vertexModule);
		m_vertexModule = nullptr;
		return Err("Failed to create fragment shader module");
	}

	m_pipeline = device->CreateGraphicsPipeline(GFX::GraphicsPipelineDesc{
		.vertexShader   = m_vertexModule,
		.fragmentShader = m_fragmentModule,
		.cullMode       = ps.cullMode,
		.depthTestEnable = ps.depthTest,
		.depthWriteEnable = ps.depthTest,
		.blendMode      = ps.blendMode,
		.format = {
			.colorFormat = { device->GetCurrentBackbuffer()->m_format },
			.depthFormat = GFX::Format::D32_FLOAT,
		},
		.pushConstantSize = 128,
	});
	if (!m_pipeline) {
		device->DestroyShaderModule(m_fragmentModule);
		device->DestroyShaderModule(m_vertexModule);
		m_fragmentModule = nullptr;
		m_vertexModule = nullptr;
		return Err("Failed to create graphics pipeline");
	}

	return Success();
}
