#include <EVA/Library.hpp>
#include <EVA/GLTF.hpp>
#include <EVA/Physics.hpp>

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

Collider* collider_cube;
Collider* collider_character;

GLTF* map_prime;

}

void LibraryInitialize()
{
	// @VOLATILE - The order these are loaded in defines their asset ids, which
	//             are shared over the network.

	AssetsSkipToId(256);
	Library::tex_test           = TextureLoad("test.jpg"); assert(Library::tex_test->id == 256);

	Library::tex_proto          = TextureLoad("proto.png");
	Library::collider_cube      = PhysicsCreateBoxCollider(float3(1,1,1));
	Library::tex_crate          = TextureLoad("tex_crate.jpg");
	Library::tex_tiles1         = TextureLoad("tex_tiles1.jpg");
	Library::tex_tiles2         = TextureLoad("tex_tiles2.jpg");
	Library::tex_wall1          = TextureLoad("tex_wall1.jpg");
	Library::tex_character      = TextureLoad("tex_character.png");
	Library::map_prime          = GLTFLoad("map_prime.glb", true);
	Library::mesh_monke         = GLTFLoad("monke.glb", false)->meshes[0];
	Library::mesh_cube          = GLTFLoad("cube.glb", false)->meshes[0];
	Library::mesh_character     = GLTFLoad("character.glb", false)->meshes[0];
	Library::collider_character = PhysicsCreateBoxCollider(float3(.25,1.8,.25) / 2.0f);
}