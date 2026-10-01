#include <EVA/Test/Test.hpp>
#include <stdint.h>
#include <string.h>

using namespace EVA;

TEST(Arena, AllocateAdvancesHead)
{
	Arena* arena = CreateArena(64);
	DEFER(DestroyArena(arena));

	uint8* first = (uint8*)arena->Allocate(10);
	uint8* second = (uint8*)arena->Allocate(20);
	CHECK(first == arena->begin);
	CHECK(second == first + 10);
	CHECK(arena->head == first + 30);
}

TEST(Arena, AllocateAligned)
{
	for (size_t alignment = 1; alignment <= 256; alignment *= 2)
	{
		test.arena->Allocate(1); // misalign the head
		void* memory = test.arena->Allocate(8, alignment);
		CHECK_EQ((uintptr_t)memory % alignment, 0u);
	}
}

TEST(Arena, ExactFitDoesNotPanic)
{
	Arena* arena = CreateArena(32);
	DEFER(DestroyArena(arena));

	arena->Allocate(32);
	CHECK(arena->head == arena->end);
	arena->Allocate(0);
}

TEST(Arena, FullArenaPanics)
{
	Arena* arena = CreateArena(32);
	DEFER(DestroyArena(arena));

	arena->Allocate(32);
	CHECK_PANICS(arena->Allocate(1));
}

TEST(Arena, AlignmentPastEndPanics)
{
	Arena* arena = CreateArena(32);
	DEFER(DestroyArena(arena));

	arena->Allocate(31);
	CHECK_PANICS(arena->Allocate(1, 64));
}

TEST(Arena, HugeAllocationPanics)
{
	CHECK_PANICS(test.arena->Allocate(SIZE_MAX));
}

TEST(Arena, PanicMessageMentionsSize)
{
	Arena* arena = CreateArena(32);
	DEFER(DestroyArena(arena));

	try
	{
		arena->Allocate(100);
		CHECK(!"didn't panic");
	}
	catch (const Test::PanicException& exception)
	{
		CHECK(strstr(exception.message, "100") != nullptr);
	}
}

struct Thing
{
	int a;
	float b;
	void* c;
	int d = 7;
};

TEST(Arena, NewValueInitializes)
{
	// Dirty the memory first so zeroes can only come from New.
	memset(test.arena->Allocate(sizeof(Thing) * 2), 0xCD, sizeof(Thing) * 2);
	test.arena->head = test.arena->begin;

	Thing* thing = test.arena->New<Thing>();
	CHECK_EQ(thing->a, 0);
	CHECK_EQ(thing->b, 0.0f);
	CHECK(thing->c == nullptr);
	CHECK_EQ(thing->d, 7);
	CHECK_EQ((uintptr_t)thing % alignof(Thing), 0u);
}

TEST(InternString, CopiesAndTerminates)
{
	char source[] = "hello";
	ZTStringView copy = InternString(test.arena, source);
	CHECK(copy.CString() != source);
	CHECK_EQ(copy, "hello");
	CHECK_EQ(copy.CString()[copy.length], '\0');

	source[0] = 'j';
	CHECK_EQ(copy, "hello");
}

TEST(InternString, CopiesPartOfAString)
{
	ZTStringView copy = InternString(test.arena, StringView("hello world", 5));
	CHECK_EQ(copy, "hello");
	CHECK_EQ(copy.CString()[5], '\0');
}

TEST(InternString, Empty)
{
	ZTStringView copy = InternString(test.arena, StringView());
	CHECK_EQ(copy.length, 0u);
	REQUIRE(copy.CString() != nullptr);
	CHECK_EQ(copy.CString()[0], '\0');
}

TEST(InternString, KeepsEmbeddedZero)
{
	ZTStringView copy = InternString(test.arena, StringView("ab\0cd", 5));
	CHECK_EQ(copy.length, 5u);
	CHECK_EQ(copy, StringView("ab\0cd", 5));
	CHECK_EQ(copy.CString()[5], '\0');
}

TEST(aprintf, Formats)
{
	ZTStringView string = aprintf(test.arena, "%s=%d", "x", 42);
	CHECK_EQ(string, "x=42");
	CHECK_EQ(string.length, strlen(string.CString()));
}

TEST(aprintf, EmptyFormat)
{
	ZTStringView string = aprintf(test.arena, "%s", "");
	CHECK_EQ(string.length, 0u);
	REQUIRE(string.CString() != nullptr);
	CHECK_EQ(string.CString()[0], '\0');
}

TEST(aprintf, PrecisionString)
{
	CHECK_EQ(aprintf(test.arena, "%.*s|", 3, "abcdef"), "abc|");
}

TEST(aprintf, LongString)
{
	const size_t length = 10000;
	char* long_string = (char*)test.arena->Allocate(length + 1);
	memset(long_string, 'x', length);
	long_string[length] = '\0';

	ZTStringView string = aprintf(test.arena, "<%s>", long_string);
	CHECK_EQ(string.length, length + 2);
	CHECK_EQ(string.CString()[0], '<');
	CHECK_EQ(string.CString()[length + 1], '>');
	CHECK_EQ(string.CString()[length + 2], '\0');
}

TEST(aprintf, AllocatesExactly)
{
	uint8* before = test.arena->head;
	aprintf(test.arena, "%d", 12345);
	CHECK_EQ(test.arena->head - before, 6);
}
