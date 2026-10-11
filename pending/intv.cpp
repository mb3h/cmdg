#include <stdint.h> // uint32_t
#include "intv.hpp"

#include "portab.h"
#include "assert.h"

#include <stdlib.h> // exit
#include <memory.h>
#include <malloc.h>
#include <limits.h> // UINT_MAX
#include <stdbool.h>

#define RUPDIV(d, src) (((src) +(d) -1) / (d))
#define ALIGN(n, src) (RUPDIV(n, src) * (n))

#define max(a,b) ((a) > (b) ? (a) : (b))

#define BITUSED32(msb__, dw) ((dw) ? 1 +( \
	(0x00000002 & (dw) >> (msb__ = \
	(0x0000000c & (dw) >> (msb__ = \
	(0x000000f0 & (dw) >> (msb__ = \
	(0x0000ff00 & (dw) >> (msb__ = \
	(0xffff0000UL & (dw) ? 16 : 0)) \
	? 8 : 0) | msb__) \
	? 4 : 0) | msb__) \
	? 2 : 0) | msb__) \
	? 1 : 0) | msb__) \
	: 0)

typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint64_t u64;

static unsigned u32le_bitused (unsigned bitused, const u32 *v)
{
BUG(v)
	if (! (0 < bitused))
		return 0;
unsigned i, msb;
	for (i = (bitused -1) / 32 +1; 0 < i; --i)
		if (v[i -1])
			return (i -1) * 32 + BITUSED32(msb, v[i -1]);
	return 0;
}

typedef struct intv_ intv_s;
struct intv_ {
	unsigned bitused;
	unsigned bitmax;
	u32 v[1];
};

void intv_security_erase (struct intv *this__)
{
intv_s *this_;
	this_ = (intv_s *)this__;
	memset (this_, 0, offsetof(intv_s, v) + RUPDIV(32, this_->bitmax) * sizeof(this_->v[0])); // security erase
	this_->bitused = 0;
}
void intv_dtor (struct intv *this__)
{
intv_s *this_;
	this_ = (intv_s *)this__;
	this_->bitmax = 0; // security
	free (this_);
}

struct intv *intv_ctor (unsigned bitmax)
{
intv_s *ret;
	if (NULL == (ret = (intv_s *)malloc (offsetof(intv_s, v) + RUPDIV(32, bitmax) * sizeof(ret->v[0]))))
		return NULL;
	ret->bitmax = bitmax;
	ret->bitused = 0;
	return (struct intv *)ret;
}
unsigned intv_store_u8be (const struct intv *this__, void *dst_)
{
const intv_s *this_;
	this_ = (const intv_s *)this__;
unsigned n, i;
	n = (this_->bitused +7) / 32;
	i = (this_->bitused +7) % 32 / 8;
u8 *dst;
	dst = (u8 *)dst_;
	for (; 0 < i; --i)
		*dst++ = 0xff & this_->v[n] >> (i -1) * 8;
	for (; 0 < n; --n, dst += 4)
		store_be32 (dst, this_->v[n -1]);
BUG(dst <= (u8 *)dst_ + UINT_MAX)
	return (unsigned)(dst - (u8 *)dst_);
}
bool intv_load_u8be (struct intv *this__, const void *src_, unsigned cb)
{
intv_s *this_;
	this_ = (intv_s *)this__;
const u8 *src;
	src = (const u8 *)src_;
unsigned used, i, msb;
u32 dw;
	for (dw = 0, i = 0; i < cb % 4; ++i)
		dw = dw << 8 | *(src +i);
	if (0 == (used = BITUSED32(msb, dw)))
		for (; i < cb; i += 4)
			if (used = BITUSED32(msb, load_be32 (src +i)))
				break;
	if (i < cb || dw) {
unsigned n;
		n = (cb -i) / 4;
unsigned bitused;
		if (this_->bitmax < (bitused = (dw ? n : n -1) * 32 + used))
			return false; // buffer overflow
		src += cb;
		for (i = 0; i < n; ++i, src -= 4)
			this_->v[i] = load_be32 (src -4);
		if (dw)
			this_->v[n] = dw;
		this_->bitused = bitused;
	}
	return true;
}

static bool vmov (intv_s *dst, const intv_s *src)
{
	if (dst->bitmax < src->bitused)
		return false;
	dst->bitused = src->bitused;
	memcpy (dst->v, src->v, RUPDIV(32, src->bitused) * sizeof(src->v[0]));
	return true;
}

static int vcmp_u32le (const intv_s *lsrc, const u32 *src_u32le)
{
int i;
	i = RUPDIV(32, lsrc->bitused) -1;
	do {
		if (! (lsrc->v[i] == src_u32le[i]))
			return (lsrc->v[i] < src_u32le[i]) ? -1 : 1;
	} while (0 < i--);
	return 0;
}
static int vcmp (const intv_s *lsrc, const intv_s *src)
{
	if (! (lsrc->bitused == src->bitused))
		return (lsrc->bitused < src->bitused) ? -1 : 1;
	return vcmp_u32le (lsrc, src->v);
}

