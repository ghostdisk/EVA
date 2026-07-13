#include <EVA/Assets/Shader.hpp>
#include <EVA/Core/Serialization.hpp>
#include <EVA/Core/OS.hpp>

static Result CompileShaderStage(String name, String stage, const std::vector<String>& defines, void** out_spv, size_t* out_spv_size) {
	TRY(ExecProcess(Fmt(FrameArena, "glslc -fshader-stage=%.*s %s/EVA/Shaders/%.*s -o tmp.spv",
		STRING_PRINTF_ARGS(stage), EVA_BASE_DIR, STRING_PRINTF_ARGS(name))));

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
	if (version != 1)
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
	s.SerializeU32(1);
	s.Key("vs");
	s.SerializeBytes(vs, vs_size);
	s.Key("fs");
	s.SerializeBytes(fs, fs_size);
	s.EndObject();

	return d.res;
}

Result Shader::LoadImpl(FILE* f) {
	TextDeserializer d(f);

	d.BeginObject();
	d.Key("version");
	U32 version = d.DeserializeU32();

	if (version != 1) {
		return Err("Unsupported version");
	}

	size_t vs_size, fs_size;
	void *vs, *fs;

	d.Key("vs");
	d.DeserializeBytes(FrameArena->Alloc(), &vs_size, &vs);
	d.Key("fs");
	d.DeserializeBytes(FrameArena->Alloc(), &fs_size, &fs);
	d.EndObject();

	if (d.res.error) return d.res;

	return Success();
}