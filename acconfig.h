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

/* Prototype strerror() if we don't have it. */
#ifndef HAVE_STRERROR
char *strerror(int errno);
#endif

#ifdef __cplusplus
}
#endif
#endif
