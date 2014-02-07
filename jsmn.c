#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "jsmn.h"


/**
 * Allocates a fresh unused token from the token pull.
 */
static jsmntok_t *jsmn_alloc_token() {
	jsmntok_t *tok;

	// @todo: custom allocator
	tok = (jsmntok_t *)malloc( sizeof( jsmntok_t ) );
	tok->nameStart = tok->nameEnd = -1;
	tok->valueStart = tok->valueEnd = -1;
	tok->length = 0;
	tok->parent = nullptr;
	tok->next = nullptr;
	tok->child = nullptr;
	return tok;
}

/**
 * Fills token type and boundaries.
 */
/*
static void jsmn_fill_token(jsmntok_t *token, jsmntype_t type, 
                            int start, int end) {
	token->type = type;
	token->start = start;
	token->end = end;
	token->length = 0;
}
*/

/**
 * Fills next available token with JSON primitive.
 */
jsmnerr_t jsmn_lex_primitive(lexer_t *lexer, lextok_t *tok) {
	int start;

	start = lexer->pos;
	for (; lexer->input[lexer->pos] != '\0'; lexer->pos++) {
		char c;

		c = lexer->input[lexer->pos];
		switch(c) {
		case '\t':
		case '\r':
		case '\n':
		case ' ':
		case ':':
		case ',':
		case ']':
		case '}':
			goto found;
		}
		if (lexer->input[lexer->pos] < 32 || lexer->input[lexer->pos] >= 127) {
			lexer->pos = start;
			return JSMN_ERROR_INVAL;
		}
	}

	if ( start == lexer->pos )
		return JSMN_ERROR_INVAL;

found:
	tok->type = TOKEN_PRIMITIVE;
	tok->start = start;
	tok->end = lexer->pos;
	return JSMN_SUCCESS;
}

/**
 * Filsl next token with JSON string.
 */
jsmnerr_t jsmn_lex_string(lexer_t *lexer, lextok_t *tok) {
	int start;
	
	start = lexer->pos;
	if ( lexer->input[lexer->pos] != '"' )
		return JSMN_ERROR_INVAL;
	lexer->pos++;

	for (; lexer->input[lexer->pos] != '\0'; lexer->pos++) {
		char c = lexer->input[lexer->pos];

		/* Quote: end of string */
		if (c == '\"') {
			tok->type = TOKEN_STRING;
			tok->start = start+1;
			tok->end = lexer->pos;
			lexer->pos++;
			return JSMN_SUCCESS;
		}

		/* Backslash: Quoted symbol expected */
		if (c == '\\') {
			lexer->pos++;
			switch (lexer->input[lexer->pos]) {
				/* Allowed escaped symbols */
				case '\"': case '/' : case '\\' : case 'b' :
				case 'f' : case 'r' : case 'n'  : case 't' :
					break;
				/* Allows escaped symbol \uXXXX */
				case 'u':
					lexer->pos++;
					for(int i = 0; i < 4 && lexer->input[lexer->pos] != '\0'; i++) {
						/* If it isn't a hex character we have an error */
						if(!((lexer->input[lexer->pos] >= 48 && lexer->input[lexer->pos] <= 57) || /* 0-9 */
									(lexer->input[lexer->pos] >= 65 && lexer->input[lexer->pos] <= 70) || /* A-F */
									(lexer->input[lexer->pos] >= 97 && lexer->input[lexer->pos] <= 102))) { /* a-f */
							lexer->pos = start;
							return JSMN_ERROR_INVAL;
						}
						lexer->pos++;
					}
					lexer->pos--;
					break;
				/* Unexpected symbol */
				default:
					lexer->pos = start;
					return JSMN_ERROR_INVAL;
			}
		}
	}

	return JSMN_ERROR_INVAL;
}

jsmnerr_t jsmn_lex_control_char(lexer_t *lexer, lextok_t *tok) {
	char c;
	toktype_t type;

	c = lexer->input[lexer->pos];

	switch(c) {
	case '{':
		type = TOKEN_LBRACE;
		break;
	case '}':
		type = TOKEN_RBRACE;
		break;
	case '[':
		type = TOKEN_LBRACKET;
		break;
	case ']':
		type = TOKEN_RBRACKET;
		break;
	case ':':
		type = TOKEN_COLON;
		break;
	case ',':
		type = TOKEN_COMMA;
		break;
	default:
		return JSMN_ERROR_INVAL;
	}

	tok->type = type;
	tok->start = lexer->pos;
	tok->end = lexer->pos+1;
	lexer->pos++;

	return JSMN_SUCCESS;
}

