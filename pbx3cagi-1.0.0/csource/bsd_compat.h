/*
 * Compatibility header for strlcpy/strlcat.
 * macOS and FreeBSD have them in <string.h>; Linux uses libbsd <bsd/string.h>.
 */
#ifndef BSD_COMPAT_H
#define BSD_COMPAT_H

#if defined(__APPLE__) || defined(__FreeBSD__)
#include <string.h>
/* strlcpy and strlcat are in system string.h on macOS and FreeBSD */
#else
#include <bsd/string.h>
#endif

#endif /* BSD_COMPAT_H */
