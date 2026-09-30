#include <EVA/HLSL/HLSL.hpp>
#include <stdlib.h>

namespace EVA::HLSL
{

Slice<uint8> Compile(const char* source)
{
	Lexer lexer = {
		.source = (char*)source,
		.head = (char*)source,
	};

	for (;;)
	{
		if (!LexToken(lexer))
		{
			printf("error: %s\n", lexer.error_buffer);
			exit(1);
		}
		if (lexer.token.token_type == TokenType::END_OF_FILE)
			break;

		printf("%d \"%.*s\"\n", (int)lexer.token.token_type, (int)(lexer.token.end - lexer.token.start), lexer.token.start);
		EatToken(lexer);
	}

	exit(1);
	return {};
}

}
