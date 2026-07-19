#pragma once

class Game;
class Texture;
class Mesh;
class Sprite;
class Material;
class Font;

void LibraryInitialize();

namespace Library {

extern Texture* tex_proto;
extern Sprite* spr_ui_arrow_down;
extern Sprite* spr_ui_arrow_right;
extern Material* mat_brush;
extern Font* fnt_arial;

}
