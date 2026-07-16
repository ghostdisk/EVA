#pragma once
#include <EVA/Core/Serialization.hpp>
#include <EVA/Assets/Asset.hpp>
#include <EVA/GFX/GraphicsDevice.hpp>

struct EVERSION(1, ShaderPipelineState) ShaderPipelineState_v1 {
	CullMode  cullMode  = CullMode::Back;
};
struct EVERSION(2, ShaderPipelineState) ShaderPipelineState_v2 {
	CullMode  cullMode  = CullMode::Back;
	BlendMode blendMode = BlendMode::Solid;
};
struct EVERSION(3) ShaderPipelineState {
	CullMode  cullMode  = CullMode::Back;
	BlendMode blendMode = BlendMode::Solid;
	bool           depthTest = true;
};
EAUTO_SERIALIZE(ShaderPipelineState);

struct EVERSION(3) ShaderAssetSourceData {
	String              vs            = {};
	String              fs            = {};
	Vector<String>      defines       = {};
	ShaderPipelineState pipelineState = {};
};
EAUTO_SERIALIZE(ShaderAssetSourceData);

struct EVERSION(3) ShaderAssetCompiledData {
	SerializableBytes   vs            = {};
	SerializableBytes   fs            = {};
	ShaderPipelineState pipelineState = {};
};
EAUTO_SERIALIZE(ShaderAssetCompiledData);

class Shader : public Asset {
public:
	ECLASS_COMMON();

	ShaderModule*     m_vertexModule   = nullptr;
	ShaderModule*     m_fragmentModule = nullptr;
	GraphicsPipeline* m_pipeline       = nullptr;

	virtual Result LoadImpl(FILE* f) override;
};

Result BuildShader(ZTString input_path, ZTString output_path);
