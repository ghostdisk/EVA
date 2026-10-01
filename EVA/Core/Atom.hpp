#pragma once
#include <EVA/Core/Common.hpp>
#include <EVA/Core/Arena.hpp>

namespace EVA
{

// Interned string id: equal strings always get the same atom. GetAtom never returns NONE.
// The table is global and not thread safe.
enum class Atom : uint32
{
	NONE = 0,
};

Atom GetAtom(StringView string);

// Copies the atom's string into the arena.
ZTStringView GetAtomString(Atom atom, Arena* arena);

}
