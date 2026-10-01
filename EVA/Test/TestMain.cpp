#include <EVA/Test/Test.hpp>

// Entry point compiled into each test executable. Kept out of the Test library so a PAL based runner can provide its own.
int main(int argc, char** argv)
{
	return EVA::Test::RunTests(argc, argv);
}
