#pragma once

struct GLTF;
struct Game;
struct Texture;
struct Mesh;
struct Collider;
struct CharacterCollider;

void LibraryInitialize();

namespace Library
{

extern Mesh* mesh_cube;
extern Mesh* mesh_monke;
extern Mesh* mesh_character;

extern Texture* tex_test;
extern Texture* tex_proto;

extern Collider* collider_cube;
extern CharacterCollider* cc_main;

extern GLTF* map_prime;

}
