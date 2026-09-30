#include <EVA/HLSL/HLSL.hpp>
#include <stdlib.h>

namespace EVA::HLSL
{

Slice<uint8> Compile(const char* source)
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
