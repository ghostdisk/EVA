#pragma once
#include <EVA/Assets/Asset.hpp>
#include <EVA/Renderer/GraphicsDevice.hpp>

struct ShaderPipelineState {
	GFX::CullMode cullMode = GFX::CullMode::Back;
	GFX::BlendMode blendMode = GFX::BlendMode::Solid;
};

class Shader : public Asset {
public:
	ECLASS_COMMON();

	GFX::ShaderModule*     m_vertexModule   = nullptr;
	GFX::ShaderModule*     m_fragmentModule = nullptr;
	GFX::GraphicsPipeline* m_pipeline       = nullptr;

	virtual Result LoadImpl(FILE* f) override;
};

Result BuildShader(ZTString input_path, ZTString output_path);
