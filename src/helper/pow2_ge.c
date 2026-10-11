#include "helper/pow2_ge.h"

#include <stdio.h> // printf
#include <stdlib.h> // exit

///////////////////////////////////////////////////////////////
// VT100

#define VTO  "\033[0m"
#define VTRR "\033[1;31m"
#define VTGG "\033[1;32m"
#define VTYY "\033[1;33m"
#define VTBB "\033[1;34m"
#define VTMM "\033[1;35m"
#define VTCC "\033[1;36m"

///////////////////////////////////////////////////////////////
// assertion

#define assert(expr) \
	do { \
		if (! (expr)) { \
printf (VTRR "ASSERT" VTO ": " #expr " [%s]" "\n", __func__); \
			exit (-1); \
		} \
	} while (0);

#define caller_bug assert

///////////////////////////////////////////////////////////////////////////////

unsigned pow2_ge (unsigned n)
{
caller_bug(n <= (1U << 31)) // safety on unsigned == uint32_t
	if (0 == n) return 0;

unsigned r = 1;
	while (r < n)
		r <<= 1;
	return r;
}
