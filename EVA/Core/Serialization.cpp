#include <EVA/Core/Serialization.hpp>
#include <stdio.h>

TextSerializer::TextSerializer(FILE* out) : out(out) {
}

void TextSerializer::NextValue() {
	if (stack.size() == 0) return;
	if (stack.back().type == '[')
		WriteNewLine();
}

void TextSerializer::WriteNewLine() {
	fprintf(out, "\n");
	for (int i = 0; i < stack.size(); i++)
		fprintf(out, "\t");
}

void TextSerializer::BeginObject() {
	NextValue();
	fprintf(out, "{");
	stack.push_back({ '{' });
}

void TextSerializer::EndObject() {
	stack.pop_back();
	WriteNewLine();
	fprintf(out, "}");
}

void TextSerializer::Key(String key) {
	WriteNewLine();
	fprintf(out, "%.*s = ", STRING_PRINTF_ARGS(key));
}

void TextSerializer::BeginArray(U32 size) {
	fprintf(out, "[:%u", size);
	stack.push_back({ '[' });
}

void TextSerializer::EndArray() {
	stack.pop_back();
	WriteNewLine();
	fprintf(out, "]");
}

void TextSerializer::SerializeU64(U64 value) {
	NextValue();
	fprintf(out, "%llu", value);
}

void TextSerializer::SerializeI64(I64 value) {
	NextValue();
	fprintf(out, "%lld", value);
}

void TextSerializer::SerializeF32(F32 value) {
	NextValue();
	fprintf(out, "%f", value);
}

void TextSerializer::SerializeF64(F64 value) {
	NextValue();
	fprintf(out, "%f", value);
}

void TextSerializer::SerializeString(String value) {
	fprintf(out, "\"%.*s\"", STRING_PRINTF_ARGS(value));
}

void TextSerializer::SerializeBool(bool value) {
	fprintf(out, "%s", (value ? "true" : "false"));
}

void TextSerializer::SerializeNull() {
	fprintf(out, "null");
}

void TextSerializer::SerializeBytes(U8* bytes, size_t size) {
	fprintf(out, "B:%llu:", size);
	fwrite(bytes, size, 1, out);
}

void TextSerializer::SerializeU8(U8 value) { SerializeU64((U64)value); }
void TextSerializer::SerializeU16(U16 value) { SerializeU64((U64)value); }
void TextSerializer::SerializeU32(U32 value) { SerializeU64((U64)value); }
void TextSerializer::SerializeI8(I8 value) { SerializeI64((I64)value); }
void TextSerializer::SerializeI16(I16 value) { SerializeI64((I64)value); }
void TextSerializer::SerializeI32(I32 value) { SerializeI64((I64)value); }

void TextDeserializer::SkipWhitespace(bool new_lines_too) {
	int c;
	for (;;) {
		c = fgetc(in);
		if (c == EOF) return;
		if (c == ' ' || c == '\t') continue;
		if (new_lines_too && (c == '\n' || c == '\r')) continue;

		ungetc(c, in);
		return;
	}
}

TextDeserializer::TextDeserializer(FILE* in) : in(in) {
}

bool TextDeserializer::BeginObject() {
	if (this->res.error) return false;

	SkipWhitespace(false);
	int c = fgetc(in);

	switch (c) {
		case '{': {
			stack.push_back({ '{' });
			return true;
		}
		case 'n': {
			char buf[4] = { "n" };
			fread(buf + 1, 3, 1, in);
			if (String(buf, 4) == "null") {
				return true;
			} else {
				res = Err("invalid syntax");
				return false;
			}
		}
		default: {
			return false;
		}
	}
	return false;
}

void TextDeserializer::EndObject() {
	if (this->res.error) return;

	stack.pop_back();
	SkipWhitespace(true);
	if (fgetc(in) != '}')
		res = Err("expected }");
}

void TextDeserializer::Key(String key) {
	if (this->res.error) return;

	SkipWhitespace(true);

	assert(key.size < 64);
	char buf[64];
	fread(buf, key.size, 1, in);

	if (memcmp(key.data, buf, key.size) != 0) {
		res = Err("unexpected key");
		return ;
	}

	SkipWhitespace(false);
	if (fgetc(in) != '=') {
		res = Err("expected =");
		return;
	}
	SkipWhitespace(false);
}

U32 TextDeserializer::BeginArray() {
	assert(0);
	return 0;
}

void TextDeserializer::EndArray() {
	assert(0);
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
		res = Err("fscan error");
	return value;
}

I64 TextDeserializer::DeserializeI64() {
	if (this->res.error) return 0;
	I64 value;
	if (fscanf(in, "%lld", &value) != 1)
		res = Err("fscan error");
	return value;
}

F64 TextDeserializer::DeserializeF64() {
	if (this->res.error) return 0;
	double value;
	if (fscanf(in, "%lf", &value) != 1)
		res = Err("fscan error");
	return value;
}

ZTString TextDeserializer::DeserializeString() {
	assert(0);
	return {};
}

bool TextDeserializer::DeserializeBool() {
	if (this->res.error) return false;
	char buf[10];
	if (fscanf(in, "%9s", buf) != 1)
		res = Err("fscan error");
	ZTString str(buf);
	if (str == "true") return true;
	if (str == "false") return false;
	res = Err("invalid bool");
	return false;
}

size_t TextDeserializer::DeserializeBytes1() {
	assert(0);
	return 0;
}

void TextDeserializer::DeserializeBytes2(U8* out_bytes) {
	assert(0);
}