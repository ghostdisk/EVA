#include <EVA/Core/Basic.hpp>
#include <EVA/Core/Binary.hpp>
#include <stdarg.h>
#include <stdio.h>

bool ReadEntireFile(const char* path, void** out_data, size_t* out_size) {
	*out_data = nullptr;
	if (out_size) *out_size = 0;

	FILE* f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "ReadEntireFile: failed to open %s\n", path);
		return false;
	}
	DEFER(fclose(f));

	fseek(f, 0, SEEK_END);
	long length = ftell(f);
	rewind(f);

	if (length < 0) {
		fprintf(stderr, "ReadEntireFile: I/O error on %s\n", path);
		return false;
	}

	U8* buffer = (U8*)malloc(length + 1);
	if (!buffer) {
		fprintf(stderr, "ReadEntireFile: out of memory\n");
		return false;
	}

	if (length > 0 && fread(buffer, length, 1, f) != 1) {
		fprintf(stderr, "ReadEntireFile: I/O error on %s\n", path);
		free(buffer);
		return false;
	}

	buffer[length] = '\0';

	*out_data = buffer;
	if (out_size) *out_size = length;

	return true;
}

void WriteBinString(BinaryWriter& writer, String str) {
	WriteBinT<U32>(writer, str.size);
	WriteBinBytes(writer, str.data, str.size);
}

ZTString ReadBinString(BinaryReader& reader, Arena* arena, int max_size) {
	ZTString str;
	str.size = ReadBinT<U32>(reader);
	if (str.size > max_size) {
		reader.ok = false;
		return {};
	}
	str.data = (U8*)ArenaAllocate(arena, str.size + 1);
	ReadBinBytes(reader, str.data, str.size);
	if (!reader.ok) {
		return {};
	}
	str.data[str.size] = '\0';
	return str;
}
