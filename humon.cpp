#include <stdio.h>
#include <malloc.h>
#include <memory.h>
#include "jsmn.h"


#define MAX_TOKENS 4096

int main( int argc, char **argv )
{
	lexer_t lexer;
	jsmntok_t value;

	lexer.input = "{\"a\":1.234, \"b\":[1,2,3]}";
	lexer.pos = 0;
	lexer.prev = 0;

	memset( &value, 0, sizeof( value ) );
	jsmn_parse_value( &lexer, &value );
}
