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

/**
 ** GLSLShader corresponds to a .glsl file. They're fed as inputs to ShaderSource.
 **
 ** This class is a dumb node that exists just so the asset system can track changes in .glsl files,
 ** so we can do shader hot-reloading.
 **/
class GLSLShader : public Asset {
public:
	ECLASS_COMMON(GLSLShader);

	virtual AssetLoadType GetLoadType() override {
		return AssetLoadType::File;
	}
};

/**
 ** ShaderSource corresponds to a .shader file. It references to its GLSL shader stage source files and pipeline state.
 ** LoadImpl builds the compiled runtime file (.shader)
 **/
class ShaderSource : public Asset {
public:
	ECLASS_COMMON(ShaderSource);

	virtual bool AssetNameHasFileExtension() override {
		return true;
	}

	virtual Result LoadImpl(Deserializer& d) override;
};

/**
 ** Shader corresponds to a .cshader file - this is the runtime shader asset class.
 ** It's built from a corresponding .shader file.
 **/
class Shader : public Asset {
public:
	ECLASS_COMMON(Shader);

	GPUGraphicsPipeline* m_pipeline = nullptr;

	virtual Result LoadImpl(Deserializer& d) override;
};
