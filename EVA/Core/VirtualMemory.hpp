#pragma once
#include <EVA/Core/Common.hpp>

namespace EVA
{

static const size_t VIRTUAL_MEMORY_ALIGNMENT = 64 * 1024;

// 0 if large pages can't be used. On Windows that needs the lock memory privilege granted to the account.
size_t GetLargePageSize();

// Committed, zeroed, aligned to VIRTUAL_MEMORY_ALIGNMENT. nullptr on failure.
void* AllocateVirtualMemory(size_t size, bool large_pages);
void FreeVirtualMemory(void* memory, size_t size);

}
