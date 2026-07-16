#pragma once
#include <EVA/Core/Serialization.hpp>
#include <EVA/Assets/Asset.hpp>
#include <EVA/GFX/GPUDevice.hpp>

struct EVERSION(4) ShaderPipelineState {
	GPUCullMode          cullMode  = GPUCullMode::Back;
	GPUBlendMode         blendMode = GPUBlendMode::Solid;
	GPUPrimitiveTopology topology  = GPUPrimitiveTopology::TriangleList;
	bool                 depthTest = true;
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
	GPUGraphicsPipeline* m_pipeline       = nullptr;

	virtual Result LoadImpl(FILE* f) override;
};

Result BuildShader(ZTString input_path, ZTString output_path);
