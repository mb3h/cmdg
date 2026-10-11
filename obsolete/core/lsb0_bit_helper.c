#include "lsb0_bit_helper.h"

#include <endian.h>
#include <stdint.h>
#include <stddef.h> // size_t

typedef uint32_t u32;
typedef  uint8_t u8;
#define arraycountof(x) (sizeof(x)/(sizeof((x)[0])))
#define bitsizeof(x) (unsigned)(sizeof(x) * 8)
#define HIDIV(x,a) (((x) +(a) -1)/(a))

///////////////////////////////////////////////////////////////////////////////
// recycle

unsigned lsb0_bitrchr (const void *s, int b, unsigned n)
{
	if (! (0 < n))
		return 0; // invalid parameter
#if __BYTE_ORDER == __LITTLE_ENDIAN
const u32 *begin, *p;
	begin = (const u32 *)s, p = begin + HIDIV(n, bitsizeof(p[0]));
unsigned i;
	if (b) {
		if (0 < (i = n % bitsizeof(p[0])) && *--p << bitsizeof(p[0]) - i)
			++p;
		else {
			while (begin < p && 0 == p[-1])
				--p;
			i = bitsizeof(p[0]);
		}
		if (! (begin < p))
			return 0; // not found '1'
		--p;
		while (0 < i)
			if (1 == (1 & *p >> --i))
				break;
	}
	else {
		if (0 < (i = n % bitsizeof(p[0])) && ~*--p << bitsizeof(p[0]) - i)
			++p;
		else {
			while (begin < p && 0 == ~p[-1])
				--p;
			i = bitsizeof(p[0]);
		}
		if (! (begin < p))
			return 0; // not found '0'
		--p;
		while (0 < i)
			if (0 == (1 & *p >> --i))
				break;
	}
	return (unsigned)(p - begin) * bitsizeof(p[0]) + i;
#else // __BYTE_ORDER == __BIG_ENDIAN
	// TODO:
#endif
}

// PENDING: repair alignment (word), avoid overwriting (byte boundary)
// PENDING: speed optimize
void lsb0_bitset (void *s, unsigned ofs, int b, unsigned n)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
u32 *dst;
	dst = (u32 *)s;
unsigned i;
	if (b)
		for (i = ofs; i < ofs + n; ++i)
			dst[i / bitsizeof(dst[0])] |= 1 << i % bitsizeof(dst[0]);
	else
		for (i = ofs; i < ofs + n; ++i)
			dst[i / bitsizeof(dst[0])] &= ~(1 << i % bitsizeof(dst[0]));
#else // __BYTE_ORDER == __BIG_ENDIAN
	// TODO:
#endif
}

// PENDING: speed optimize
void lsb0_bitmove (void *dst_, unsigned n_dst, const void *src_, unsigned n_src, unsigned n)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
u32 *dst; const u32 *src;
	dst = (u32 *)dst_, src = (const u32 *)src_;
// repair alignment (don't depend on 'dst_' and 'src_')
	n_dst += (size_t)dst_ % sizeof(dst[0]) * 8, dst = (      u32 *)((      u8 *)dst_ - (size_t)dst_ % sizeof(dst[0]));
	n_src += (size_t)src_ % sizeof(src[0]) * 8, src = (const u32 *)((const u8 *)src_ - (size_t)src_ % sizeof(src[0]));
// repair big offset
	dst += n_dst / bitsizeof(dst[0]), n_dst -= n_dst / bitsizeof(dst[0]) * bitsizeof(dst[0]);
	src += n_src / bitsizeof(src[0]), n_src -= n_src / bitsizeof(src[0]) * bitsizeof(src[0]);
	if (0 == n || dst == src && n_dst == n_src)
		return;
unsigned i;
	if (dst < src || dst == src && n_dst < n_src)
		for (i = 0; i < n; ++i)
			if (1 & src[(n_src +i) / bitsizeof(src[0])] >> (n_src +i) % bitsizeof(src[0]))
				dst[(n_dst +i) / bitsizeof(dst[0])] |=   1 << (n_dst +i) % bitsizeof(dst[0]);
			else
				dst[(n_dst +i) / bitsizeof(dst[0])] &= ~(1 << (n_dst +i) % bitsizeof(dst[0]));
	else
		for (i = n; 0 < i; --i)
			if (1 & src[(n_src +i -1) / bitsizeof(src[0])] >> (n_src +i -1) % bitsizeof(src[0]))
				dst[(n_dst +i -1) / bitsizeof(dst[0])] |=   1 << (n_dst +i -1) % bitsizeof(dst[0]);
			else
				dst[(n_dst +i -1) / bitsizeof(dst[0])] &= ~(1 << (n_dst +i -1) % bitsizeof(dst[0]));
#else // __BYTE_ORDER == __BIG_ENDIAN
	// TODO:
#endif
}
