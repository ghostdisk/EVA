#include <EVA/Library.hpp>
#include <EVA/GLTF.hpp>
#include <EVA/Physics.hpp>
#include <EVA/Character.hpp>
#include <EVA/Renderer/Renderer.hpp>

namespace Library
{

Mesh* mesh_cube = nullptr;
Mesh* mesh_monke = nullptr;
Mesh* mesh_character = nullptr;
Mesh* mesh_cone = nullptr;

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
Sprite* spr_crosshair = nullptr;

Material* mat_brush = nullptr;

}

void LibraryInitialize()
{
	// @VOLATILE - The order these are loaded in defines their asset ids, which
	//             are shared over the network.

	AssetsSkipToId(256);
	Library::tex_test           = TextureLoad("test.jpg", true); assert(Library::tex_test->id == 256);

	Library::tex_proto          = TextureLoad("proto.png", true);
	Library::tex_crate          = TextureLoad("tex_crate.jpg", true);
	Library::tex_tiles1         = TextureLoad("tex_tiles1.jpg", true);
	Library::tex_tiles2         = TextureLoad("tex_tiles2.jpg", true);
	Library::tex_wall1          = TextureLoad("tex_wall1.jpg", true);
	Library::tex_character      = TextureLoad("tex_character.png", true);
	Library::map_prime          = GLTFLoad("map_prime.glb", true);
	Library::mesh_monke         = GLTFLoad("monke.glb", false)->meshes[0];
	Library::mesh_cube          = GLTFLoad("cube.glb", false)->meshes[0];
	Library::mesh_character     = GLTFLoad("character.glb", false)->meshes[0];
	Library::mesh_cone          = GLTFLoad("cone.glb", false)->meshes[0];

	Texture* ui_atlas = TextureLoad("ui_assets.psd", false);

	Library::spr_ui_arrow_down = SpriteCreate("spr_ui_arrow", ui_atlas, 0, 0, 15, 15);
	Library::spr_ui_arrow_right = SpriteCreate("spr_ui_arrow", ui_atlas, 16, 0, 15, 15);
	Library::spr_crosshair = SpriteCreate("spr_crosshair", ui_atlas, 32, 0, 15, 15);

	Library::mat_brush = MaterialCreate("mat_brush", shd_brush, Library::tex_proto);
}