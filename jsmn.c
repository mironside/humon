#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "jsmn.h"

/**
 * Allocates a fresh unused token from the token pull.
 */
static jsmntok_t *jsmn_alloc_token(jsmn_parser *parser) {
	jsmntok_t *tok;

	// @todo: custom allocator
	tok = (jsmntok_t *)malloc( sizeof( jsmntok_t ) );
	tok->start = tok->end = -1;
	tok->length = 0;
	tok->parent = nullptr;
	return tok;
}

/**
 * Fills token type and boundaries.
 */
static void jsmn_fill_token(jsmntok_t *token, jsmntype_t type, 
                            int start, int end) {
	token->type = type;
	token->start = start;
	token->end = end;
	token->length = 0;
}

/**
 * Fills next available token with JSON primitive.
 */
static jsmnerr_t jsmn_parse_primitive(jsmn_parser *parser, const char *js) {
	jsmntok_t *token;
	int start;

	start = parser->pos;

	for (; js[parser->pos] != '\0'; parser->pos++) {
		switch (js[parser->pos]) {
			case '\t' : case '\r' : case '\n' : case ' ' :
			case ','  : case ']'  : case '}' :
				goto found;
		}
		if (js[parser->pos] < 32 || js[parser->pos] >= 127) {
			parser->pos = start;
			return JSMN_ERROR_INVAL;
		}
	}

	/* In strict mode primitive must be followed by a comma/object/array */
	parser->pos = start;
	return JSMN_ERROR_PART;

found:
	token = jsmn_alloc_token(parser);
	if (token == NULL) {
		parser->pos = start;
		return JSMN_ERROR_NOMEM;
	}
	jsmn_fill_token(token, JSMN_PRIMITIVE, start, parser->pos);
	token->parent = parser->toksuper;
	parser->pos--;
	return JSMN_SUCCESS;
}

/**
 * Filsl next token with JSON string.
 */
static jsmnerr_t jsmn_parse_string(jsmn_parser *parser, const char *js, int *begin, int *end) {
	int start;
	
	
	start = parser->pos;
	parser->pos++;

	/* Skip starting quote */
	for (; js[parser->pos] != '\0'; parser->pos++) {
		char c = js[parser->pos];

		/* Quote: end of string */
		if (c == '\"') {
			*begin = start+1;
			*end = parser->pos;
			return JSMN_SUCCESS;
		}

		/* Backslash: Quoted symbol expected */
		if (c == '\\') {
			parser->pos++;
			switch (js[parser->pos]) {
				/* Allowed escaped symbols */
				case '\"': case '/' : case '\\' : case 'b' :
				case 'f' : case 'r' : case 'n'  : case 't' :
					break;
				/* Allows escaped symbol \uXXXX */
				case 'u':
					parser->pos++;
					for(int i = 0; i < 4 && js[parser->pos] != '\0'; i++) {
						/* If it isn't a hex character we have an error */
						if(!((js[parser->pos] >= 48 && js[parser->pos] <= 57) || /* 0-9 */
									(js[parser->pos] >= 65 && js[parser->pos] <= 70) || /* A-F */
									(js[parser->pos] >= 97 && js[parser->pos] <= 102))) { /* a-f */
							parser->pos = start;
							return JSMN_ERROR_INVAL;
						}
						parser->pos++;
					}
					parser->pos--;
					break;
				/* Unexpected symbol */
				default:
					parser->pos = start;
					return JSMN_ERROR_INVAL;
			}
		}
	}
	parser->pos = start;
	return JSMN_ERROR_PART;
}
/*
static const char *parse_object(cJSON *item,const char *value)
{
	object := LBRACE keyvals? RBRACE
	keyvals := keyvals COMMA keyval|keyval
	keyval := STRING COLON value

	token = lex();
	if ( token != LBRACE ) return ERROR;

	for ( ;; )
	{
		token = lex();
		if ( token == RBRACE ) return SUCCESS;
		if ( !first )
		{
			if ( token != COMMA ) return ERROR;
			token = lex();
		}	
		first = false;
		if ( token != STRING ) return ERROR;
		child->name = token;

		token = lex();
		if ( token != COLON ) return ERROR;
		if ( !parse_value( &child ) ) return ERROR;
		last->child = child;
	}



	cJSON *child;
	if (*value!='{')	{ep=value;return 0;}

	item->type=cJSON_Object;
	if (*value=='}') return value+1;
	
	item->child=child=cJSON_New_Item();
	if (!item->child) return 0;

	value=skip(parse_string(child,skip(value)));
	if (!value) return 0;
	child->string=child->valuestring;child->valuestring=0;
	if (*value!=':') {ep=value;return 0;}
	value=skip(parse_value(child,skip(value+1)));
	if (!value) return 0;
	
	while (*value==',')
	{
		cJSON *new_item;
		if (!(new_item=cJSON_New_Item()))	return 0;
		child->next=new_item;new_item->prev=child;child=new_item;
		value=skip(parse_string(child,skip(value+1)));
		if (!value) return 0;
		child->string=child->valuestring;child->valuestring=0;
		if (*value!=':') {ep=value;return 0;}
		value=skip(parse_value(child,skip(value+1)));
		if (!value) return 0;
	}
	
	if (*value=='}') return value+1;
	ep=value;return 0;
}
*/

