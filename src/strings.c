/*
 * Title:			AGON MOS - Additional string functions
 * Author:			Leigh Brown, HeathenUK, and others
 * Created:			24/05/2023
 */

#include "../src_umm_malloc/umm_malloc.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// Alternative to missing strdup() in ZDS libraries
char *mos_strdup(const char *s)
{
	char *d = umm_malloc(strlen(s) + 1); // Allocate memory
	if (d != NULL) {
		strcpy(d, s);		     // Copy the string
	}
	return d;
}

// Alternative to missing strndup() in ZDS libraries
char *mos_strndup(const char *s, size_t n)
{
	size_t len = strnlen(s, n);
	char *d = umm_malloc(len + 1); // Allocate memory for length plus null terminator

	if (d != NULL) {
		strncpy(d, s, len);    // Copy up to len characters
		d[len] = '\0';	       // Null-terminate the string
	}

	return d;
}

size_t strbuf_insert(char *buf, int buf_capacity, const char *src, int insert_loc)
{
	int src_len = strlen(src);
	int dest_tail_len = strlen(buf + insert_loc) + 1;

	int count = MIN(dest_tail_len, buf_capacity - insert_loc - src_len - 1);
	if (count > 0) {
		memmove(buf + insert_loc + src_len,
		    buf + insert_loc, count);
	}
	buf[insert_loc + src_len + count] = 0;

	count = MIN(src_len, buf_capacity - insert_loc - 1);

	if (count > 0) {
		memcpy(buf + insert_loc, src, count);
	}
	return count;
}

/* Assumes buf contains a null-terminated string already.
 * Will not append more than max_chars_to_append.
 * Will always leave `buf` null-terminated */
void strbuf_append(char *buf, int buf_capacity, const char *str_to_append, int max_chars_to_append)
{
	int src_len = strnlen(str_to_append, max_chars_to_append);
	int insert_loc = strnlen(buf, buf_capacity);

	int count = MIN(src_len, buf_capacity - insert_loc - 1);
	if (count > 0) {
		memcpy(buf + insert_loc, str_to_append, count);
	}

	buf[insert_loc + count] = 0;
}

const char *strrchr_pathsep(const char *path)
{
	if (path == NULL) return NULL;
	const char *p = strrchr(path, '/');
	if (p) return p;
	return strrchr(path, '\\');
}
