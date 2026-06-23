#pragma once

enum ConValueType
{
	ConValueType_Null,
	ConValueType_String,
	ConValueType_Number,
	ConValueType_Error,
};

struct ConValue
{
	ConValueType type = ConValueType_Null;
	union
	{
		const char* string;
		float number;
		void* dummy = 0;
	};
};

struct ConVar
{
	const char* name   = nullptr;
	const char* help   = nullptr;
	ConValue    value  = {};
	void (*on_change)(ConVar* cvar) = nullptr;
};

ConValue  ConExec(const char* script);
void      ConLog(const char* fmt, ...);

void ConRegisterVar(ConVar* cvar);
void ConRegisterCommand(const char* name, ConValue (*function)(int num_args, ConValue* args), const char* help = "");

void ConsoleInitialize();
void ConsoleDraw();