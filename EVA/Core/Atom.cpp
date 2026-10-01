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

Atom GetAtom(StringView string)
{
	AtomTable& table = GetAtomTable();
	std::string key((const char*)string.data, string.length);

	auto it = table.atoms.find(key);
	if (it != table.atoms.end())
		return it->second;

	Atom atom = (Atom)table.strings.size();
	table.strings.push_back(key);
	table.atoms.emplace(std::move(key), atom);
	return atom;
}

ZTStringView GetAtomString(Atom atom, Arena* arena)
{
	AtomTable& table = GetAtomTable();
	assert((uint32)atom < table.strings.size());

	const std::string& string = table.strings[(uint32)atom];
	return InternString(arena, StringView(string.data(), string.size()));
}

}
