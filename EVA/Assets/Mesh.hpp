#pragma once
#include <EVA/Assets/Asset.hpp>

class Material;

class ECLASS() Mesh : public Asset {
public:
	ECLASS_COMMON();

	U32           vao             = 0;
	U32           vbo             = 0;
	U32           ibo             = 0;
	U32           index_count     = 0;
	U32           vertex_count    = 0;
	Material*     default_maerial = nullptr;
};