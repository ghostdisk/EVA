#pragma once

struct GLTF;
struct Game;
struct Texture;
struct Mesh;
namespace JPH
{
	class Shape;
}
using PhysicsShape = JPH::Shape;

void LibraryInitialize();

namespace Library
{

extern Mesh* mesh_cube;
extern Mesh* mesh_monke;
extern Texture* tex_test;
extern Texture* tex_proto;
extern PhysicsShape* shape_cube;
extern PhysicsShape* shape_ground;

}
