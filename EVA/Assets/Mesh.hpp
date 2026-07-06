#include <EVA/Assets/Asset.hpp>

struct Mesh : Asset {
	U32           vao             = 0;
	U32           vbo             = 0;
	U32           ibo             = 0;
	U32           index_count     = 0;
	U32           vertex_count    = 0;
	Material*     default_maerial = nullptr;
};