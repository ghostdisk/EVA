#include <EVA/Script/Script.hpp>
#include <EVA/Core/Panic.hpp>

namespace EVA::Script
{

Slice<uint8> CompileShader(const char* source)
{
	Parser parser = {
		.source = (char*)source,
		.head = (char*)source,
		.arena = CreateArena(1024 * 1024),
	};
	DEFER(DestroyArena(parser.arena));

	Node* module = nullptr;
	if (!Parse(parser, &module))
	{
		for (ScriptError* error : parser.errors)
			printf("error: %s\n", error->message.CString());
		Panic("shader failed to parse");
	}

	Resolver resolver = { .arena = parser.arena };
	bool resolved = Resolve(resolver, module);
	for (ScriptError* error : resolver.errors)
		printf("error: %s\n", error->message.CString());

	DumpNode(module, parser.arena);
	printf("\n");

	if (!resolved)
		Panic("shader failed to resolve");
	Panic("CompileShader: code generation is not implemented yet");
}

}
