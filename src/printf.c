// npf_config.c — one source file compiles the implementation.
#define NANOPRINTF_IMPLEMENTATION
#include "printf.h"
#include <stdarg.h>

extern int putch(int);

static void putchar_wrapper(int c, void *userdata)
{
  putch(c);
}

void kprintf(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
  npf_vpprintf(&putchar_wrapper, NULL, format, ap);
}
