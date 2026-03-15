// npf_config.h — your project's wrapper header. Every source file includes this.
#ifndef NPF_CONFIG_H
#define NPF_CONFIG_H

#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_ALT_FORM_FLAG 1
#define NANOPRINTF_USE_FLOAT_SINGLE_PRECISION 0

#include "nanoprintf.h"

#define ksnprintf npf_snprintf
#define kvsnprintf npf_vsnprintf

void kprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));

#endif
