#include <stdio.h>
#include <malloc.h>
#include <memory.h>
#include <string.h>
#include "jsmn.h"


#define MAX_TOKENS 4096

int main( int argc, char **argv )
{
	jsmntok_t *value;

	const char *text = "{\"a\":1.234, \"b\":[1,2,3]}";
	jsmn_parse( text, strlen( text ), &value );
}
