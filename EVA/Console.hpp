#pragma once
#include <EVA/Math.hpp>
#include <EVA/Core/Basic.hpp>

struct ConParser {
	U8* start = nullptr;
	U8* end = nullptr;
	U8* head = nullptr;

	int button = 0; // which button triggered this action, optional

	ZTString StringArg(Arena* arena);
	float FloatArg(float fallback = 0.0f);
	int IntArg(int fallback);
	const char* RestArgs(Arena* arena);
};

struct ConVar {
	const char* name           = nullptr;
	const char* help           = nullptr;
	char        svalue[16]     = {};
	float       fvalue         = 0.0f;
	void (*on_change)(ConVar* cvar) = nullptr;
};

Result  ConExec(ConParser& parser);
Result  ConExec(String, int button = 0);
void    ConLog(const char* fmt, ...);
void    ConError(Result res);
ConVar* ConGetVar(const char* name);

using ConProc = Result (*)(ConParser& p);

void ConRegisterVar(ConVar* cvar);
void ConRegisterCommand(const char* name, ConProc proc, const char* help = "");

void ConsoleInitialize();
void ConsoleDraw();