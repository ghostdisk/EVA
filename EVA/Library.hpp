#pragma once

struct GLTF;
struct Game;
struct Texture;
struct Mesh;
struct Collider;

void LibraryInitialize();

namespace Library
{

extern Mesh* mesh_cube;
extern Mesh* mesh_monke;
extern Texture* tex_test;
extern Texture* tex_proto;
extern Collider* collider_cube;
extern GLTF* map_prime;

}
