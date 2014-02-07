#ifndef __JSMN_H_
#define __JSMN_H_

typedef enum {
	JSMN_PRIMITIVE = 0,
	JSMN_OBJECT = 1,
	JSMN_ARRAY = 2,
	JSMN_STRING = 3
} jsmntype_t;

typedef enum {
	JSMN_ERROR_NOMEM = -1,
	JSMN_ERROR_INVAL = -2,
	JSMN_ERROR_PART = -3,
	JSMN_SUCCESS = 0
} jsmnerr_t;

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

jsmnerr_t jsmn_parse(const char *text, int length, jsmntok_t **value);

#endif /* __JSMN_H_ */
