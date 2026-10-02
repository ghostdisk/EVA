#include <EVA/Core/StringBuilder.hpp>
#include <stdarg.h>
#include <string.h>

namespace EVA
{

static const size_t MIN_CAPACITY = 64;

void StringBuilder::Reserve(size_t extra)
{
	size_t needed = length + extra + 1;
	if (needed <= capacity)
		return;

	size_t new_capacity = capacity * 2 > needed ? capacity * 2 : needed;
	if (new_capacity < MIN_CAPACITY)
		new_capacity = MIN_CAPACITY;

	// Still the arena's last allocation: extend it rather than copying.
	if (data && (uint8*)data + capacity == arena->head && new_capacity - capacity <= (size_t)(arena->end - arena->head))
	{
		arena->Allocate(new_capacity - capacity);
		capacity = new_capacity;
		return;
	}

	char* new_data = (char*)arena->Allocate(new_capacity, 1);
	if (length)
	{
		assert(data);
		memcpy(new_data, data, length);
	}
	new_data[length] = '\0';
	data = new_data;
	capacity = new_capacity;
}

void StringBuilder::Append(StringView string)
{
	Reserve(string.length);
	if (string.length)
		memcpy(data + length, string.data, string.length);
	length += string.length;
	data[length] = '\0';
}

void StringBuilder::AppendFormat(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	va_list measure_args;
	va_copy(measure_args, args);
	int formatted_length = vsnprintf(nullptr, 0, format, measure_args);
	va_end(measure_args);

	if (formatted_length > 0)
	{
		Reserve((size_t)formatted_length);
		vsnprintf(data + length, (size_t)formatted_length + 1, format, args);
		length += (size_t)formatted_length;
	}
	va_end(args);
}

ZTStringView StringBuilder::ToString() const
{
	if (!data)
		return {};

	// Set directly rather than via strlen, which would stop early at a '\0' inside the string.
	ZTStringView result;
	result.data = (uint8*)data;
	result.length = length;
	return result;
}

}
