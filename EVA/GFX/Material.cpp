#include <EVA/Assets/Material.hpp>

// TODO
Material* MaterialCreate(const char* name, Shader* shader, Texture* texture) {
	printf("[asset] MaterialCreate uses old style\n");
	Material* material = new Material();

	material->shader = shader;
	material->color_texture = texture;
	return material;
}