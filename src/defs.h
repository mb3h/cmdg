#ifndef DEFS_H_INCLUDED__
#define DEFS_H_INCLUDED__

#include <stdint.h> // uintNN_t
#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# define bool int
# define false 0
# define true 1
#endif
#include <stddef.h> // size_t
#define unixerror strerror

#ifndef min
# define min(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
# define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#define ASSERTE(expr) \
	do { \
		if (! (expr)) { \
fprintf (stderr_old, "\033[1;31m" "ASSERT" "\033[0m" "! '%s'" "\n", #expr); \
			exit (-1); \
		} \
	} while (0);

#endif // DEFS_H_INCLUDED__
