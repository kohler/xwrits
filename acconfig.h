/* Process this file with autoheader to produce config.h.in */
#ifndef CONFIG_H
#define CONFIG_H

/* Package and version */
#define PACKAGE "xwrits"
#define VERSION "97"

/* How many arguments does gettimeofday() take? */
#undef GETTIMEOFDAY_PROTO

/* Get the [u_]int*_t typedefs */
#undef NEED_SYS_TYPES_H
#ifdef NEED_SYS_TYPES_H
# include <sys/types.h>
#endif
#undef u_int16_t
#undef u_int32_t
#undef int32_t

@TOP@
@BOTTOM@

#ifdef __cplusplus
extern "C" {
#endif

/* Use the clean-failing malloc library in fmalloc.c */
#include <stddef.h>
#define xmalloc(s)		fail_die_malloc((s),__FILE__,__LINE__)
#define xrealloc(p,s)		fail_die_realloc((p),(s),__FILE__,__LINE__)
#define xfree			free
void *fail_die_malloc(size_t, const char *, int);
void *fail_die_realloc(void *, size_t, const char *, int);

/* Prototype strerror() if we don't have it. */
#ifndef HAVE_STRERROR
char *strerror(int errno);
#endif

#ifdef __cplusplus
}
/* Get rid of inline macro under C++ */
# undef inline
#endif
#endif
