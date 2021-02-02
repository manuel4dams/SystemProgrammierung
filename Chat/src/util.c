#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include "util.h"

typedef enum {
    STYLE_NORMAL,
    STYLE_INFO,
    STYLE_ERROR,
    STYLE_DEBUG,
    STYLE_HEXDUMP
} OutputStyle;

typedef union {
    uint64_t uint64;
    unsigned char bytes[sizeof(uint64_t)];
} Uint64Bytes;

static const char *prog_name = "<unknown>";
static int debug_enabled = 0;
static int style_enabled = 1;

static int lockFile(FILE *file) {
    int oldState;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
    flockfile(file);
    return oldState;
}

static void unlockFile(FILE *file, int savedCancelState) {
    funlockfile(file);
    pthread_setcancelstate(savedCancelState, NULL);
}

static void setStyle(FILE *file, OutputStyle style) {
    if (style_enabled && isatty(fileno(file))) {
        switch (style) {
            case STYLE_NORMAL:
                fprintf(file, "\033[0;39;49m");    //reset attributes and foreground color
                break;
            case STYLE_INFO:
                fprintf(file, "\033[1;39;49m");    //bold, default colors
                break;
            case STYLE_ERROR:
                fprintf(file, "\033[1;31;49m");    //bold, red
                break;
            case STYLE_DEBUG:
                fprintf(file, "\033[0;33;49m");    //regular, yellow
                break;
            case STYLE_HEXDUMP:
                fprintf(file, "\033[0;32;49m");    //regular, green
                break;
        }
    }
}

void setProgName(const char *argv0) {
    prog_name = argv0;
}

const char *getProgName(void) {
    return prog_name;
}

void debugEnable(void) {
    debug_enabled = 1;
}

int debugEnabled(void) {
    return debug_enabled;
}

void debugDisable(void) {
    debug_enabled = 0;
}

void styleEnable(void) {
    style_enabled = 1;
}

int styleEnabled(void) {
    return style_enabled;
}

void styleDisable(void) {
    style_enabled = 0;
}

void normalPrint(const char *fmt, ...) {
    va_list args;
    int savedCancelState;

    va_start(args, fmt);
    savedCancelState = lockFile(stderr);
    fprintf(stderr, "%s: ", getProgName());
    vfprintf(stderr, fmt, args);
    putc_unlocked('\n', stderr);
    unlockFile(stderr, savedCancelState);
    va_end(args);
}

void debugPrint(const char *fmt, ...) {
    va_list args;
    int savedCancelState;

    if (debug_enabled) {
        va_start(args, fmt);
        savedCancelState = lockFile(stderr);
        setStyle(stderr, STYLE_DEBUG);
        fprintf(stderr, "%s: ", getProgName());
        vfprintf(stderr, fmt, args);
        putc_unlocked('\n', stderr);
        setStyle(stderr, STYLE_NORMAL);
        unlockFile(stderr, savedCancelState);
        va_end(args);
    }
}

void infoPrint(const char *fmt, ...) {
    va_list args;
    int savedCancelState;

    va_start(args, fmt);
    savedCancelState = lockFile(stderr);
    setStyle(stderr, STYLE_INFO);
    fprintf(stderr, "%s: ", getProgName());
    vfprintf(stderr, fmt, args);
    putc_unlocked('\n', stderr);
    setStyle(stderr, STYLE_NORMAL);
    unlockFile(stderr, savedCancelState);
    va_end(args);
}

void errorPrint(const char *fmt, ...) {
    va_list args;
    int savedCancelState;

    va_start(args, fmt);
    savedCancelState = lockFile(stderr);
    setStyle(stderr, STYLE_ERROR);
    fprintf(stderr, "%s: ", getProgName());
    vfprintf(stderr, fmt, args);
    putc_unlocked('\n', stderr);
    setStyle(stderr, STYLE_NORMAL);
    unlockFile(stderr, savedCancelState);
    va_end(args);
}

void errnoPrint(const char *prefixFmt, ...) {
    va_list args;
    int savedCancelState;
    int savedErrno = errno;

    va_start(args, prefixFmt);
    savedCancelState = lockFile(stderr);
    setStyle(stderr, STYLE_ERROR);
    fprintf(stderr, "%s: ", getProgName());
    vfprintf(stderr, prefixFmt, args);
    fputs(": ", stderr);
    errno = savedErrno;
    perror(NULL);
    setStyle(stderr, STYLE_NORMAL);
    unlockFile(stderr, savedCancelState);
    va_end(args);
}

