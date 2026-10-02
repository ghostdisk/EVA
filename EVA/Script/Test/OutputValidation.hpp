#pragma once
#include <EVA/Script/Script.hpp>

// Checks backend output with the targets' own tools, for tests and fuzzing: SPIRV-Tools' validator (from the Vulkan SDK,
// when CMake finds it) and fxc (D3DCompile, part of Windows). Each check returns an empty string when the output is
// valid or the tool isn't available, otherwise the tool's message, allocated in arena.

namespace EVA::Script::Validation
{

bool HaveSPIRVTools();
bool HaveFXC();

// Validates a module for Vulkan 1.0.
ZTStringView ValidateSPIRV(Slice<uint32> words, Arena* arena);

// The module as text, for tests. Empty without SPIRV-Tools.
ZTStringView DisassembleSPIRV(Slice<uint32> words, Arena* arena);

// Compiles main with fxc for the stage at shader model 5.0.
ZTStringView CompileHLSL(StringView text, ShaderStage stage, Arena* arena);

}
