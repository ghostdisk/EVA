#pragma once
#include <EVA/Core/Basic.hpp>

class Arena;

namespace FS {

struct Timestamp {
	U64 value = 0;

	constexpr bool operator<(Timestamp other) const { return value < other.value; }
	constexpr bool operator>(Timestamp other) const { return value > other.value; }
	constexpr bool operator==(Timestamp other) const { return value == other.value; }
};

struct Stat {
	ZTString  filename     = {};
	ZTString  fullPath     = {};
	Timestamp ctime        = {};
	Timestamp atime        = {};
	Timestamp mtime        = {};
	bool      isDirectory  = false;
};

void ReadDirectory(String path, void* userdata, void (*callback)(const Stat& stat, void* userdata));
String GetExtension(String path);
String WithoutExtension(String path);

void ReplaceFileExtension(char* buffer, size_t buflen, const char* new_ext);

}
