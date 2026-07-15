#pragma once
#include <EVA/Core/Basic.hpp>

class Arena;
struct String;
struct ZTString;

struct BinaryReader {
	const U8*   begin = nullptr;
	const U8*   end   = nullptr;
	const U8*   head  = nullptr;
	bool        ok    = false;
};

struct BinaryWriter {
	Vector<U8> data;
};

inline void BinaryReaderInit(BinaryReader& reader, const U8* data, size_t size) {
	reader.begin = data;
	reader.end = data + size;
	reader.head = data;
	reader.ok = true;
}

inline void ReadBinBytes(BinaryReader& reader, void* out_bytes, size_t size) {
	if (!reader.ok || size > (reader.end - reader.head)) {
		reader.ok = false;
		memset(out_bytes, 0, size);
	} else {
		memcpy(out_bytes, reader.head, size);
		reader.head += size;
	}
}

template <typename T>
T ReadBinT(BinaryReader& reader) {
	T value;
	ReadBinBytes(reader, &value, sizeof(value));
	return value;
}

inline void BinaryWriterInit(BinaryWriter& writer) {
	// just for symmetry with reader :)
}

inline void WriteBinBytes(BinaryWriter& writer, const void* in_bytes, size_t size) {
	size_t head = writer.data.size();
	writer.data.resize(head + size);
	memcpy(writer.data.data() + head, in_bytes, size);
}

template <typename T>
inline void WriteBinT(BinaryWriter& writer, const T& value) {
	WriteBinBytes(writer, &value, sizeof(value));
}

void WriteBinString(BinaryWriter& writer, String str);
ZTString ReadBinString(BinaryReader& reader, Arena* arena, int max_size = -1);