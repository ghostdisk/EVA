#include <EVA/Core/Serialization.hpp>
#include <EVA/Core/Base64.hpp>
#include <stdio.h>

TextSerializer::TextSerializer(FILE* out) : m_out(out) {
}

void TextSerializer::NextValue() {
	if (stack.size() == 0) return;
	auto& entry = stack.back();

	if (entry.type == '[') {
		if (entry.oneline) {
			if (entry.has_some)
				fputc(' ', m_out);
			entry.has_some = true;
		}
		else
			WriteNewLine();
	}
}

void TextSerializer::WriteNewLine() {
	fprintf(m_out, "\n");
	for (int i = 0; i < stack.size(); i++)
		fprintf(m_out, "\t");
}

void TextSerializer::BeginObject() {
	NextValue();
	fprintf(m_out, "{");
	stack.push_back({ .type = '{' });
}

void TextSerializer::EndObject() {
	stack.pop_back();
	WriteNewLine();
	fprintf(m_out, "}");
}

void TextSerializer::Key(String key) {
	WriteNewLine();
	fprintf(m_out, "%.*s = ", STRING_PRINTF_ARGS(key));
}

void TextSerializer::BeginArray(U32 size) {
	fprintf(m_out, "[:%u", size);
	stack.push_back({ .type = '[' });
}

void TextSerializer::EndArray() {
	stack.pop_back();
	WriteNewLine();
	fprintf(m_out, "]");
}

void TextSerializer::BeginFixedSizeArray(U32 size)  {
	fprintf(m_out, "[");
	stack.push_back({ .type = '[', .oneline = true });
}

void TextSerializer::EndFixedSizeArray() {
	stack.pop_back();
	fprintf(m_out, "]");
}

void TextSerializer::SerializeU64(U64 value) {
	NextValue();
	fprintf(m_out, "%llu", value);
}

void TextSerializer::SerializeI64(I64 value) {
	NextValue();
	fprintf(m_out, "%lld", value);
}

void TextSerializer::SerializeF32(F32 value) {
	NextValue();
	fprintf(m_out, "%f", value);
}

void TextSerializer::SerializeF64(F64 value) {
	NextValue();
	fprintf(m_out, "%f", value);
}

void TextSerializer::SerializeString(String value) {
	fprintf(m_out, "\"%.*s\"", STRING_PRINTF_ARGS(value));
}

void TextSerializer::SerializeBool(bool value) {
	fprintf(m_out, "%s", (value ? "true" : "false"));
}

void TextSerializer::SerializeNull() {
	fprintf(m_out, "null");
}

void TextSerializer::SerializeBytes(const void* bytes, size_t size) {
	size_t b64_size = Base64::GetEncodedBufferSize(size);
	U8* b64 = (U8*)malloc(b64_size); // TODO: scratch arena
	DEFER(free(b64));

	Base64::Encode(b64, bytes, size);

	fprintf(m_out, "B64:%llu:", size);
	fwrite(b64, b64_size, 1, m_out);
}

void TextSerializer::SerializeU8(U8 value) { SerializeU64((U64)value); }
void TextSerializer::SerializeU16(U16 value) { SerializeU64((U64)value); }
void TextSerializer::SerializeU32(U32 value) { SerializeU64((U64)value); }
void TextSerializer::SerializeI8(I8 value) { SerializeI64((I64)value); }
void TextSerializer::SerializeI16(I16 value) { SerializeI64((I64)value); }
void TextSerializer::SerializeI32(I32 value) { SerializeI64((I64)value); }

void TextDeserializer::SkipWhitespace() {
	int c;
	for (;;) {
		c = fgetc(in);
		if (c == EOF) return;
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;

		ungetc(c, in);
		return;
	}
}

TextDeserializer::TextDeserializer(FILE* in, Arena* arena) : in(in) {
	this->arena = arena;
}

bool TextDeserializer::BeginObject() {
	if (this->res.error) return false;

	SkipWhitespace();
	int c = fgetc(in);

	switch (c) {
		case '{': {
			stack.push_back({ .type = '{' });
			return true;
		}
		case 'n': {
			char buf[4] = { "n" };
			fread(buf + 1, 3, 1, in);
			if (String(buf, 4) == "null") {
				return true;
			} else {
				SetError(Err("syntax error at BeginObject"));
				return false;
			}
		}
		default: {
			SetError(Err("syntax error at BeginObject (expected { or null, got '%c')", c));
			return false;
		}
	}
	return false;
}

void TextDeserializer::EndObject() {
	if (this->res.error) return;

	stack.pop_back();
	SkipWhitespace();
	if (fgetc(in) != '}')
		SetError(Err("syntax error at EndObject"));
}

