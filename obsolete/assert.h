#ifndef ASSERT_H_INCLUDED__
#define ASSERT_H_INCLUDED__

#include "log.h"
#include "vt100.h" // TODO: removing

#define ASSERTE(expr) \
	if (! (expr)) { \
echo (VTRR "ASSERT" VTO "! " #expr "  [" VTCC "%s" VTO "]" "\n", __func__); \
		exit (-1); \
	}
#define ASSERTEc(expr, fmt, a, b) \
	if (! (expr)) { \
echo (VTRR "ASSERT" VTO "! " #expr fmt "  [" VTCC "%s" VTO "]" "\n", a, b, __func__); \
		exit (-1); \
	}
#define BUG  ASSERTE
#define BUGc ASSERTEc

#ifndef PLATFORM_PAGE_SIZE
# define PLATFORM_PAGE_SIZE 4096
#endif
/* notify unfavorable alloca() calling to assign new memory page
   with high possibility. this cannot detect all, because it is
   caused by less alloca() depending on situation of other var
   declare and of stack pointer (esp/rsp) beginning address. */
#define alloca_chk(var, let_and_type_cast, value, assign) \
	{ \
		if (PLATFORM_PAGE_SIZE < (value assign)) \
/* [reason] unfavorable size stack frame using [ignore] possible [recovery] replace into malloc() */ \
log_ (VTRR "warn" VTO ": alloca(" VTRR "%d" VTO ")" " [" VTCC "%s" VTO "]" "\n" \
, value, __func__); \
		var let_and_type_cast alloca (value); \
	}

#endif // ASSERT_H_INCLUDED__
