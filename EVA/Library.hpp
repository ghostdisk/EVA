#pragma once

struct GLTF;
struct Game;
struct Texture;
struct Mesh;
struct PhysicsShape;

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
