#include <EVA/Test/Test.hpp>

static size_t Length(StringView string)
{
	return string.length;
}

TEST(StringView, DefaultIsEmpty)
{
	StringView string;
	CHECK(string.data == nullptr);
	CHECK_EQ(string.length, 0u);
	CHECK(!string);
}

TEST(StringView, FromCString)
{
	const char* cstring = "hello";
	StringView string = cstring;
	CHECK(string.data == (uint8*)cstring);
	CHECK_EQ(string.length, 5u);
}

TEST(StringView, FromNullptrIsEmpty)
{
	StringView string = (const char*)nullptr;
	CHECK_EQ(string.length, 0u);
	CHECK(!string);
}

TEST(StringView, FromPointerAndLengthKeepsEmbeddedZero)
{
	StringView string("ab\0cd", 5);
	CHECK_EQ(string.length, 5u);
	CHECK_EQ(string.data[3], 'c');
}

TEST(StringView, BoolMeansNonEmpty)
{
	CHECK(!StringView(""));
	CHECK(StringView("a"));
	CHECK(!StringView("abc", 0));
}

TEST(StringView, EqualityComparesContents)
{
	char buffer[] = "abc";
	CHECK_EQ(StringView(buffer), StringView("abc"));
	CHECK(StringView(buffer).data != StringView("abc").data);

	CHECK(!(StringView("abc") == StringView("abd")));
	CHECK(!(StringView("abc") == StringView("ab")));
	CHECK(StringView("abc") != StringView("abcd"));
}

TEST(StringView, EqualityOfEmptyViews)
{
	CHECK_EQ(StringView(), StringView(""));
	CHECK_EQ(StringView(), ZTStringView());
	CHECK_EQ(StringView("abc", 0), StringView());
}

TEST(StringView, EqualityWithCStrings)
{
	StringView string("hello world", 5);
	CHECK(string == "hello");
	CHECK("hello" == string);
	CHECK(string != "hello world");
}

TEST(ZTStringView, DefaultPointsAtEmptyString)
{
	ZTStringView string;
	REQUIRE(string.CString() != nullptr);
	CHECK_EQ(string.CString()[0], '\0');
	CHECK_EQ(string.length, 0u);
	CHECK(!string);
}

TEST(ZTStringView, FromNullptrPointsAtEmptyString)
{
	ZTStringView string = (const char*)nullptr;
	REQUIRE(string.CString() != nullptr);
	CHECK_EQ(string.CString()[0], '\0');
	CHECK_EQ(string.length, 0u);
}

TEST(ZTStringView, FromCString)
{
	const char* cstring = "hello";
	ZTStringView string = cstring;
	CHECK(string.CString() == cstring);
	CHECK_EQ(string.length, 5u);
}

TEST(ZTStringView, UsableAsStringView)
{
	ZTStringView string = "hello";
	CHECK_EQ(Length(string), 5u);
	CHECK_EQ(string, StringView("hello"));
}
