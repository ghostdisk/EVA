#pragma once
#include <EVA/Script/Script.hpp>

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
	DONE, // every stage passed
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
};

// Compiles source as far as it gets, checking the invariants of each stage. The arenas' blocks are filled with fill, so
// reading memory nothing wrote shows up as a difference between runs with different fills.
// Code generation isn't implemented yet, so this stops after typing, where CompileShader panics.
void Compile(Compilation& compilation, ZTStringView source, ContextKind kind, uint8 fill);

// The errors and, if parsing succeeded, the serialized module. Equal for equal inputs. Allocated in arena.
ZTStringView Fingerprint(Compilation& compilation, Arena* arena);

// Destroys the intermediate arena first and then checks the errors, so errors pointing into it are caught.
void Destroy(Compilation& compilation);

// What every target checks for a source: it compiles as a shader twice with different arena contents, with identical
// results, and as a script. check, if given, runs on the first shader compilation before it's destroyed.
void CheckSource(ZTStringView source, void (*check)(Compilation& compilation, void* user) = nullptr, void* user = nullptr);

}
