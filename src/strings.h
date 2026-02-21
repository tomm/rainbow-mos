/*
 * Title:			AGON MOS - Additional string functions
 * Author:			Leigh Brown
 * Created:			24/05/2023
 * Last Updated:	24/05/2023
 *
 * Modinfo:
 */

#ifndef STRINGS_H
#define STRINGS_H

#include <stddef.h> // size_t

// Alternative to missing strdup() in ZDS libraries
char *mos_strdup(const char *s);

// Alternative to missing strndup() in ZDS libraries
char *mos_strndup(const char *s, size_t n);

/* Find a '/' or '\' in path, starting from the end */
const char *strrchr_pathsep(const char *path);

void strbuf_append(char *buf, int buf_capacity, const char *str_to_append, int max_chars_to_append);

/* Returns number of characters inserted */
size_t strbuf_insert(char *buf, int buf_capacity, const char *src, int insert_loc);

#endif // STRINGS_H
