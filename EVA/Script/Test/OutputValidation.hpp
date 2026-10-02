#pragma once
#include <EVA/Script/Script.hpp>

// Checks backend output with the targets' own tools, for tests and fuzzing: SPIRV-Tools' validator (from the Vulkan SDK,
// when CMake finds it), fxc (D3DCompile, part of Windows) and Metal's compiler (part of macOS, needs a GPU). Each check
// returns an empty string when the output is valid or the tool isn't available, otherwise the tool's message, allocated
// in arena.

namespace EVA::Script::Validation
{

bool HaveSPIRVTools();
bool HaveFXC();
bool HaveMetal();

// Validates a module for Vulkan 1.0.
ZTStringView ValidateSPIRV(Slice<uint32> words, Arena* arena);

// The module as text, for tests. Empty without SPIRV-Tools.
ZTStringView DisassembleSPIRV(Slice<uint32> words, Arena* arena);

// Compiles main with fxc for the stage at shader model 5.0.
ZTStringView CompileHLSL(StringView text, ShaderStage stage, Arena* arena);

// Compiles the text with Metal as MSL 2.0, the way the Metal GPU backend does, and checks it has main0.
ZTStringView CompileMSL(StringView text, Arena* arena);

}
