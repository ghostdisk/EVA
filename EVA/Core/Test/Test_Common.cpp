#include <EVA/Test/Test.hpp>

TEST(Defer, RunsAtScopeExit)
{
	int value = 0;
	{
		DEFER(value = 1);
		CHECK_EQ(value, 0);
	}
	CHECK_EQ(value, 1);
}

TEST(Defer, RunsInReverseOrder)
{
	int order[2] = {};
	int count = 0;
	{
		DEFER(order[count++] = 1);
		DEFER(order[count++] = 2);
	}
	CHECK_EQ(count, 2);
	CHECK_EQ(order[0], 2);
	CHECK_EQ(order[1], 1);
}

static void ReturnEarly(int* value)
{
	DEFER(*value = 1);
	if (*value == 0)
		return;
	*value = 2;
}

TEST(Defer, RunsOnEarlyReturn)
{
	int value = 0;
	ReturnEarly(&value);
	CHECK_EQ(value, 1);
}

TEST(Slice, FromArray)
{
	int values[] = { 1, 2, 3 };
	Slice<int> slice = values;
	CHECK(slice.data == values);
	CHECK_EQ(slice.count, 3u);
	CHECK_EQ(slice[2], 3);
}

TEST(Slice, DefaultIsEmpty)
{
	Slice<int> slice;
	CHECK(slice.data == nullptr);
	CHECK_EQ(slice.count, 0u);
}
