#include <EVA/Core/OS.hpp>
#include <EVA/Core/FS.hpp>
#include <Windows.h>
#include <stdarg.h>
#include <stdio.h>

U16* StringToUTF16(Arena* arena, String string, size_t* out_len) {
	int src_len = (int)string.size;

	int wide_len = 0;
	if (src_len > 0)
		wide_len = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)string.data, src_len, nullptr, 0);

	U16* out = (U16*)ArenaAllocate(arena, (wide_len + 1) * sizeof(U16), alignof(U16));
	if (wide_len > 0) 
		MultiByteToWideChar(CP_UTF8, 0, (LPCCH)string.data, src_len, (LPWSTR)out, wide_len);
	out[wide_len] = 0;

	if (out_len) *out_len = (size_t)wide_len;
	return out;
}

ZTString UTF16ToString(Arena* arena, U16* string, int len) {
	bool null_terminated = len < 0;

	int byte_len = 0;
	if (len != 0) {
		byte_len = WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)string, len, nullptr, 0, nullptr, nullptr);
	}

	char* out = (char*)ArenaAllocate(arena, byte_len + 1);
	if (byte_len > 0) {
		WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)string, len, out, byte_len, nullptr, nullptr);
	}
	out[byte_len] = 0;

	size_t size = (null_terminated && byte_len > 0) ? (size_t)(byte_len - 1) : (size_t)byte_len;
	return ZTString(String((U8*)out, size));
}

namespace FS {

void ReadDirectory(String path, void* userdata, void (*callback)(const Stat& stat, void* userdata)) {
	Arena* arena = FrameArena;

	size_t path_units = 0;
	U16*   path_w     = StringToUTF16(arena, path, &path_units);

	U16* pattern = (U16*)ArenaAllocate(arena, (path_units + 3) * sizeof(U16), alignof(U16));
	memcpy(pattern, path_w, path_units * sizeof(U16));
	pattern[path_units + 0] = L'\\';
	pattern[path_units + 1] = L'*';
	pattern[path_units + 2] = 0;

	WIN32_FIND_DATAW fd = {};
	HANDLE handle = FindFirstFileW((LPCWSTR)pattern, &fd);
	if (handle == INVALID_HANDLE_VALUE) return;

	do {
		if (fd.cFileName[0] == '.') continue;

		ZTString filename  = UTF16ToString(arena, (U16*)fd.cFileName, -1);
		ZTString full_path = Fmt(arena, "%.*s/%s", STRING_PRINTF_ARGS(path), filename.c_str());

		Stat stat        = {};
		stat.filename     = filename;
		stat.full_path    = full_path;
		stat.is_directory = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		callback(stat, userdata);
	} while (FindNextFileW(handle, &fd));

	FindClose(handle);
}

}

Result ExecProcess(String cmdline) {
	STARTUPINFOW startup_info{
		.cb = sizeof(startup_info),
	};
	PROCESS_INFORMATION process_info{};
	if (!CreateProcessW(nullptr, (LPWSTR)StringToUTF16(FrameArena, cmdline, nullptr), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup_info, &process_info)) {
		return Err("ExecProcess: CreateProcessW failed (%lu)", GetLastError());
	}
	DEFER(CloseHandle(process_info.hThread));
	DEFER(CloseHandle(process_info.hProcess));

	DWORD wait_result = WaitForSingleObject(process_info.hProcess, INFINITE);
	if (wait_result == WAIT_FAILED) {
		DWORD error = GetLastError();
		return Err("ExecProcess: WaitForSingleObject failed (%lu)", error);
	}

	DWORD exit_code = 0;
	if (!GetExitCodeProcess(process_info.hProcess, &exit_code)) {
		DWORD error = GetLastError();
		return Err("ExecProcess: GetExitCodeProcess failed (%lu)", error);
	}

	if (exit_code != 0) return Err("ExecProcess: process exited with code %lu", exit_code);
	return Success();
}

[[noreturn]] void Fatal(const char* fmt, ...) {
	char message_buffer[2048];
	va_list args;
	va_start(args, fmt);
	vsnprintf(message_buffer, sizeof(message_buffer), fmt, args);
	MessageBoxA(nullptr, message_buffer, "Fatal Error", MB_ICONERROR | MB_OK);
	__debugbreak();
	exit(1);
}

