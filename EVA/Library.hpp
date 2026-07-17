#pragma once

class Game;
class Texture;
class Mesh;
class Sprite;
class Material;
class Font;

void LibraryInitialize();

namespace Library {

extern Mesh* mesh_cone;

extern Texture* tex_test;
extern Texture* tex_proto;

extern Sprite* spr_ui_arrow_down;
extern Sprite* spr_ui_arrow_right;
extern Sprite* spr_crosshair;

extern Material* mat_brush;

extern Font* fnt_arial;

}