int u8be_cmp (const void *lhs_u8be, unsigned lhs_cb, const void *rhs_u8be, unsigned rhs_cb)
{
const u8 *lhs, *rhs;
	lhs = (const u8 *)lhs_u8be;
	rhs = (const u8 *)rhs_u8be;
unsigned lhs_bitused, rhs_bitused;
unsigned i, msb;
	for (i = 1; i < lhs_cb && 0 == lhs[i -1];)
		++i;
	lhs_bitused = BITUSED32(msb, lhs[i -1]) + 8 * (lhs_cb - i);
	for (i = 1; i < rhs_cb && 0 == rhs[i -1];)
		++i;
	rhs_bitused = BITUSED32(msb, rhs[i -1]) + 8 * (rhs_cb - i);
	if (! (lhs_bitused == rhs_bitused))
		return (lhs_bitused < rhs_bitused) ? -1 : 1;
	lhs = lhs + lhs_cb - RUPDIV(8, lhs_bitused);
	rhs = rhs + rhs_cb - RUPDIV(8, rhs_bitused);
	while (lhs < (const u8 *)lhs_u8be + lhs_cb)
		if (! (*lhs++ == *rhs++))
			return (lhs[-1] < rhs[-1]) ? -1 : 1;
	return 0;
}

static bool vsub (intv_s *dst, const intv_s *lsrc, const intv_s *src)
{
	if (lsrc) {
ASSERTE(lsrc->bitused <= dst->bitmax)
		vmov (dst, lsrc);
	}
ASSERTE(src->bitused <= dst->bitused)
int n, i;
	n = RUPDIV(32, src->bitused);
int borrow;
	borrow = 0;
	for (i = 0; i < n; ++i) {
		if (borrow) {
			borrow = (dst->v[i] < 1) ? 1 : 0;
			--dst->v[i];
		}
		borrow |= (dst->v[i] < src->v[i]) ? 1 : 0;
		dst->v[i] -= src->v[i];
	}
	n = RUPDIV(32, dst->bitused);
	for (; borrow && i < n; ++i) {
		borrow = (dst->v[i] < 1) ? 1 : 0;
		--dst->v[i];
	}
	if (borrow)
		return false;
	dst->bitused = u32le_bitused (dst->bitused, dst->v);
	return true;
}

#define div64(q, hi, lo, m) \
	__asm__ ("div %1" : "=a" (q) : "r" (m), "d" (hi), "a" (lo))
#define mul64(hi, lo, a, b) \
	__asm__ ("mul %2" : "=d" (hi), "=a" (lo) : "r" (b), "a" (a))

static bool vmul (intv_s *dst, const intv_s *lsrc, const intv_s *src)
{
	if (dst == lsrc || dst == src)
		return false;
	if (dst->bitmax < lsrc->bitused + src->bitused) // check#1
		return false;
	dst->v[0] = 0;
	dst->bitused = 0;
	if (0 == lsrc->bitused || 0 == src->bitused)
		return true;
int m, n;
	m = RUPDIV(32, lsrc->bitused);
	n = RUPDIV(32, src->bitused);
int i, j;
	for (j = 0; j < m; ++j) {
u32 a;
		a = lsrc->v[j];
u64 hl;
		hl = 0;
		for (i = 0; i < n; ++i) {
u32 b;
			b = src->v[i];
u32 tmplo, tmphi;
			mul64 (tmphi, tmplo, a, b);
			hl += (u64)tmphi << 32 | tmplo;
			if (0 < j)
				hl += dst->v[j +i];
			dst->v[j +i] = (u32)hl;
			hl >>= 32;
		}
		if (j < m -1 || hl)
			dst->v[j +i] = hl; // never over buffer because check#1.
	}
	dst->bitused = lsrc->bitused + src->bitused;
	dst->bitused -= (1 << (dst->bitused -1) % 32 & dst->v[(dst->bitused -1) / 32]) ? 0 : 1;
	return true;
}

