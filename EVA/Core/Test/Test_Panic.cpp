#include <EVA/Test/Test.hpp>
#include <string.h>

using namespace EVA;

TEST(Panic, FormatsMessage)
{
	try
	{
		Panic("bad %s %d", "value", 42);
	}
	catch (const Test::PanicException& exception)
	{
		CHECK_EQ(StringView(exception.message), "bad value 42");
	}
}

TEST(Panic, LongMessageIsTruncated)
{
	char long_string[4096];
	memset(long_string, 'x', sizeof(long_string) - 1);
	long_string[sizeof(long_string) - 1] = '\0';
	try
	{
		Panic("%s", long_string);
	}
	catch (const Test::PanicException& exception)
	{
		CHECK(strlen(exception.message) > 0);
		CHECK(strlen(exception.message) < sizeof(exception.message));
	}
}

TEST(Panic, RunsDestructorsWhenCaught)
{
	bool ran = false;
	try
	{
		DEFER(ran = true);
		Panic("unwind");
	}
	catch (const Test::PanicException&)
	{
		CHECK(ran); // during unwinding, before the handler
	}
	CHECK(ran);
}
