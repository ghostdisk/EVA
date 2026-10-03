#pragma once
#include <EVA/Script/Script.hpp>
#include <EVA/Script/Script_IR.hpp>

// Shared by the fuzz targets: runs the compiler on an input the way CompileShader does, and checks everything that has
// to hold for any input beyond "doesn't crash". A failed check prints the source and aborts, which libFuzzer reports
// like a crash and saves the input for.

namespace EVA::Script::Fuzz
{

// Sets up panics and failed checks to abort with a message. Called by every target before its first input.
void InitFuzzing();

// Prints the message and the source being compiled, then aborts.
[[noreturn]] void Fail(const char* format, ...);

enum class Stage : uint8
{
	PARSE,
	RESOLVE,
	TYPE,
	INTERFACE, // shaders only
	DONE,      // every stage passed
};

// One run of the front end. Like CompileShader: the AST and context in an intermediate arena, errors in an output
// arena standing in for the caller's.
struct Compilation
{
	Arena* output_arena = nullptr;
	Arena* intermediate_arena = nullptr;
	Context context;
	Node* module = nullptr;
	Stage failed_stage = Stage::DONE;
	Slice<ScriptError*> errors; // in output_arena
	uint32 attribute_count = 0; // '@' tokens in the source, once it parsed
	ShaderInterface shader_interface; // shaders, once typing succeeded
	IRModule ir;                      // once every stage succeeded

	// Shaders, once every stage succeeded: each entry point's output for every target, empty where the backend reported
	// one of its target's limits instead, to the context's errors. In the output arena.
	std::vector<Slice<uint32>> spirv;
	std::vector<ZTStringView> hlsl;
	std::vector<ZTStringView> msl;
};

// Compiles source as far as it gets, checking the invariants of each stage, and for shaders emits every entry point for
// every target. The arenas' blocks are filled with fill, so reading memory nothing wrote shows up as a difference
// between runs with different fills. With validate, the output also has to pass the targets' own tools
// (OutputValidation.hpp), which are slow enough that only one of the runs of an input does it.
void Compile(Compilation& compilation, ZTStringView source, ContextKind kind, uint8 fill, bool validate = false);

// The errors and, if typing was reached, the serialized module, shader interface and IR. Equal for equal inputs. Allocated in arena.
ZTStringView Fingerprint(Compilation& compilation, Arena* arena);

// Destroys the intermediate arena first and then checks the errors, so errors pointing into it are caught.
void Destroy(Compilation& compilation);

// What every target checks for a source: it compiles as a shader twice with different arena contents, with identical
// results, and as a script. check, if given, runs on the first shader compilation before it's destroyed.
void CheckSource(ZTStringView source, void (*check)(Compilation& compilation, void* user) = nullptr, void* user = nullptr);

}
