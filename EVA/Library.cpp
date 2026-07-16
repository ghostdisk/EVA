#include <EVA/Library.hpp>
#include <EVA/Assets/Sprite.hpp>
#include <EVA/Assets/Texture.hpp>
#include <EVA/Assets/Model.hpp>
#include <EVA/Assets/Mesh.hpp>
#include <EVA/GFX/Renderer.hpp>
#include <EVA/Assets/Font.hpp>

namespace Library {

Mesh* mesh_cube = nullptr;
Mesh* mesh_cone = nullptr;

Texture* tex_test = nullptr;
Texture* tex_proto = nullptr;
Texture* tex_crate = nullptr;
Texture* tex_tiles1 = nullptr;
Texture* tex_tiles2 = nullptr;
Texture* tex_wall1 = nullptr;
Texture* tex_character = nullptr;

Sprite* spr_ui_arrow_down = nullptr;
Sprite* spr_ui_arrow_right = nullptr;
Sprite* spr_crosshair = nullptr;

Material* mat_brush = nullptr;

Font* fnt_arial = 0;

}

void LibraryInitialize() {
	ZoneScopedN("LibraryInitialize");
	// @VOLATILE - The order these are loaded in defines their asset ids, which
	//             are shared over the network.

	AssetsSkipToId(256);
	Library::tex_test           = Asset::Get<Texture>("test");
	Library::tex_proto          = Asset::Get<Texture>("proto");
	Library::tex_crate          = Asset::Get<Texture>("tex_crate");
	Library::tex_tiles1         = Asset::Get<Texture>("tex_tiles1");
	Library::tex_tiles2         = Asset::Get<Texture>("tex_tiles2");
	Library::mesh_cube          = Asset::Get<Model>("cube")->meshes[0];
	Library::mesh_cone          = Asset::Get<Model>("cone")->meshes[0];

	Texture* ui_atlas = Asset::Get<Texture>("ui_assets"); // TODO: disable mips!
	Library::spr_ui_arrow_down = SpriteCreate("spr_ui_arrow", ui_atlas, 0, 0, 15, 15);
	Library::spr_ui_arrow_right = SpriteCreate("spr_ui_arrow", ui_atlas, 16, 0, 15, 15);
	Library::spr_crosshair = SpriteCreate("spr_crosshair", ui_atlas, 32, 0, 15, 15);

	Library::mat_brush = MaterialCreate("mat_brush", shd_brush, Library::tex_proto);
	Library::fnt_arial = FontLoad("Arial.ttf", 20, 512);
}