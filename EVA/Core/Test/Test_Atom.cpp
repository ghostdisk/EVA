#include <EVA/Test/Test.hpp>
#include <EVA/Core/Atom.hpp>

using namespace EVA;

// The atom table is global and shared by every test in the process, so each test uses its own strings.

TEST(Atom, SameStringSameAtom)
{
	char copy[] = "Test_Atom_Same";
	CHECK_EQ(GetAtom("Test_Atom_Same"), GetAtom(copy));
}

TEST(Atom, DifferentStringsDifferentAtoms)
{
	CHECK(GetAtom("Test_Atom_DifferentA") != GetAtom("Test_Atom_DifferentB"));
	CHECK(GetAtom("Test_Atom_Case") != GetAtom("test_atom_case"));
}

TEST(Atom, NeverNone)
{
	CHECK(GetAtom("Test_Atom_NeverNone") != Atom::NONE);
	CHECK(GetAtom("x") != Atom::NONE);
}

TEST(Atom, UsesOnlyTheViewedPart)
{
	CHECK_EQ(GetAtom(StringView("Test_Atom_Partial_Suffix", 17)), GetAtom("Test_Atom_Partial"));
}

TEST(Atom, StringRoundTrip)
{
	Atom atom = GetAtom("Test_Atom_RoundTrip");
	ZTStringView string = GetAtomString(atom, test.arena);
	CHECK_EQ(string, "Test_Atom_RoundTrip");
	CHECK_EQ(string.CString()[string.length], '\0');
}

TEST(Atom, StringIsACopy)
{
	Atom atom = GetAtom("Test_Atom_Copy");
	ZTStringView first = GetAtomString(atom, test.arena);
	ZTStringView second = GetAtomString(atom, test.arena);
	CHECK(first.data != second.data);
	CHECK_EQ(first, second);
}