jsmnerr_t jsmn_parse_object(jsmn_parser *parser, const char *js, int length, jsmntok_t *value)
{
	/*
	object := LBRACE keyvals? RBRACE
	keyvals := keyvals COMMA keyval|keyval
	keyval := STRING COLON value
	*/
	item_t *last;
	token_t token;

	lex( &token );
	if ( token != LBRACE ) return ERROR;

	last = nullptr;
	for ( ;; )
	{
		lex( &token );
		if ( token == RBRACE ) return SUCCESS;

		if ( !first )
		{
			if ( token != COMMA ) return ERROR;
			lex( &token );
		}	
		first = false;

		if ( token != STRING ) return ERROR;
		child->name = token;

		lex( &token );
		if ( token != COLON ) return ERROR;

		if ( !parse_value( &child ) return ERROR;

		if ( value->child == nullptr )
			value->child = child;
		if ( last != nullptr )
			last->next = child;
		last = child;
	}
}

jsmnerr_t jsmn_parse_array(jsmn_parser *parser, const char *js, int length, jsmntok_t *value)
{
	/*
	array := LBRACKET values? RBRACKET
	values := values COMMA value|value
	*/
	item_t *last;
	token_t token;

	lex( &token );
	if ( token != LBRACKET ) return ERROR;

	last = nullptr;
	for ( ;; )
	{
		lex( &token );
		if ( token == RBRACKET ) return SUCCESS;

		if ( !first && token != COMMA ) return ERROR;
		first = false;

		child = new_item();
		if ( !parse_value( &child ) ) return ERROR;

		if ( value->child == nullptr )
			value->child = child;
		if ( last != nullptr )
			last->next = child;
		last = child;
	}
}

/**
 * Parse JSON string and fill tokens.
 */
jsmnerr_t jsmn_parse_value(jsmn_parser *parser, const char *js, int length, jsmntok_t *root) {
	jsmnerr_t r;
	int start;
	int end;
	jsmntok_t *token;

	token = nullptr;
	root = nullptr;
	for (; (int)parser->pos < length; parser->pos++) {
		char c;
		jsmntype_t type;

		c = js[parser->pos];
		switch (c) {
			case '{': case '[':
				token = jsmn_alloc_token(parser);
				if (token == NULL)
					return JSMN_ERROR_NOMEM;
				if (parser->toksuper != nullptr) {
					parser->toksuper->length++;
					token->parent = parser->toksuper;
				}
				token->type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
				token->start = parser->pos;
				parser->toksuper = token;
				break;
			case '}': case ']':
				type = (c == '}' ? JSMN_OBJECT : JSMN_ARRAY);
				if (token == nullptr) {
					return JSMN_ERROR_INVAL;
				}
				for (;;) {
					if (token->start != -1 && token->end == -1) {
						if (token->type != type) {
							return JSMN_ERROR_INVAL;
						}
						token->end = parser->pos + 1;
						parser->toksuper = token->parent;
						break;
					}
					if (token->parent == nullptr) {
						break;
					}
					token = token->parent;
				}
				break;
			case '\"':
				r = jsmn_parse_string(parser, js, &start, &end);
				if (r < 0) return r;

				if ( token->type == JSMN_OBJECT )
				{
					token = jsmn_alloc_token( parser );
					if (parser->toksuper != nullptr)
						parser->toksuper->length++;
					token->nameStart = start;
					token->nameEnd = end;
				}
				// super is a string, this is a value!
				else if ( parser->toksuper->type == JSMN_STRING )
				{
					parser->toksuper->start = start;
					parser->toksuper->end = end;
				}
				else 
				{
					// WTF!
				}
				break;
			case ':':
				if ( !token || token->type != JSMN_OBJECT )
					return JSMN_ERROR_INVAL;
				break;
			case '\t' : case '\r' : case '\n' : case ',': case ' ': 
				break;
			/* In strict mode primitives are: numbers and booleans */
			case '-': case '0': case '1' : case '2': case '3' : case '4':
			case '5': case '6': case '7' : case '8': case '9':
			case 't': case 'f': case 'n' :
				r = jsmn_parse_primitive(parser, js);
				if (r < 0) return r;
				if (parser->toksuper != nullptr)
					parser->toksuper->length++;
				break;

			/* Unexpected char in strict mode */
			default:
				return JSMN_ERROR_INVAL;
		}

		if ( root == nullptr )
			root = token;
	}

	// @todo: can this be detected by having an a parser->parent?
	/*
	for (i = parser->toknext - 1; i >= 0; i--) {
		// Unmatched opened object or array
		if (tokens[i].start != -1 && tokens[i].end == -1) {
			return JSMN_ERROR_PART;
		}
	}
	*/

	return JSMN_SUCCESS;
}

/**
 * Creates a new parser based over a given  buffer with an array of tokens 
 * available.
 */
void jsmn_init(jsmn_parser *parser) {
	parser->pos = 0;
	parser->toksuper = nullptr;
}

