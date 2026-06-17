#include <EVA/Common.hpp>
#include <stdarg.h>
#include <stdio.h>
#include <Windows.h>

[[noreturn]] void Fatal(const char* fmt, ...)
{
	char message_buffer[2048];
	va_list args;
	va_start(args, fmt);
	vsnprintf(message_buffer, sizeof(message_buffer), fmt, args);
	MessageBoxA(nullptr, message_buffer, "Fatal Error", MB_ICONERROR | MB_OK);
	exit(1);
}

bool ReadEntireFile(const char* path, void** out_data, size_t* out_size)
{

	*out_data = nullptr;
	if (out_size) *out_size = 0;

	FILE* f = fopen(path, "rb");
	if (!f)
	{
		fprintf(stderr, "ReadEntireFile: failed to open %s\n", path);
		return false;
	}
	DEFER(fclose(f));

	fseek(f, 0, SEEK_END);
	long length = ftell(f);
	rewind(f);

	if (length < 0)
	{
		fprintf(stderr, "ReadEntireFile: I/O error on %s\n", path);
		return false;
	}

	U8* buffer = (U8*)malloc(length + 1);
	if (!buffer)
	{
		fprintf(stderr, "ReadEntireFile: out of memory\n");
		return false;
	}

	if (length > 0 && fread(buffer, length, 1, f) != 1)
	{
		fprintf(stderr, "ReadEntireFile: I/O error on %s\n", path);
		free(buffer);
		return false;
	}

	buffer[length] = '\0';

	*out_data = buffer;
	if (out_size) *out_size = length;

	return true;
}



void ReplaceFileExtension(char* buffer, size_t buflen, const char* new_ext)
{
	size_t len = strlen(buffer);
	char* candidate = buffer + len;

	for (int i = len - 1; i >= 0; i--)
	{
		if (buffer[i] == '.')
		{
			candidate = buffer + i;
			break;
		}
		if (buffer[i] == '/' || buffer[i] == '\'')
		{
			break;
		}
	}

	snprintf(candidate, buflen - (candidate - buffer), "%s", new_ext);
}