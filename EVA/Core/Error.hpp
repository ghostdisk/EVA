#pragma once
#include <EVA/Core/Common.hpp>

namespace EVA
{

// Where an error comes from, and so how to read error_code. Also identifies the Error subtype.
enum class ErrorFamily : uint32
{
	NONE = 0,
	SCRIPT_ERROR, // ScriptError
};

// Arena allocated, along with its message. Subtypes extend it with extra data, identified by error_family.
struct Error
{
	ErrorFamily error_family = ErrorFamily::NONE;
	int64 error_code = 0; // native code for the family, kept as is
	ZTStringView message = {};
};

}
