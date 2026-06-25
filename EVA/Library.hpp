#pragma once

struct GLTF;
struct Game;
struct Texture;
struct Mesh;
struct Sprite;
struct Material;

void LibraryInitialize();

namespace Library
{

extern Mesh* mesh_cube;
extern Mesh* mesh_monke;
extern Mesh* mesh_character;

extern Texture* tex_test;
extern Texture* tex_proto;

extern GLTF* map_prime;

extern Sprite* spr_ui_arrow_down;
extern Sprite* spr_ui_arrow_right;
extern Sprite* spr_crosshair;

extern Material* mat_brush;

}