static u32 vclip32 (const intv_s *a, int lsb)
{
int i;
	i = lsb / 32;
u32 ret;
	ret = a->v[i];
	if (0 == lsb % 32)
		return ret;
	ret >>= lsb % 32;
	if (RUPDIV(32, a->bitused) < i +2)
		return ret;
	ret |= a->v[i +1] << 32 - lsb % 32;
	return ret;
}
static bool vmod (intv_s *r, const intv_s *a, const intv_s *divisor)
{
ASSERTE(max(a->bitused, divisor->bitused) <= r->bitmax)
ASSERTE(61 < divisor->bitused) // To do 63bits(b62=1) / 32bits(b31=1) on div64().
	if (! (r == a))
		vmov (r, a);
unsigned r_bitused_backup = r->bitused;

int msb_d;
	msb_d = divisor->bitused -1;
u32 dhi32;
	dhi32 = vclip32 (divisor, msb_d -31);
int msb_r;
	msb_r = r->bitused -1;
	while (msb_d < msb_r) {
u32 quo, tmplo, tmphi;
		tmplo = vclip32 (r, msb_r -62); // 32bits
		tmphi = vclip32 (r, msb_r -30); // 31bits (*) (31bits):(32bits) / (32bits) <= (32bits)
		div64 (quo, tmphi, tmplo, dhi32);
		quo -= 2; // logic#1 (*)legal because b30 or b31 of quo is always 1.
int lshift;
		if ((lshift = (msb_r - msb_d) -31) < 0)
			quo >>= -lshift, lshift = 0;
int i, end;
		i = lshift / 32;
		end = lshift / 32 + msb_d / 32 +1;
		lshift %= 32;
u64 hl;
		hl = 0;
u32 e;
		e = 0;
const u32 *src;
		src = divisor->v;
		for (; i < end; ++i, ++src) {
u32 tmp;
			tmp = *src;
			mul64 (tmphi, tmplo, quo, tmp);
			hl += (u64)tmphi << 32 | tmplo;
u32 u;
			u = (u32)hl;
			if (lshift)
				u = u << lshift | e;
			if (r->v[i] < u)
				hl += (u64)1 << 32 - lshift;
			r->v[i] -= u;
			if (lshift)
				e = (u32)hl >> 32 - lshift;
			hl >>= 32;
		}
		for (; hl || e; ++i) { // never over borrow because logic#1
u32 u;
			u = (u32)hl;
			if (lshift)
				u = u << lshift | e;
			if (r->v[i] < u)
				hl += (u64)1 << 32 - lshift;
			r->v[i] -= u;
			if (lshift)
				e = (u32)hl >> 32 - lshift;
			hl >>= 32;
		}
		// update msb_r(= r->bitused -1).
		msb_r = u32le_bitused (msb_r +1, r->v) -1;
	}
	r->bitused = msb_r +1;
	if (vcmp (divisor, r) <= 0)
		vsub (r, r, divisor);
	return true;
}

bool vmodpow (
	  struct intv *r_
	, struct intv *g_
	, const struct intv *a_
	, const struct intv *p_
	, void (*bits_fn)(void *bits_arg)
	, void *bits_arg
	, unsigned bits_span
)
{
intv_s *r, *g;
	r = (intv_s *)r_, g = (intv_s *)g_;
const intv_s *a, *p;
	a = (const intv_s *)a_, p = (const intv_s *)p_;
ASSERTE(32 < p->bitused && p->bitused <= r->bitmax && p->bitused * 2 <= g->bitmax)
struct intv *tmp;
	tmp = intv_ctor (2 * p->bitused);

	if (! (vcmp (g, p) < 0)) {
		vmod ((intv_s *)tmp, g, p), vmov (g, (intv_s *)tmp); // PENDING: is recovery needed really ? is caller bug ?
BUG(vcmp (g, p) < 0)
	}
unsigned i;
	for (i = 0; i < a->bitused; ++i, vmul ((intv_s *)tmp, g, g), vmod (g, (intv_s *)tmp, p)) {
		if (bits_fn && 0 < bits_span && 0 == i % bits_span && 0 < i)
			bits_fn (bits_arg);
		if (! (1 << i % 32 & a->v[i / 32]))
			continue;
		if (0 == r->bitused)
			vmov (r, g);
		else
			vmul ((intv_s *)tmp, r, g), vmod (r, (intv_s *)tmp, p);
	}
	intv_security_erase (tmp);
	intv_dtor (tmp);
	return true;
}

bool vmodpow32 (
	  struct intv *r
	, u32 g_
	, const struct intv *a
	, const struct intv *p
	, void (*bits_fn)(void *bits_arg)
	, void *bits_arg
	, unsigned bits_span
)
{
	if (g_ < 2)
		return false;
struct intv *g;
	//g = intv_ctor_u32le (&g_, 32, 2 * p->bitused);
	g = intv_ctor (2 * p->bitused);
u8 tmp[4];
	store_be32 (tmp, g_);
	intv_load_u8be (g, tmp, sizeof(tmp));
bool retval;
	retval = vmodpow (r, g, a, p, bits_fn, bits_arg, bits_span);
	intv_security_erase (g);
	intv_dtor (g);
	return retval;
}

bool intv_h_validate ()
{
ASSERTE(sizeof(struct intv) == sizeof(intv_s))
ASSERTE(offsetof(struct intv, bitused) == offsetof(intv_s, bitused))
ASSERTE(offsetof(struct intv, priv) == offsetof(intv_s, bitmax))
ASSERTE(offsetof(struct intv, v) == offsetof(intv_s, v))
	return sizeof(struct intv) == sizeof(intv_s)
		&& offsetof(struct intv, bitused) == offsetof(intv_s, bitused)
		&& offsetof(struct intv, priv) == offsetof(intv_s, bitmax)
		&& offsetof(struct intv, v) == offsetof(intv_s, v)
		? true : false;
}
