#pragma once
#include <EVA/Core/Common.hpp>
#include <EVA/Core/Arena.hpp>

namespace EVA
{

// Builds a string in an arena. Grows in place while its buffer is the arena's most recent allocation, otherwise moves
// to a bigger buffer, leaving the old one behind in the arena.
struct StringBuilder
{
	Arena* arena = nullptr;
	char* data = nullptr;
	size_t length = 0;
	size_t capacity = 0; // always room for the zero terminator once data is allocated

	StringBuilder(Arena* arena) : arena(arena) {}

	void Append(StringView string);
	void AppendFormat(const char* format, ...);

	// The contents so far. Valid until the next append.
	ZTStringView ToString() const;

	// Makes room for extra more characters plus the zero terminator.
	void Reserve(size_t extra);
};

}
