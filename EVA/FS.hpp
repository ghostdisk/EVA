#pragma once
#include <EVA/String.hpp>

struct Arena;

namespace FS {

struct Stat {
	String   filename      = {};
	String   full_path     = {};
	bool     is_directory  = false;
};

void ReadDirectory(String path, void* userdata, void (*callback)(const Stat& stat, void* userdata));
String GetExtension(String path);

U16* StringToUTF16(Arena* arena, String string, size_t* out_len);
ZTString UTF16ToString(Arena* arena, U16* string, int len);

}
