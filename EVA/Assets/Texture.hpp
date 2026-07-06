#include <EVA/Assets/Asset.hpp>

struct Texture : Asset {
	U32    handle = 0;
	size_t width  = 0;
	size_t height = 0;
};