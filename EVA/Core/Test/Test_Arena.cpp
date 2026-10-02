#include <EVA/Test/Test.hpp>
#include <stdint.h>
#include <string.h>
#include <atomic>
#include <thread>
#include <vector>

using namespace EVA;

TEST(Arena, AllocateAdvancesHead)
{
	Arena* arena = CreateArena();
	DEFER(DestroyArena(arena));

	uint8* first = (uint8*)arena->Allocate(10);
	uint8* second = (uint8*)arena->Allocate(20);
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

TEST(Arena, ExactFitStaysInTheBlock)
{
	Arena* arena = CreateArena();
	DEFER(DestroyArena(arena));

	ArenaBlock* block = arena->current;
	arena->Allocate(arena->end - arena->head);
	CHECK(arena->head == arena->end);
	arena->Allocate(0);
	CHECK(arena->current == block);
	arena->Allocate(1);
	CHECK(arena->current != block);
}

TEST(Arena, GrowsAcrossBlocks)
{
	Arena* arena = CreateArena();
	DEFER(DestroyArena(arena));

	size_t size = GetArenaBlockSize() / 3;
	std::vector<uint8*> allocations;
	for (int i = 0; i < 10; ++i)
	{
		uint8* memory = (uint8*)arena->Allocate(size);
		memset(memory, i, size);
		allocations.push_back(memory);
	}
	for (int i = 0; i < 10; ++i)
	{
		CHECK_EQ(allocations[i][0], (uint8)i);
		CHECK_EQ(allocations[i][size - 1], (uint8)i);
		CHECK(arena->Contains(allocations[i]));
	}
}

TEST(Arena, LargeAllocationsGetABlockOfTheirOwn)
{
	Arena* arena = CreateArena();
	DEFER(DestroyArena(arena));

	uint8* before = (uint8*)arena->Allocate(16);
	size_t size = GetArenaBlockSize() * 3 + 5;
	uint8* large = (uint8*)arena->Allocate(size, 64);
	CHECK_EQ((uintptr_t)large % 64, 0u);
	memset(large, 0xAB, size);
	uint8* after = (uint8*)arena->Allocate(16);
	memset(after, 0xCD, 16);

	CHECK(arena->Contains(before));
	CHECK(arena->Contains(large) && arena->Contains(large + size - 1));
	CHECK(after + 16 <= large || after >= large + size);
	CHECK_EQ(large[0], 0xAB);
	CHECK_EQ(large[size - 1], 0xAB);
}

TEST(Arena, DestroyedBlocksAreReused)
{
	// The pool is a stack.
	Arena* first = CreateArena();
	DestroyArena(first);
	Arena* second = CreateArena();
	CHECK(second == first);
	DestroyArena(second);
}

TEST(Arena, Rewind)
{
	Arena* arena = CreateArena();
	DEFER(DestroyArena(arena));

	arena->Allocate(100);
	ArenaMark mark = arena->Mark();
	void* first = arena->Allocate(16);
	for (int i = 0; i < 5; ++i)
		arena->Allocate(GetArenaBlockSize() / 2);
	arena->Allocate(GetArenaBlockSize() * 2);
	CHECK(arena->current != mark.block);

	arena->Rewind(mark);
	CHECK(arena->current == mark.block);
	CHECK(arena->head == mark.head);
	CHECK(arena->Allocate(16) == first);
}

TEST(Arena, Contains)
{
	uint8* memory = (uint8*)test.arena->Allocate(8);
	CHECK(test.arena->Contains(memory));
	CHECK(test.arena->Contains(memory + 7));
	CHECK(!test.arena->Contains(test.arena->head));
	int local = 0;
	CHECK(!test.arena->Contains(&local));

	Arena* other = CreateArena();
	CHECK(!other->Contains(memory));
	DestroyArena(other);
}

TEST(Arena, Fill)
{
	SetArenaFill(0xCD);
	DEFER(SetArenaFill(-1));
	Arena* arena = CreateArena();
	DEFER(DestroyArena(arena));

	uint8* memory = (uint8*)arena->Allocate(64);
	for (int i = 0; i < 64; ++i)
		CHECK_EQ(memory[i], 0xCD);

	size_t size = GetArenaBlockSize() / 2;
	arena->Allocate(size);
	memory = (uint8*)arena->Allocate(size);
	CHECK_EQ(memory[0], 0xCD);
	CHECK_EQ(memory[size - 1], 0xCD);
}

TEST(Arena, HugeAllocationPanics)
{
	CHECK_PANICS(test.arena->Allocate(SIZE_MAX));
}

TEST(Arena, PanicMessageMentionsSize)
{
	size_t size = SIZE_MAX / 2;
	char expected[32];
	snprintf(expected, sizeof(expected), "%zu", size);
	try
	{
		test.arena->Allocate(size);
		CHECK(!"didn't panic");
	}
	catch (const Test::PanicException& exception)
	{
		CHECK(strstr(exception.message, expected) != nullptr);
	}
}

TEST(Arena, PoolIsThreadSafe)
{
	// A block handed to two arenas at once would get one thread's marks overwritten by another's.
	const int THREADS = 8;
	const int ITERATIONS = 500;
	std::atomic<int> failures = 0;
	std::vector<std::thread> threads;
	for (int t = 0; t < THREADS; ++t)
	{
		threads.emplace_back([&failures, t] {
			size_t size = GetArenaBlockSize() / 2;
			for (int i = 0; i < ITERATIONS; ++i)
			{
				Arena* arena = CreateArena();
				uint8* memory[4];
				for (uint8*& allocation : memory)
				{
					allocation = (uint8*)arena->Allocate(size);
					allocation[0] = allocation[size / 2] = allocation[size - 1] = (uint8)t;
				}
				std::this_thread::yield();
				for (uint8* allocation : memory)
				{
					if (allocation[0] != t || allocation[size / 2] != t || allocation[size - 1] != t)
						failures++;
				}
				DestroyArena(arena);
			}
		});
	}
	for (std::thread& thread : threads)
		thread.join();
	CHECK_EQ(failures.load(), 0);
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
	ArenaMark mark = test.arena->Mark();
	memset(test.arena->Allocate(sizeof(Thing) * 2, alignof(Thing)), 0xCD, sizeof(Thing) * 2);
	test.arena->Rewind(mark);

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
