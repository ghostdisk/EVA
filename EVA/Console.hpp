#pragma once

int ConExec(const char* cmd);
void ConLog(const char* fmt, ...);
void ConRegisterCommand(const char* name, int (*function)(int argc, const char** argv), const char* help = "");

void ConsoleInitialize();
void ConsoleDraw();