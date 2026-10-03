#include <EVA/Test/Test.hpp>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>
#ifdef EVA_WIN32
#include <crtdbg.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

namespace EVA::Test
{

static TestCase* first_test = nullptr;
static TestCase* last_test = nullptr;

static const uint32 MAX_GENERATORS = 64;
static void (*generators[MAX_GENERATORS])();
static uint32 generator_count = 0;

static bool update_expected = false;

Registrar::Registrar(TestCase* test_case)
{
	if (last_test)
		last_test->next = test_case;
	else
		first_test = test_case;
	last_test = test_case;
}

GeneratorRegistrar::GeneratorRegistrar(void (*generator)())
{
	if (generator_count == MAX_GENERATORS)
	{
		fprintf(stderr, "more than %u test generators\n", MAX_GENERATORS);
		abort();
	}
	generators[generator_count++] = generator;
}

void AddTest(const char* group, const char* name, const char* file, int line, void (*function)(Context& test), const void* data)
{
	// Lives until the process ends, like the TEST macro's.
	TestCase* test_case = new TestCase{ .group = group, .name = name, .file = file, .line = line, .function = function, .data = data };
	Registrar registrar(test_case);
}

bool UpdateExpected()
{
	return update_expected;
}

void ReportFailure(Context& test, const char* file, int line, const char* format, ...)
{
	test.failures++;
	// file(line): error: is the format IDEs turn into a link.
	printf("%s(%d): error: ", file, line);
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	printf("\n");
	fflush(stdout);
}

static void ThrowPanic(ZTStringView message)
{
	PanicException exception;
	snprintf(exception.message, sizeof(exception.message), "%s", message.CString());
	throw exception;
}

// Failed asserts and crashes would otherwise open dialogs, which hang CI. Report to stderr and exit instead.
static void DisableErrorDialogs()
{
#ifdef EVA_WIN32
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
}

static bool Matches(const TestCase* test_case, const char* filter)
{
	if (!filter)
		return true;
	char full_name[256];
	snprintf(full_name, sizeof(full_name), "%s.%s", test_case->group, test_case->name);
	return strstr(full_name, filter) != nullptr;
}

// Runs one test with a fresh arena, turning panics and exceptions into failures. Returns whether it passed.
static bool RunTest(TestCase* test_case)
{
	printf("[ RUN  ] %s.%s\n", test_case->group, test_case->name);
	fflush(stdout); // so a crash leaves the test's name as the last line

	Context test;
	test.arena = CreateArena();
	test.data = test_case->data;
	auto start = std::chrono::steady_clock::now();
	try
	{
		test_case->function(test);
	}
	catch (const PanicException& exception)
	{
		ReportFailure(test, test_case->file, test_case->line, "panicked: %s", exception.message);
	}
	catch (...)
	{
		ReportFailure(test, test_case->file, test_case->line, "threw an unknown exception");
	}
	auto end = std::chrono::steady_clock::now();
	DestroyArena(test.arena);

	double milliseconds = std::chrono::duration<double, std::milli>(end - start).count();
	printf("[ %s ] %s.%s (%.1f ms)\n", test.failures ? "FAIL" : " OK ", test_case->group, test_case->name, milliseconds);
	fflush(stdout);
	test_case->failed = test.failures > 0;
	return !test_case->failed;
}

int RunTests(int argc, char** argv)
{
	const char* filter = nullptr;
	bool list = false;
	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--filter") == 0 && i + 1 < argc)
			filter = argv[++i];
		else if (strcmp(argv[i], "--list") == 0)
			list = true;
		else if (strcmp(argv[i], "--update") == 0)
			update_expected = true;
		else
		{
			fprintf(stderr, "unknown argument '%s'\nusage: %s [--filter <substring of Group.Name>] [--list] [--update]\n", argv[i],
				argv[0]);
			return 2;
		}
	}

	for (uint32 i = 0; i < generator_count; ++i)
		generators[i]();

	if (list)
	{
		for (TestCase* test_case = first_test; test_case; test_case = test_case->next)
		{
			if (Matches(test_case, filter))
				printf("%s.%s\n", test_case->group, test_case->name);
		}
		return 0;
	}

	DisableErrorDialogs();
	SetPanicHandler(ThrowPanic);
	DEFER(SetPanicHandler(nullptr));

	uint32 passed = 0;
	uint32 failed = 0;
	for (TestCase* test_case = first_test; test_case; test_case = test_case->next)
	{
		if (!Matches(test_case, filter))
			continue;
		if (RunTest(test_case))
			passed++;
		else
			failed++;
	}

	if (passed + failed == 0)
	{
		printf("no tests matched\n");
		return 1;
	}

	printf("\n%u passed, %u failed\n", passed, failed);
	for (TestCase* test_case = first_test; test_case; test_case = test_case->next)
	{
		if (test_case->failed)
			printf("  FAILED %s.%s\n", test_case->group, test_case->name);
	}
	return failed ? 1 : 0;
}

}