void debugHexdump(const void *ptr, size_t n, const char *fmt, ...) {
    va_list args;

    if (debug_enabled) {
        va_start(args, fmt);
        vhexdump(ptr, n, fmt, args);
        va_end(args);
    }
}

void hexdump(const void *ptr, size_t n, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vhexdump(ptr, n, fmt, args);
    va_end(args);
}

void vhexdump(const void *ptr, size_t n, const char *fmt, va_list args) {
    const size_t charsPerLine = 16U;
    const size_t fullLines = n / charsPerLine;
    const size_t incompleteLine = n % charsPerLine;
    const unsigned char *array = (const unsigned char *) ptr;
    char byte;
    size_t line;
    size_t column;
    va_list a;
    int savedCancelState;

    savedCancelState = lockFile(stderr);
    setStyle(stderr, STYLE_HEXDUMP);

    //print every complete line
    for (line = 0; line < fullLines; ++line) {
        //program name
        fprintf(stderr, "%s: ", getProgName());

        //prefix
        va_copy(a, args);
        vfprintf(stderr, fmt, a);
        va_end(a);
        fprintf(stderr, ": ");

        //bytes as hex values
        for (column = 0; column < charsPerLine; ++column)
            fprintf(stderr, "%02x ", (unsigned) array[line * charsPerLine + column]);

        //space between hex values and ASCII characters
        fprintf(stderr, "%3s", "");

        //bytes as ASCII characters
        for (column = 0; column < charsPerLine; ++column) {
            byte = array[line * charsPerLine + column];
            fprintf(stderr, "%c", isgraph(byte) ? byte : '.');
        }

        //line ending
        fputc('\n', stderr);
    }

    //print last, incomplete line
    if (incompleteLine) {
        //program name
        fprintf(stderr, "%s: ", getProgName());

        //prefix
        va_copy(a, args);
        vfprintf(stderr, fmt, a);
        va_end(a);
        fprintf(stderr, ": ");

        //bytes as hex values
        for (column = 0; column < incompleteLine; ++column)
            fprintf(stderr, "%02x ", (unsigned) array[line * charsPerLine + column]);

        //fill empty hex value spaces
        while (column++ < charsPerLine)
            fprintf(stderr, "   ");

        //space between hex values and ASCII characters
        fprintf(stderr, "%3s", "");

        //bytes as ASCII characters
        for (column = 0; column < incompleteLine; ++column) {
            byte = array[line * charsPerLine + column];
            fprintf(stderr, "%c", isgraph(byte) ? byte : '.');
        }

        //line ending
        fputc('\n', stderr);
    }

    setStyle(stderr, STYLE_NORMAL);
    unlockFile(stderr, savedCancelState);
}

size_t nameBytesValidate(const char *input, size_t n) {
    const unsigned char *s = (const unsigned char *) input;
    size_t i;

    for (i = 0U; i < n; ++i) {
        //Reject lower control characters and spaces
        if (s[i] < 33)
            return i;

        //Reject quotes: "'`
        if (s[i] == 34 || s[i] == 39 || s[i] == 96)
            return i;

        //Reject DEL and above
        if (s[i] >= 127)
            return i;

        //Everything else is okay
    }

    return i;
}

uint64_t ntoh64u(uint64_t network64u) {
    Uint64Bytes conv = {.uint64 = network64u};
    uint64_t host64u = 0U;

    for (size_t i = 0U; i < sizeof(conv.bytes); ++i) {
        host64u <<= 8U;
        host64u |= conv.bytes[i];
    }

    return host64u;
}

uint64_t hton64u(uint64_t host64u) {
    Uint64Bytes conv = {.uint64 = 0U};

    unsigned char *p = conv.bytes + sizeof(conv.bytes);
    for (size_t i = 0U; i < sizeof(conv.bytes); ++i)
        *--p = (host64u & ((uint64_t) 0xffU << (i * 8U))) >> (i * 8U);

    return conv.uint64;
}
