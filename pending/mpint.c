
#include <stdint.h>
#include "intv.hpp"
#include "portab.h"

#include <stdlib.h> // exit
#include <memory.h>
#include <stdbool.h>
typedef uint32_t u32;
typedef  uint8_t u8;
#define max(a, b) ((a) > (b) ? (a) : (b))

#include "assert.h"

#define ALIGN(a,x) ((x)+(a)-1 - ((x)+(a)-1)%(a))
#define BITUSED8(c) \
	((128 & (c)) ? 8 : \
	 ( 64 & (c)) ? 7 : \
	 ( 32 & (c)) ? 6 : \
	 ( 16 & (c)) ? 5 : \
	 (  8 & (c)) ? 4 : \
	 (  4 & (c)) ? 3 : \
	 (  2 & (c)) ? 2 : \
	 (  1 & (c)) ? 1 : 0)
#define mpint_bitused(body_len, body) \
	(BITUSED8((body)[0]) + ((body_len) -1) * 8)

///////////////////////////////////////////////////////////////////////////////
/* readability (with keeping conflict avoided)

Trade-off:
   - function-name searching
 */
#define _modpow mpint_modpow

///////////////////////////////////////////////////////////////////////////////
// public

/* (*1) plus number must be MSB(first byte b7)=0
        because mpint is defined as signed number [RFC4251]
 */

// r = g ^ a mod p
unsigned _modpow (
	  const void *g__, unsigned g_len
	, const void *a__, unsigned a_len
	, const void *p__, unsigned p_len
	,       void *r__, unsigned r_len
	, void (*bits_fn)(void *bits_arg)
	, void *bits_arg
	, unsigned bits_span
)
{
const u8 *g_, *a_, *p_;
	g_ = (const u8 *)g__, a_ = (const u8 *)a__, p_ = (const u8 *)p__;

	while (0 < g_len && 0x00 == *g_) ++g_, --g_len;
	while (0 < a_len && 0x00 == *a_) ++a_, --a_len;
	while (0 < p_len && 0x00 == *p_) ++p_, --p_len;
BUG(0 < g_len && 0 < a_len && 0 < p_len)
	if (! (0 < g_len && 0 < a_len && 0 < p_len))
		return 0;

unsigned g_bitused, a_bitused, p_bitused;
	g_bitused = mpint_bitused(g_len, g_);
	a_bitused = mpint_bitused(a_len, a_);
	p_bitused = mpint_bitused(p_len, p_);
BUG(ALIGN(8, 1 + p_bitused) / 8 <= r_len)
	if (! (ALIGN(8, 1 + p_bitused) / 8 <= r_len)) // (*1)
		return 0;

struct intv *r, *a, *p;
	r = intv_ctor (2 * p_bitused);
	intv_load_u8be (a = intv_ctor (ALIGN(8, a_bitused) +64), a_, a_len);
	intv_load_u8be (p = intv_ctor (ALIGN(8, p_bitused) +64), p_, p_len);

bool result;
	if (g_len < 5) {
u32 g; u8 be32[4];
		memset (be32, 0, sizeof(be32));
		memcpy (be32 +4 -g_len, g_, g_len);
		g = load_be32 (be32);
		result = vmodpow32 (r, g, a, p, bits_fn, bits_arg, bits_span);
	}
	else {
struct intv *g;
		intv_load_u8be (g = intv_ctor (max(ALIGN(8, g_bitused) +64, 2 * p_bitused)), g_, g_len);
		result = vmodpow (r, g, a, p, bits_fn, bits_arg, bits_span);
		intv_dtor (g);
	}
BUG(r->bitused <= p_bitused)

u8 *dst;
		dst = (u8 *)r__;
	if (true == result) {
		if (0 == r->bitused % 8)
			*dst++ = 0x00;
		dst += intv_store_u8be (r, dst);
	}
	intv_dtor (r);
	intv_dtor (p);
	intv_dtor (a);

	return (unsigned)(dst - (u8 *)r__);
}
