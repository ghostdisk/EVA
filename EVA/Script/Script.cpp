#include <EVA/Script/Script.hpp>
#include <stdlib.h>

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

	Node* declarations = nullptr;
	if (!Parse(parser, &declarations))
	{
		for (ScriptError* error : parser.errors)
			printf("error: %s\n", error->message.CString());
		exit(1);
	}

	for (Node* declaration = declarations; declaration; declaration = declaration->next)
	{
		DumpNode(declaration, parser.arena);
		printf("\n");
	}

	exit(1);
	return {};
}

}
