#include <EVA/Serialization.hpp>
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
	fprintf(out, "}");
	stack.pop_back();
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
