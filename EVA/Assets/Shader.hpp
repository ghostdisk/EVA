#pragma once
#include <EVA/Assets/Asset.hpp>
#include <EVA/Renderer/GraphicsDevice.hpp>

class Shader : public Asset {
public:
	ECLASS_COMMON();

	GFX::ShaderModule*     m_vertexModule   = nullptr;
	GFX::ShaderModule*     m_fragmentModule = nullptr;
	GFX::GraphicsPipeline* m_pipeline       = nullptr;
};
