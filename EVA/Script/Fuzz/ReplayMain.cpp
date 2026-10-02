#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <filesystem>
#include <vector>

// main() for the fuzz targets in builds without libFuzzer. Runs LLVMFuzzerTestOneInput on each file given and every file
// in each directory given: the seeds and regressions as CTest tests in normal builds, or one crashing input under a
// debugger. A failure aborts, like under libFuzzer. --quiet leaves out the name of each input.
// std::filesystem since this is tooling and there's no file API yet.

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

static bool quiet = false;

static bool RunFile(const std::filesystem::path& path)
{
	FILE* file = fopen(path.string().c_str(), "rb");
	if (!file)
	{
		fprintf(stderr, "can't open %s\n", path.string().c_str());
		return false;
	}
	std::vector<uint8_t> data;
	uint8_t buffer[4096];
	for (;;)
	{
		size_t read = fread(buffer, 1, sizeof(buffer), file);
		data.insert(data.end(), buffer, buffer + read);
		if (read < sizeof(buffer))
			break; // end of file or an error
	}
	fclose(file);

	if (!quiet)
	{
		printf("%s\n", path.string().c_str());
		fflush(stdout); // so a crash leaves the input's name as the last line
	}
	LLVMFuzzerTestOneInput(data.data(), data.size());
	return true;
}

static int Run(int argc, char** argv)
{
	if (argc < 2)
	{
		fprintf(stderr, "usage: %s [--quiet] <file or directory>...\n", argv[0]);
		return 1;
	}

	bool ok = true;
	size_t count = 0;
	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--quiet") == 0)
		{
			quiet = true;
			continue;
		}
		std::filesystem::path path = argv[i];
		if (!std::filesystem::is_directory(path))
		{
			ok = RunFile(path) && ok;
			count++;
			continue;
		}
		std::vector<std::filesystem::path> files;
		for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
		{
			if (entry.is_regular_file())
				files.push_back(entry.path());
		}
		std::sort(files.begin(), files.end());
		for (const std::filesystem::path& file : files)
		{
			ok = RunFile(file) && ok;
			count++;
		}
	}
	printf("ran %zu inputs\n", count);
	return ok ? 0 : 1;
}

int main(int argc, char** argv)
{
	// std::filesystem throws, e.g. for a directory that can't be read.
	try
	{
		return Run(argc, argv);
	}
	catch (const std::exception& exception)
	{
		fprintf(stderr, "%s\n", exception.what());
		return 1;
	}
}
