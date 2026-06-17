#include <EVA/Library.hpp>
#include <EVA/GLTF.hpp>
#include <EVA/Physics.hpp>

namespace Library
{
Mesh* mesh_cube = nullptr;
Mesh* mesh_monke = nullptr;

Texture* tex_test = nullptr;
Texture* tex_proto = nullptr;
Texture* tex_crate = nullptr;
Texture* tex_tiles1 = nullptr;
Texture* tex_tiles2 = nullptr;
Texture* tex_wall1 = nullptr;

PhysicsShape* shape_cube;
PhysicsShape* shape_ground;

GLTF* map_prime;

}

void LibraryInitialize()
{
	// @VOLATILE - The order these are loaded in defines their asset ids, which
	//             are shared over the network.

	AssetsSkipToId(256);
	Library::mesh_monke   = GLTFLoad("monke.glb")->meshes[0]; assert(Library::mesh_monke->id == 256);
	Library::mesh_cube    = GLTFLoad("cube.glb")->meshes[0];
	Library::tex_test     = TextureLoad("test.jpg");
	Library::tex_proto    = TextureLoad("proto.png");
	Library::shape_cube   = PhysicsCreateBoxShape(float3(1,1,1));
	Library::shape_ground = PhysicsCreateBoxShape(float3(20,1,20));
	Library::tex_crate    = TextureLoad("tex_crate.jpg");
	Library::tex_tiles1   = TextureLoad("tex_tiles1.jpg");
	Library::tex_tiles2   = TextureLoad("tex_tiles2.jpg");
	Library::tex_wall1    = TextureLoad("tex_wall1.jpg");
	Library::map_prime    = GLTFLoad("map_prime.glb");
}