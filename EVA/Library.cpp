#include <EVA/Library.hpp>
#include <EVA/GLTF.hpp>
#include <EVA/Physics.hpp>

namespace Library
{
Mesh* mesh_cube = nullptr;
Mesh* mesh_monke = nullptr;

Texture* tex_test = nullptr;
Texture* tex_proto = nullptr;

PhysicsShape* shape_cube;
PhysicsShape* shape_ground;
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
}