#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "jsmn.h"


typedef enum {
	TOKEN_LBRACE,
	TOKEN_RBRACE,
	TOKEN_LBRACKET,
	TOKEN_RBRACKET,
	TOKEN_COLON,
	TOKEN_COMMA,
	TOKEN_PRIMITIVE,
	TOKEN_STRING,
} toktype_t;


typedef struct {
	toktype_t type;
	const char *start;
	const char *end;
} lextok_t;

typedef struct {
	const char *pos;
	const char *prev;
} lexer_t;

static jsmnerr_t jsmn_parse_value(lexer_t *lexer, jsmntok_t *value);


static jsmntok_t *jsmn_alloc_token() {
	jsmntok_t *tok;

	// @todo: custom allocator
	tok = (jsmntok_t *)malloc( sizeof( jsmntok_t ) );
	tok->nameStart = tok->nameEnd = nullptr;
	tok->valueStart = tok->valueEnd = nullptr;
	tok->length = 0;
	tok->parent = nullptr;
	tok->next = nullptr;
	tok->child = nullptr;
	return tok;
}


static jsmnerr_t jsmn_lex_primitive(lexer_t *lexer, lextok_t *tok) {
	const char *start;

	start = lexer->pos;
	for (; lexer->pos != '\0'; lexer->pos++) {
		char c;

		c = *lexer->pos;
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
		if (*lexer->pos < 32 || *lexer->pos >= 127) {
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


static jsmnerr_t jsmn_lex_string(lexer_t *lexer, lextok_t *tok) {
	const char *start;
	
	start = lexer->pos;
	if ( *lexer->pos != '"' )
		return JSMN_ERROR_INVAL;
	lexer->pos++;

	for (; *lexer->pos != '\0'; lexer->pos++) {
		char c = *lexer->pos;

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
			switch (*lexer->pos) {
				/* Allowed escaped symbols */
				case '\"': case '/' : case '\\' : case 'b' :
				case 'f' : case 'r' : case 'n'  : case 't' :
					break;
				/* Allows escaped symbol \uXXXX */
				case 'u':
					lexer->pos++;
					for(int i = 0; i < 4 && *lexer->pos != '\0'; i++) {
						/* If it isn't a hex character we have an error */
						c = *lexer->pos;
						if(!((c >= 48 && c <= 57) || /* 0-9 */
							(c >= 65 && c <= 70) || /* A-F */
							(c >= 97 && c <= 102))) { /* a-f */
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


static jsmnerr_t jsmn_lex_control_char(lexer_t *lexer, lextok_t *tok) {
	toktype_t type;

	switch(*lexer->pos) {
	case '{': type = TOKEN_LBRACE; break;
	case '}': type = TOKEN_RBRACE; break;
	case '[': type = TOKEN_LBRACKET; break;
	case ']': type = TOKEN_RBRACKET; break;
	case ':': type = TOKEN_COLON; break;
	case ',': type = TOKEN_COMMA; break;
	default:  return JSMN_ERROR_INVAL;
	}

	tok->type = type;
	tok->start = lexer->pos;
	tok->end = lexer->pos+1;
	lexer->pos++;

	return JSMN_SUCCESS;
}


static void jsmn_lex_backup(lexer_t *lexer) {
	lexer->pos = lexer->prev;
}


static jsmnerr_t jsmn_lex(lexer_t *lexer, lextok_t *tok) {
	// whitespace
	for(;*lexer->pos != '\0'; lexer->pos++) {
		if(*lexer->pos > 32)
			break;
	}

	if(*lexer->pos == '\0') {
		// EOF!
		return JSMN_SUCCESS;
	}

	lexer->prev = lexer->pos;
	char c = *lexer->pos;

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


static jsmnerr_t jsmn_parse_object(lexer_t *lexer, jsmntok_t *value)
{
	jsmntok_t *last;
	bool first;
	lextok_t tok;

	jsmn_lex( lexer, &tok );
	if ( tok.type != TOKEN_LBRACE ) return JSMN_ERROR_INVAL;
	value->type = JSMN_OBJECT;

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
		value->length++;
	}
}


static jsmnerr_t jsmn_parse_array(lexer_t *lexer, jsmntok_t *value)
{
	jsmntok_t *last;
	bool first;
	lextok_t tok;

	jsmn_lex( lexer, &tok );
	if ( tok.type != TOKEN_LBRACKET ) return JSMN_ERROR_INVAL;
	value->type = JSMN_ARRAY;

	last = nullptr;
	first = true;
	for ( ;; ) {
		jsmntok_t *child;

		jsmn_lex( lexer, &tok );
		if ( tok.type == TOKEN_RBRACKET ) return JSMN_SUCCESS;

		if ( !first ) {
			if ( tok.type != TOKEN_COMMA ) return JSMN_ERROR_INVAL;
		}
		else {
			jsmn_lex_backup( lexer );
		}
		first = false;

		child = jsmn_alloc_token();
		if ( jsmn_parse_value( lexer, child ) != JSMN_SUCCESS ) return JSMN_ERROR_INVAL;

		if ( value->child == nullptr )
			value->child = child;
		if ( last != nullptr )
			last->next = child;
		last = child;
		value->length++;
	}
}


static jsmnerr_t jsmn_parse_value(lexer_t *lexer, jsmntok_t *value) {
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


jsmnerr_t jsmn_parse(const char *text, int length, jsmntok_t **value) {
	lexer_t lexer;
	lexer.prev = lexer.pos = text;

	*value = jsmn_alloc_token();
	return jsmn_parse_value( &lexer, *value );
}

