#pragma once
#include <EVA/Core/Common.hpp>
#include <EVA/Core/Arena.hpp>
#include <EVA/Core/Panic.hpp>
#include <type_traits>

// Test framework. Test code is the one place exceptions are allowed: the runner turns panics into PanicException so a
// panic fails only the current test, and tests can catch it to check that something panics.
//
//   TEST(Arena, Allocate)
//   {
//       void* memory = test.arena->Allocate(16);
//       REQUIRE(memory);
//       CHECK_EQ(test.arena->head - (uint8*)memory, 16);
//   }
//
// CHECK records a failure and continues, REQUIRE records it and returns from the test.

namespace EVA::Test
{

// Passed to every test as `test`.
struct Context
{
	Arena* arena = nullptr; // fresh for each test
	uint32 failures = 0;
};

struct TestCase
{
	const char* group = nullptr;
	const char* name = nullptr;
	const char* file = nullptr;
	int line = 0;
	void (*function)(Context& test) = nullptr;
	TestCase* next = nullptr;
	bool failed = false; // set by RunTests
};

// Thrown by the runner's panic handler.
struct PanicException
{
	char message[1024] = {};
};

// Adds the test case to the list RunTests runs, in registration order.
struct Registrar
{
	Registrar(TestCase* test_case);
};

void ReportFailure(Context& test, const char* file, int line, const char* format, ...);

// Formats a value for failure messages.
template <typename T>
void FormatValue(char* buffer, size_t size, const T& value)
{
	if constexpr (std::is_same_v<T, bool>)
		snprintf(buffer, size, "%s", value ? "true" : "false");
	else if constexpr (std::is_enum_v<T> || (std::is_integral_v<T> && std::is_signed_v<T>))
		snprintf(buffer, size, "%lld", (long long)value);
	else if constexpr (std::is_integral_v<T>)
		snprintf(buffer, size, "%llu", (unsigned long long)value);
	else if constexpr (std::is_floating_point_v<T>)
		snprintf(buffer, size, "%g", (double)value);
	else if constexpr (std::is_convertible_v<const T&, StringView>)
	{
		StringView string = value;
		snprintf(buffer, size, "\"%.*s\" (length %zu)", (int)string.length, (const char*)string.data, string.length);
	}
	else if constexpr (std::is_pointer_v<T> || std::is_null_pointer_v<T>)
		snprintf(buffer, size, "%p", (const void*)value);
	else
		snprintf(buffer, size, "<unprintable>");
}

template <typename A, typename B>
bool CheckEqual(Context& test, const char* file, int line, const char* a_text, const char* b_text, const A& a, const B& b)
{
	if (a == b)
		return true;
	char a_value[256];
	char b_value[256];
	FormatValue(a_value, sizeof(a_value), a);
	FormatValue(b_value, sizeof(b_value), b);
	ReportFailure(test, file, line, "CHECK_EQ(%s, %s) failed\n    %s\n    %s", a_text, b_text, a_value, b_value);
	return false;
}

// Runs the registered tests. Arguments: --filter <substring of Group.Name>, --list.
// Returns the process exit code: 0 if every selected test passed.
int RunTests(int argc, char** argv);

}

#define TEST(group, name)                                                                                               \
	static void Test_##group##_##name(EVA::Test::Context& test);                                                        \
	static EVA::Test::TestCase TestCase_##group##_##name = { #group, #name, __FILE__, __LINE__, Test_##group##_##name }; \
	static EVA::Test::Registrar TestRegistrar_##group##_##name(&TestCase_##group##_##name);                             \
	static void Test_##group##_##name([[maybe_unused]] EVA::Test::Context& test)

#define CHECK(expr)                                                                                                     \
	do                                                                                                                  \
	{                                                                                                                   \
		if (!(expr))                                                                                                    \
			EVA::Test::ReportFailure(test, __FILE__, __LINE__, "CHECK(%s) failed", #expr);                             \
	} while (0)

#define REQUIRE(expr)                                                                                                   \
	do                                                                                                                  \
	{                                                                                                                   \
		if (!(expr))                                                                                                    \
		{                                                                                                               \
			EVA::Test::ReportFailure(test, __FILE__, __LINE__, "REQUIRE(%s) failed", #expr);                           \
			return;                                                                                                     \
		}                                                                                                               \
	} while (0)

#define CHECK_EQ(a, b) EVA::Test::CheckEqual(test, __FILE__, __LINE__, #a, #b, (a), (b))

#define REQUIRE_EQ(a, b)                                                                                                \
	do                                                                                                                  \
	{                                                                                                                   \
		if (!EVA::Test::CheckEqual(test, __FILE__, __LINE__, #a, #b, (a), (b)))                                         \
			return;                                                                                                     \
	} while (0)

// Checks that the statement panics. To also check the message, catch EVA::Test::PanicException directly.
#define CHECK_PANICS(statement)                                                                                         \
	do                                                                                                                  \
	{                                                                                                                   \
		bool eva_test_panicked = false;                                                                                 \
		try                                                                                                             \
		{                                                                                                               \
			statement;                                                                                                  \
		}                                                                                                               \
		catch (const EVA::Test::PanicException&)                                                                       \
		{                                                                                                               \
			eva_test_panicked = true;                                                                                   \
		}                                                                                                               \
		if (!eva_test_panicked)                                                                                         \
			EVA::Test::ReportFailure(test, __FILE__, __LINE__, "CHECK_PANICS(%s) didn't panic", #statement);           \
	} while (0)
