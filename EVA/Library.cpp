#include <EVA/Library.hpp>
#include <EVA/GLTF.hpp>
#include <EVA/Physics.hpp>
#include <EVA/Character.hpp>

namespace Library
{
Mesh* mesh_cube = nullptr;
Mesh* mesh_monke = nullptr;
Mesh* mesh_character = nullptr;

Texture* tex_test = nullptr;
Texture* tex_proto = nullptr;
Texture* tex_crate = nullptr;
Texture* tex_tiles1 = nullptr;
Texture* tex_tiles2 = nullptr;
Texture* tex_wall1 = nullptr;
Texture* tex_character = nullptr;

GLTF* map_prime = nullptr;

Sprite* spr_ui_arrow_down = nullptr;
Sprite* spr_ui_arrow_right = nullptr;

}

void LibraryInitialize()
{
	// @VOLATILE - The order these are loaded in defines their asset ids, which
	//             are shared over the network.

	AssetsSkipToId(256);
	Library::tex_test           = TextureLoad("test.jpg"); assert(Library::tex_test->id == 256);

	Library::tex_proto          = TextureLoad("proto.png");
	Library::tex_crate          = TextureLoad("tex_crate.jpg");
	Library::tex_tiles1         = TextureLoad("tex_tiles1.jpg");
	Library::tex_tiles2         = TextureLoad("tex_tiles2.jpg");
	Library::tex_wall1          = TextureLoad("tex_wall1.jpg");
	Library::tex_character      = TextureLoad("tex_character.png");
	Library::map_prime          = GLTFLoad("map_prime.glb", true);
	Library::mesh_monke         = GLTFLoad("monke.glb", false)->meshes[0];
	Library::mesh_cube          = GLTFLoad("cube.glb", false)->meshes[0];
	Library::mesh_character     = GLTFLoad("character.glb", false)->meshes[0];

	Texture* ui_atlas = TextureLoad("ui_assets.psd");

	Library::spr_ui_arrow_down = SpriteCreate("spr_ui_arrow", ui_atlas, 0, 0, 15, 15);
	Library::spr_ui_arrow_right = SpriteCreate("spr_ui_arrow", ui_atlas, 16, 0, 15, 15);
}