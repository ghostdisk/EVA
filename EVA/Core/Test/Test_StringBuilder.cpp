#include <EVA/Test/Test.hpp>
#include <EVA/Core/StringBuilder.hpp>
#include <string.h>

using namespace EVA;

TEST(StringBuilder, EmptyIsEmptyString)
{
	StringBuilder builder(test.arena);
	ZTStringView string = builder.ToString();
	CHECK_EQ(string.length, 0u);
	REQUIRE(string.CString() != nullptr);
	CHECK_EQ(string.CString()[0], '\0');
}

TEST(StringBuilder, Append)
{
	StringBuilder builder(test.arena);
	builder.Append("hello");
	builder.Append(StringView(" world!!", 6));
	CHECK_EQ(builder.ToString(), "hello world");
	CHECK_EQ(builder.ToString().CString()[11], '\0');
}

TEST(StringBuilder, AppendEmpty)
{
	StringBuilder builder(test.arena);
	builder.Append("");
	builder.Append(StringView());
	CHECK_EQ(builder.ToString(), "");
	builder.Append("a");
	builder.Append("");
	CHECK_EQ(builder.ToString(), "a");
}

TEST(StringBuilder, AppendFormat)
{
	StringBuilder builder(test.arena);
	builder.AppendFormat("%s=%d", "x", 42);
	builder.AppendFormat(", %.*s", 2, "yzw");
	builder.AppendFormat("%s", "");
	CHECK_EQ(builder.ToString(), "x=42, yz");
}

TEST(StringBuilder, KeepsEmbeddedZero)
{
	StringBuilder builder(test.arena);
	builder.Append(StringView("a\0b", 3));
	CHECK_EQ(builder.ToString().length, 3u);
	CHECK_EQ(builder.ToString(), StringView("a\0b", 3));
}

TEST(StringBuilder, GrowsLarge)
{
	StringBuilder builder(test.arena);
	for (int i = 0; i < 10000; ++i)
		builder.Append("0123456789");
	ZTStringView string = builder.ToString();
	REQUIRE_EQ(string.length, 100000u);
	CHECK_EQ(StringView(string.CString() + 99990, 10), "0123456789");
	CHECK_EQ(string.CString()[100000], '\0');
}

TEST(StringBuilder, GrowsInPlaceWhenLastAllocation)
{
	StringBuilder builder(test.arena);
	builder.Append("a");
	char* first = builder.data;
	for (int i = 0; i < 1000; ++i)
		builder.Append("b");
	CHECK(builder.data == first);
	CHECK_EQ(builder.ToString().length, 1001u);
}

TEST(StringBuilder, MovesWhenArenaWasUsedInBetween)
{
	StringBuilder builder(test.arena);
	builder.Append("abc");
	char* first = builder.data;
	char* other = (char*)test.arena->Allocate(4);
	memcpy(other, "zzz", 4);

	for (int i = 0; i < 1000; ++i)
		builder.Append("d");
	CHECK(builder.data != first);
	CHECK_EQ(StringView(builder.data, 4), "abcd");
	CHECK_EQ(builder.ToString().length, 1003u);
	CHECK_EQ(StringView(other), "zzz");
}

TEST(StringBuilder, FullArenaPanics)
{
	Arena* arena = CreateArena(128);
	DEFER(DestroyArena(arena));

	StringBuilder builder(arena);
	CHECK_PANICS(
		for (int i = 0; i < 100; ++i)
			builder.Append("0123456789"));
}
