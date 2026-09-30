#include <EVA/Core/Atom.hpp>
#include <string.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace EVA
{

struct AtomTable
{
	std::unordered_map<std::string, Atom> atoms;
	std::vector<std::string> strings = { "" }; // indexed by atom, 0 is NONE
};

// Function-local so it's constructed before first use, even from other static initializers.
static AtomTable& GetAtomTable()
{
	static AtomTable table;
	return table;
}

Atom GetAtom(const char* string)
{
	return GetAtom(string, strlen(string));
}

Atom GetAtom(const char* string, size_t length)
{
	AtomTable& table = GetAtomTable();
	std::string key(string, length);

	auto it = table.atoms.find(key);
	if (it != table.atoms.end())
		return it->second;

	Atom atom = (Atom)table.strings.size();
	table.strings.push_back(key);
	table.atoms.emplace(std::move(key), atom);
	return atom;
}

char* GetAtomString(Atom atom, Arena* arena)
{
	AtomTable& table = GetAtomTable();
	assert((uint32)atom < table.strings.size());

	const std::string& string = table.strings[(uint32)atom];
	char* copy = (char*)arena->Allocate(string.size() + 1, 1);
	memcpy(copy, string.c_str(), string.size() + 1);
	return copy;
}

}
