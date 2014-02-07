#ifndef __JSMN_H_
#define __JSMN_H_

/**
 * JSON type identifier. Basic types are:
 * 	o Object
 * 	o Array
 * 	o String
 * 	o Other primitive: number, boolean (true/false) or null
 */
typedef enum {
	JSMN_PRIMITIVE = 0,
	JSMN_OBJECT = 1,
	JSMN_ARRAY = 2,
	JSMN_STRING = 3
} jsmntype_t;

typedef enum {
	/* Not enough tokens were provided */
	JSMN_ERROR_NOMEM = -1,
	/* Invalid character inside JSON string */
	JSMN_ERROR_INVAL = -2,
	/* The string is not a full JSON packet, more bytes expected */
	JSMN_ERROR_PART = -3,
	/* Everything was fine */
	JSMN_SUCCESS = 0
} jsmnerr_t;

/**
 * JSON token description.
 * @param		type	type (object, array, string etc.)
 * @param		start	start position in JSON data string
 * @param		end		end position in JSON data string
 */
typedef struct jsmntok_s {
	jsmntype_t type;
	const char *nameStart;
	const char *nameEnd;
	const char *valueStart;
	const char *valueEnd;
	int length;
	struct jsmntok_s *parent;
	struct jsmntok_s *next;
	struct jsmntok_s *child;
} jsmntok_t;

/**
 * JSON parser. Contains an array of token blocks available. Also stores
 * the string being parsed now and current position in that string
 */
typedef struct {
	unsigned int pos; /* offset in the JSON string */
	jsmntok_t *toksuper; /* superior token node, e.g parent object or array */
} jsmn_parser;

/**
 * Create JSON parser over an array of tokens
 */
void jsmn_init(jsmn_parser *parser);

/**
 * Run JSON parser. It parses a JSON data string into and array of tokens, each describing
 * a single JSON object.
 */


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
	int start;
	int end;
} lextok_t;

typedef struct {
	const char *input;
	int pos;
	int prev;
} lexer_t;

jsmnerr_t jsmn_lex_primitive(lexer_t *lexer, lextok_t *tok);
jsmnerr_t jsmn_lex_string(lexer_t *lexer, lextok_t *tok);
jsmnerr_t jsmn_lex(lexer_t *lexer, lextok_t *tok);
jsmnerr_t jsmn_parse_value(lexer_t *lexer, jsmntok_t *value);


#endif /* __JSMN_H_ */
