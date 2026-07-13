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

void ReplaceFileExtension(char* buffer, size_t buflen, const char* new_ext);

}
