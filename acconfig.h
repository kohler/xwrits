/* Process this file with autoheader to produce config.h.in */
#ifndef CONFIG_H
#define CONFIG_H

/* How many arguments does gettimeofday() take? */
#undef GETTIMEOFDAY_PROTO

/* Get the [u]int*_t typedefs */
#undef NEED_SYS_TYPES_H
#ifdef NEED_SYS_TYPES_H
# include <sys/types.h>
#endif
#undef HAVE_INTTYPES_H
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#undef uint16_t
#undef uint32_t
#undef int32_t

@TOP@
@BOTTOM@

/* Define if Xinerama is available. */
#undef HAVE_XINERAMA

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
/* Get rid of a possible inline macro under C++. */
# define inline inline
#endif
#endif
