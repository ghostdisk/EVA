#include <EVA/Assets/Material.hpp>

// TODO
Material* MaterialCreate(const char* name, Shader* shader, Texture* texture) {
	Material* material = new Material();
	AssetInit(material, name);

	material->shader = shader;
	material->color_texture = texture;
	return material;
}