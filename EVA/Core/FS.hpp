#pragma once
#include <EVA/Core/Basic.hpp>

struct Arena;

namespace FS {

struct Stat {
	ZTString   filename      = {};
	ZTString   full_path     = {};
	bool       is_directory  = false;
};

void ReadDirectory(String path, void* userdata, void (*callback)(const Stat& stat, void* userdata));
String GetExtension(String path);
String WithoutExtension(String path);

U16* StringToUTF16(Arena* arena, String string, size_t* out_len);
ZTString UTF16ToString(Arena* arena, U16* string, int len);

void ReplaceFileExtension(char* buffer, size_t buflen, const char* new_ext);

}
