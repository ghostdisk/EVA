#define _CRT_SECURE_NO_WARNINGS // for fopen
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <filesystem>
#include <vector>

// main() for the fuzz targets in builds without libFuzzer. Runs LLVMFuzzerTestOneInput on each file given and every file
// in each directory given: the seeds and regressions as CTest tests in normal builds, or one crashing input under a
// debugger. A failure aborts, like under libFuzzer. std::filesystem since this is tooling and EVA/OS has no file API yet.

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

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
	size_t read;
	while ((read = fread(buffer, 1, sizeof(buffer), file)) > 0)
		data.insert(data.end(), buffer, buffer + read);
	fclose(file);

	printf("%s\n", path.string().c_str());
	fflush(stdout); // so a crash leaves the input's name as the last line
	LLVMFuzzerTestOneInput(data.data(), data.size());
	return true;
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		fprintf(stderr, "usage: %s <file or directory>...\n", argv[0]);
		return 1;
	}

	bool ok = true;
	size_t count = 0;
	for (int i = 1; i < argc; ++i)
	{
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
