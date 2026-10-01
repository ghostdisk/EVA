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
		printf("error: %s\n", parser.error_buffer);
		exit(1);
	}

	exit(1);
	return {};
}

}