void TextDeserializer::Key(String key) {
	if (this->res.error) return;

	SkipWhitespace();

	assert(key.size < 64);
	char buf[64];
	fread(buf, key.size, 1, in);

	if (memcmp(key.data, buf, key.size) != 0) {
		SetError(Err("syntax error at Key (key mismatch - %.*s vs %.*s)", STRING_PRINTF_ARGS(key), key.size, buf));
		return ;
	}

	SkipWhitespace();
	if (fgetc(in) != '=') {
		SetError(Err("syntax error at Key (expected =)"));
		return;
	}
}

U32 TextDeserializer::BeginArray() {
	if (res.error) return 0;
	SkipWhitespace();
	
	U32 len;
	if (fscanf(in, "[:%u", &len) != 1) {
		SetError(Err("syntax error at BeginArray"));
		return 0;
	}

	stack.push_back({ .type = '[' });
	return len;
}

void TextDeserializer::EndArray() {
	if (res.error) return;

	SkipWhitespace();
	int ch = fgetc(in);
	if (ch != ']') 
		SetError(Err("syntax error at EndArray (expected ], got '%c')", ch));
	stack.pop_back();
}

void TextDeserializer::BeginFixedSizeArray(U32 size) {
	if (res.error) return;
	SkipWhitespace();
	if (fgetc(in) != '[')
		SetError(Err("syntax error at BeginFixedSizeArray"));
}

void TextDeserializer::EndFixedSizeArray() {
	if (res.error) return;
	SkipWhitespace();
	if (fgetc(in) != ']')
		SetError(Err("syntax error at EndFixedSizeArray"));
}

U8  TextDeserializer::DeserializeU8()  { return (U8) DeserializeU64(); }
U16 TextDeserializer::DeserializeU16() { return (U16)DeserializeU64(); }
U32 TextDeserializer::DeserializeU32() { return (U32)DeserializeU64(); }
I8  TextDeserializer::DeserializeI8()  { return (I8) DeserializeI64(); }
I16 TextDeserializer::DeserializeI16() { return (I16)DeserializeI64(); }
I32 TextDeserializer::DeserializeI32() { return (I32)DeserializeI64(); }
F32 TextDeserializer::DeserializeF32() { return (F32)DeserializeF64(); }

U64 TextDeserializer::DeserializeU64() {
	if (this->res.error) return 0;
	U64 value;
	if (fscanf(in, "%llu", &value) != 1)
		SetError(Err("syntax error at DeserializeU64"));
	return value;
}

I64 TextDeserializer::DeserializeI64() {
	if (this->res.error) return 0;
	I64 value;
	if (fscanf(in, "%lld", &value) != 1)
		SetError(Err("syntax error at DeserializeI64"));
	return value;
}

F64 TextDeserializer::DeserializeF64() {
	if (this->res.error) return 0;
	double value;
	if (fscanf(in, "%lf", &value) != 1)
		SetError(Err("syntax error at DeserializeF64"));
	return value;
}

ZTString TextDeserializer::DeserializeString() {
	assert(arena);

	SkipWhitespace();
	if (fgetc(in) != '"')
		SetError(Err("syntax error at DeserializeString"));

	Vector<U8> buffer;
	for (;;) {
		int c = (int)fgetc(in);
		if (c == '"') break;
		if (c == EOF) {
			SetError(Err("unexpected EOF at DeserializeString"));
			return {};
		}
		buffer.push_back((U8)c);
	}
	
	return String(buffer.data(), buffer.size()).CopyToArena(arena);
}

bool TextDeserializer::DeserializeBool() {
	if (this->res.error) return false;
	char buf[10];
	if (fscanf(in, "%9s", buf) != 1)
		SetError(Err("syntax error at DeserializeBool (fscan error)"));
	ZTString str(buf);
	if (str == "true") return true;
	if (str == "false") return false;
	SetError(Err("syntax error at DeserializeBool (not true or false)"));
	return false;
}

void TextDeserializer::DeserializeBytes(Allocator allocator, size_t* out_size, void** out_data) {
	*out_size = 0;
	*out_data = nullptr;

	SkipWhitespace();

	U64 size = 0;
	if (fscanf(in, "B64:%llu:", &size) != 1) {
		SetError(Err("syntax error at DeserializeBytes1"));
		return;
	}

	U64 b64_size = Base64::GetEncodedBufferSize(size);
	U8* b64 = (U8*)ArenaAllocate(FrameArena, b64_size);
	fread(b64, b64_size, 1, in);

	void* out = allocator.Allocate(size, 1);

	*out_size = Base64::Decode(out, size, b64, b64_size);
	*out_data = out;
}

void Serialize(Serializer& s, const SerializableBytes& bytes) {
	s.SerializeBytes(bytes.data, bytes.size);
}

void Deserialize(Deserializer& d, SerializableBytes& bytes) {
	d.DeserializeBytes(d.arena->Alloc(), &bytes.size, &bytes.data);
}