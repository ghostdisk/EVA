#include <EVA/Library.hpp>
#include <EVA/Assets/Sprite.hpp>
#include <EVA/Assets/Texture.hpp>
#include <EVA/Assets/Model.hpp>
#include <EVA/Assets/MeshAsset.hpp>
#include <EVA/GFX/Mesh.hpp>
#include <EVA/GFX/Renderer.hpp>
#include <EVA/Assets/Font.hpp>

namespace Library {

Texture* tex_proto = nullptr;

Sprite* spr_ui_arrow_down = nullptr;
Sprite* spr_ui_arrow_right = nullptr;
Sprite* spr_crosshair = nullptr;

Material* mat_brush = nullptr;

Font* fnt_arial = 0;

}

void LibraryInitialize() {
	ZoneScopedN("LibraryInitialize");

	printf("LibraryInitialize deprecated\n");

	Library::tex_proto          = Asset::Get<Texture>("/Textures/proto");

	Texture* ui_atlas = Asset::Get<Texture>("/Textures/ui_assets"); // TODO: disable mips!
	Library::spr_ui_arrow_down = SpriteCreate("spr_ui_arrow", ui_atlas, 0, 0, 15, 15);
	Library::spr_ui_arrow_right = SpriteCreate("spr_ui_arrow", ui_atlas, 16, 0, 15, 15);
	Library::spr_crosshair = SpriteCreate("spr_crosshair", ui_atlas, 32, 0, 15, 15);

	Library::mat_brush = MaterialCreate("mat_brush", shd_brush, Library::tex_proto);
	Library::fnt_arial = FontLoad("/Fonts/Arial.ttf", 20, 512);
}