#ifndef CHAT_UTIL_H
#define CHAT_UTIL_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/* For checking the parameters of printf-like functions, we use
 * GCC's __attribute__ language extension. Define __attribute__ as an empty
 * macro on compilers not understanding it. */
#ifndef __GNUC__
#  define  __attribute__(x)  /* empty */
#endif

#ifdef __cplusplus
extern "C" {
#endif

void setProgName(const char *argv0);

const char *getProgName(void);

void debugEnable(void);

int debugEnabled(void);

void debugDisable(void);

void styleEnable(void);

int styleEnabled(void);

void styleDisable(void);

void normalPrint(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

void debugPrint(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

void infoPrint(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

void errorPrint(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

void errnoPrint(const char *prefixFmt, ...) __attribute__((format(printf, 1, 2)));

void debugHexdump(const void *ptr, size_t n, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

void hexdump(const void *ptr, size_t n, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

void vhexdump(const void *ptr, size_t n, const char *fmt, va_list args);

size_t nameBytesValidate(const char *input, size_t n);

uint64_t ntoh64u(uint64_t network64u);

uint64_t hton64u(uint64_t host64u);

#ifdef __cplusplus
}
#endif

/* If we have defined __attribute__ before, undefine it again. The reasoning
 * behind this is, that after including this header, a module can use
 * constructs that depend on __attribute__ not only to show compiler warnings
 * but to change the actual behavior of the code, for example when packing
 * structs. In that case we do not want to just swallow errors from missing
 * __attribute__ support, as, for example, unpacked structs will cause
 * unexpected runtime behavior. Better fail compiling than have a program
 * that fails during runtime. */
#ifndef __GNUC__
#  undef __attribute__
#endif

#endif
