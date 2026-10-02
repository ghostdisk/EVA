#include <EVA/Core/VirtualMemory.hpp>
#ifdef EVA_WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#else
#include <sys/mman.h>
#endif

namespace EVA
{

static size_t RoundUp(size_t size, size_t alignment)
{
	assert(alignment);
	return (size + alignment - 1) / alignment * alignment;
}

#ifdef EVA_WIN32

static bool EnableLockMemoryPrivilege()
{
	HANDLE token = nullptr;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
		return false;
	TOKEN_PRIVILEGES privileges = {};
	privileges.PrivilegeCount = 1;
	privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	bool enabled = LookupPrivilegeValueW(nullptr, L"SeLockMemoryPrivilege", &privileges.Privileges[0].Luid) &&
				   AdjustTokenPrivileges(token, FALSE, &privileges, 0, nullptr, nullptr) &&
				   GetLastError() == ERROR_SUCCESS; // not ERROR_NOT_ALL_ASSIGNED
	CloseHandle(token);
	return enabled;
}

static size_t DetectLargePageSize()
{
	size_t size = GetLargePageMinimum();
	if (!size || size % VIRTUAL_MEMORY_ALIGNMENT || !EnableLockMemoryPrivilege())
		return 0;
	void* test = VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
	if (!test)
		return 0;
	VirtualFree(test, 0, MEM_RELEASE);
	return size;
}

size_t GetLargePageSize()
{
	static size_t size = DetectLargePageSize();
	return size;
}

void* AllocateVirtualMemory(size_t size, bool large_pages)
{
	DWORD type = MEM_RESERVE | MEM_COMMIT;
	if (large_pages)
	{
		size = RoundUp(size, GetLargePageSize());
		type |= MEM_LARGE_PAGES;
	}
	else
		size = RoundUp(size, VIRTUAL_MEMORY_ALIGNMENT);
	return VirtualAlloc(nullptr, size, type, PAGE_READWRITE);
}

void FreeVirtualMemory(void* memory, size_t size)
{
	(void)size;
	VirtualFree(memory, 0, MEM_RELEASE);
}

#else

size_t GetLargePageSize()
{
	return 0;
}

void* AllocateVirtualMemory(size_t size, bool large_pages)
{
	(void)large_pages;
	size = RoundUp(size, VIRTUAL_MEMORY_ALIGNMENT);
	if (size < VIRTUAL_MEMORY_ALIGNMENT) // overflowed
		return nullptr;

	// mmap only aligns to pages: map extra and trim.
	size_t mapped_size = size + VIRTUAL_MEMORY_ALIGNMENT;
	uint8* mapped = (uint8*)mmap(nullptr, mapped_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (mapped == MAP_FAILED)
		return nullptr;
	uint8* aligned = (uint8*)RoundUp((size_t)mapped, VIRTUAL_MEMORY_ALIGNMENT);
	if (aligned > mapped)
		munmap(mapped, aligned - mapped);
	if (mapped + mapped_size > aligned + size)
		munmap(aligned + size, mapped + mapped_size - (aligned + size));
	return aligned;
}

void FreeVirtualMemory(void* memory, size_t size)
{
	munmap(memory, RoundUp(size, VIRTUAL_MEMORY_ALIGNMENT));
}

#endif

}