void jsmn_lex_backup(lexer_t *lexer) {
	lexer->pos = lexer->prev;
}

jsmnerr_t jsmn_lex(lexer_t *lexer, lextok_t *tok) {
	// whitespace
	for(;lexer->input[lexer->pos] != '\0'; lexer->pos++) {
		if(lexer->input[lexer->pos] > 32)
			break;
	}

	if(lexer->input[lexer->pos] == '\0') {
		// EOF!
		return JSMN_SUCCESS;
	}

	lexer->prev = lexer->pos;
	char c = lexer->input[lexer->pos];

	switch(c) {
	case '[':
	case '{':
	case ']':
	case '}':
	case ':':
	case ',':
		return jsmn_lex_control_char(lexer, tok);
	case '\"':
		jsmn_lex_string(lexer, tok);
		break;
	default:
		if (c=='t' || c=='f' || c=='n' || c=='-' || (c>='0' && c<='9'))
			return jsmn_lex_primitive(lexer, tok);
	}

	return JSMN_ERROR_INVAL;
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

jsmnerr_t jsmn_parse_object(lexer_t *lexer, jsmntok_t *value)
{
	/*
	object := LBRACE keyvals? RBRACE
	keyvals := keyvals COMMA keyval|keyval
	keyval := STRING COLON value
	*/
	jsmntok_t *last;
	bool first;
	lextok_t tok;

	jsmn_lex( lexer, &tok );
	if ( tok.type != TOKEN_LBRACE ) return JSMN_ERROR_INVAL;

	last = nullptr;
	first = true;
	for ( ;; ) {
		jsmntok_t *child;

		jsmn_lex( lexer, &tok );
		if ( tok.type == TOKEN_RBRACE ) return JSMN_SUCCESS;

		if ( !first ) {
			if ( tok.type != TOKEN_COMMA ) return JSMN_ERROR_INVAL;
			jsmn_lex( lexer, &tok );
		}	
		first = false;

		if ( tok.type != TOKEN_STRING ) return JSMN_ERROR_INVAL;

		child = jsmn_alloc_token();
		child->nameStart = tok.start;
		child->nameEnd = tok.end;

		jsmn_lex( lexer, &tok );
		if ( tok.type != TOKEN_COLON ) return JSMN_ERROR_INVAL;

		if ( jsmn_parse_value( lexer, child ) != JSMN_SUCCESS ) return JSMN_ERROR_INVAL;

		if ( value->child == nullptr )
			value->child = child;
		if ( last != nullptr )
			last->next = child;
		last = child;
	}
}

jsmnerr_t jsmn_parse_array(lexer_t *lexer, jsmntok_t *value)
{
	/*
	array := LBRACKET values? RBRACKET
	values := values COMMA value|value
	*/
	jsmntok_t *last;
	bool first;
	lextok_t tok;

	jsmn_lex( lexer, &tok );
	if ( tok.type != TOKEN_LBRACKET ) return JSMN_ERROR_INVAL;

	last = nullptr;
	first = true;
	for ( ;; ) {
		jsmntok_t *child;

		jsmn_lex( lexer, &tok );
		if ( tok.type == TOKEN_RBRACKET ) return JSMN_SUCCESS;

		if ( !first && tok.type != TOKEN_COMMA ) return JSMN_ERROR_INVAL;
		first = false;

		child = jsmn_alloc_token();
		if ( jsmn_parse_value( lexer, child ) != JSMN_SUCCESS ) return JSMN_ERROR_INVAL;

		if ( value->child == nullptr )
			value->child = child;
		if ( last != nullptr )
			last->next = child;
		last = child;
	}
}

jsmnerr_t jsmn_parse_value(lexer_t *lexer, jsmntok_t *value) {
	lextok_t tok;

	if(jsmn_lex(lexer, &tok) != JSMN_SUCCESS) return JSMN_ERROR_INVAL;

	switch(tok.type) {
	case TOKEN_LBRACE:
		jsmn_lex_backup(lexer);
		return jsmn_parse_object(lexer, value);
	case TOKEN_LBRACKET:
		jsmn_lex_backup(lexer);
		return jsmn_parse_array(lexer, value);
	case TOKEN_STRING:
		value->type = JSMN_STRING;
		value->valueStart = tok.start;
		value->valueEnd = tok.end;
		return JSMN_SUCCESS;
	case TOKEN_PRIMITIVE:
		value->type = JSMN_PRIMITIVE;
		value->valueStart = tok.start;
		value->valueEnd = tok.end;
		return JSMN_SUCCESS;
	}

	return JSMN_ERROR_INVAL;
}

#if 0
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
#endif