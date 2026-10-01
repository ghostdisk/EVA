#include <EVA/Script/Script.hpp>
#include <EVA/Core/Panic.hpp>

namespace EVA::Script
{

// Copies the list of errors into arena, where the errors themselves already are.
static Slice<ScriptError*> ToSlice(Arena* arena, const std::vector<ScriptError*>& errors)
{
	ScriptError** data = (ScriptError**)arena->Allocate(errors.size() * sizeof(ScriptError*), alignof(ScriptError*));
	for (size_t i = 0; i < errors.size(); ++i)
		data[i] = errors[i];
	return Slice<ScriptError*>(data, (uint32)errors.size());
}

CompileShaderResult CompileShader(Arena* arena, ZTStringView source)
{
	Arena* intermediate_arena = CreateArena(1024 * 1024);
	DEFER(DestroyArena(intermediate_arena));

	Parser parser = {
		.source = (char*)source.CString(),
		.head = (char*)source.CString(),
		.arena = intermediate_arena,
		.error_arena = arena,
	};
	Node* module = nullptr;
	if (!Parse(parser, &module))
		return { .errors = ToSlice(arena, parser.errors) };

	Resolver resolver = { .arena = intermediate_arena, .error_arena = arena };
	if (!Resolve(resolver, module))
		return { .errors = ToSlice(arena, resolver.errors) };

	DumpNode(module, intermediate_arena);
	printf("\n");
	Panic("CompileShader: code generation is not implemented yet");
}

}
