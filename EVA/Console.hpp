#pragma once
#include <EVA/Math.hpp>

struct ConParser
{
	const char* start;
	const char* end;
	const char* head;

	int button = 0; // which button triggered this action, optional

	const char* StringArg();
	float FloatArg(float fallback = 0.0f);
	int IntArg(int fallback);
	const char* RestArgs();
};

struct ConVar
{
	const char* name           = nullptr;
	const char* help           = nullptr;
	char        svalue[16]     = {};
	float       fvalue         = 0.0f;
	void (*on_change)(ConVar* cvar) = nullptr;
};

void    ConExec(ConParser& parser);
void    ConExec(const char* script, int button = 0);
void    ConLog(const char* fmt, ...);
void    ConError(const char* fmt, ...);
ConVar* ConGetVar(const char* name);

using ConProc = void (*)(ConParser& p);

void ConRegisterVar(ConVar* cvar);
void ConRegisterCommand(const char* name, ConProc proc, const char* help = "");

void ConsoleInitialize();
void ConsoleDraw();